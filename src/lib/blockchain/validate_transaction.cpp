/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 * Copyright (c) 2016-2017 metaverse core developers (see MVS-AUTHORS)
 *
 * This file is part of metaverse.
 *
 * metaverse is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <metaverse/blockchain/validate_transaction.hpp>
#include <metaverse/bitcoin/chain/script/operation.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <metaverse/bitcoin.hpp>
#include <metaverse/blockchain/transaction_pool.hpp>
#include <metaverse/consensus/miner.hpp>

#ifdef WITH_CONSENSUS
#include <metaverse/consensus.hpp>
#endif

namespace libbitcoin {
namespace blockchain {

static BC_CONSTEXPR unsigned int min_tx_fee = 10000;

using namespace chain;
using namespace std::placeholders;

// Max transaction size is set to max block size (1,000,000).
static constexpr uint32_t max_transaction_size = 1000000;

validate_transaction::validate_transaction(block_chain& chain,
    transaction_ptr tx, const transaction_pool& pool,
    dispatcher& dispatch)
  : blockchain_(chain),
    tx_(tx),
    pool_(pool),
    dispatch_(dispatch),
    tx_hash_(tx->hash())
{
}

validate_transaction::validate_transaction(block_chain& chain,
    const chain::transaction& tx, const transaction_pool& pool,
    dispatcher& dispatch)
  : validate_transaction(chain,
        std::make_shared<message::transaction_message>(tx), pool, dispatch)
{
}

void validate_transaction::start(validate_handler handler)
{
    handle_validate_ = handler;
    const auto ec = basic_checks(static_cast<blockchain::block_chain_impl&>(this->blockchain_));

    if (ec)
    {
        handle_validate_(ec, tx_, {});
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: change to fetch_unspent_transaction, spent dups ok (BIP30).
    ///////////////////////////////////////////////////////////////////////////
    // Check for duplicates in the blockchain.
    blockchain_.fetch_transaction(tx_hash_,
        dispatch_.unordered_delegate(
            &validate_transaction::handle_duplicate_check,
                shared_from_this(), _1));
}

code validate_transaction::basic_checks(blockchain::block_chain_impl& chain) const
{
    const auto ec = check_transaction(*tx_, chain);

    if (ec)
        return ec;

    // This should probably preceed check_transaction.
    if (tx_->is_coinbase())
        return error::coinbase_transaction;

    // Ummm...
    //if ((int64)nLockTime > INT_MAX)

    if (!is_standard())
        return error::is_not_standard;

    if (pool_.is_in_pool(tx_hash_))
        return error::duplicate;

    // Check for blockchain duplicates in start (after this returns).
    return error::success;
}

bool validate_transaction::is_standard() const
{
    return true;
}

void validate_transaction::handle_duplicate_check(
    const code& ec)
{
    if (ec != (code)error::not_found)
    {
        ///////////////////////////////////////////////////////////////////////
        // BUGBUG: overly restrictive, spent dups ok (BIP30).
        ///////////////////////////////////////////////////////////////////////
        handle_validate_(error::duplicate, tx_, {});
        return;
    }

    // TODO: we may want to allow spent-in-pool (RBF).
    if (pool_.is_spent_in_pool(tx_))
    {
        handle_validate_(error::double_spend, tx_, {});
        return;
    }

    // Check inputs, we already know it is not a coinbase tx.
    blockchain_.fetch_last_height(
        dispatch_.unordered_delegate(&validate_transaction::set_last_height,
            shared_from_this(), _1, _2));
}

void validate_transaction::set_last_height(const code& ec,
    size_t last_height)
{
    if (ec)
    {
        handle_validate_(ec, tx_, {});
        return;
    }

    // Used for checking coinbase maturity
    last_block_height_ = last_height;
    current_input_ = 0;
    value_in_ = 0;
	asset_amount_in_ = 0;
	old_symbol_in_ = "";
	new_symbol_in_ = "";
	business_tp_in_ = 0;

    // Begin looping through the inputs, fetching the previous tx.
    if (!tx_->inputs.empty())
        next_previous_transaction();
}

void validate_transaction::next_previous_transaction()
{
    BITCOIN_ASSERT(current_input_ < tx_->inputs.size());

    // First we fetch the parent block height for a transaction.
    // Needed for checking the coinbase maturity.
    blockchain_.fetch_transaction_index(
        tx_->inputs[current_input_].previous_output.hash,
            dispatch_.unordered_delegate(
                &validate_transaction::previous_tx_index,
                    shared_from_this(), _1, _2));
}

void validate_transaction::previous_tx_index(const code& ec,
    size_t parent_height)
{
    if (ec)
    {
        search_pool_previous_tx();
        return;
    }

    BITCOIN_ASSERT(current_input_ < tx_->inputs.size());
    const auto& prev_tx_hash = tx_->inputs[current_input_].previous_output.hash;
    
    // Now fetch actual transaction body
    blockchain_.fetch_transaction(prev_tx_hash,
        dispatch_.unordered_delegate(&validate_transaction::handle_previous_tx,
            shared_from_this(), _1, _2, parent_height));
}

void validate_transaction::search_pool_previous_tx()
{
    transaction previous_tx;
    const auto& current_input = tx_->inputs[current_input_];

    if (!pool_.find(previous_tx, current_input.previous_output.hash))
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::input_not_found, tx_, list);
        return;
    }

    // parent_height ignored here as mempool transactions cannot be coinbase.
    BITCOIN_ASSERT(!previous_tx.is_coinbase());
    static constexpr size_t parent_height = 0;
    handle_previous_tx(error::success, previous_tx, parent_height);
    unconfirmed_.push_back(current_input_);
}

void validate_transaction::handle_previous_tx(const code& ec,
    const transaction& previous_tx, size_t parent_height)
{
    if (ec)
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::input_not_found, tx_, list);
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // HACK: this assumes that the mempool is operating at min block version 4.
    ///////////////////////////////////////////////////////////////////////////

    // Should check if inputs are standard here...
    if (!connect_input(*tx_, current_input_, previous_tx, parent_height,
        last_block_height_, value_in_, script_context::all_enabled, asset_amount_in_, old_symbol_in_,
    new_symbol_in_, business_tp_in_))
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::validate_inputs_failed, tx_, list);
        return;
    }

    // Search for double spends...
    blockchain_.fetch_spend(tx_->inputs[current_input_].previous_output,
        dispatch_.unordered_delegate(&validate_transaction::check_double_spend,
            shared_from_this(), _1, _2));
}

void validate_transaction::check_double_spend(const code& ec, 
    const chain::input_point&)
{
    if (ec != (code)error::unspent_output)
    {
        handle_validate_(error::double_spend, tx_, {});
        return;
    }

    // End of connect_input checks.
    ++current_input_;
    if (current_input_ < tx_->inputs.size())
    {
        next_previous_transaction();
        return;
    }

    // current_input_ will be invalid on last pass.
    check_fees();
}

void validate_transaction::check_fees()
{
    uint64_t fee = 0;

    if (!tally_fees(*tx_, value_in_, fee))
    {
        handle_validate_(error::fees_out_of_range, tx_, {});
        return;
    }
	
	if(((business_tp_in_== ASSET_DETAIL_TYPE) && tx_->has_asset_transfer())
		|| ((business_tp_in_== ASSET_TRANSFERABLE_TYPE) && tx_->has_asset_transfer())) {
	    if (!check_asset_amount(*tx_))
	    {
	        handle_validate_(error::asset_amount_not_equal, tx_, {});
	        return;
	    }
		
	    if (!check_asset_symbol(*tx_))
	    {
	        handle_validate_(error::asset_symbol_not_match, tx_, {});
	        return;
	    }
	}
    // Who cares?
    // Fuck the police
    // Every tx equal!
    handle_validate_(error::success, tx_, unconfirmed_);
}

code validate_transaction::check_transaction(const transaction& tx, blockchain::block_chain_impl& chain)
{
    auto ret = check_transaction_basic(tx, chain);
    if(ret == error::success) {
        for(auto& output : const_cast<transaction&>(tx).outputs){
            if(output.is_asset_issue()) {
                if(chain.is_asset_exist(output.get_asset_symbol(), false)) {
                    return error::asset_exist;
                }
            }
        }
    }
    return ret;
}

code validate_transaction::check_transaction_basic(const transaction& tx, blockchain::block_chain_impl& chain)
{
    if(tx.version >= transaction_version::max_version){
        return error::transaction_version_error;
    } else if (tx.version >= transaction_version::check_output_script) {
        for(auto& i : tx.outputs){
            if(i.script.pattern() == script_pattern::non_standard) {
                return error::script_not_standard;
            }
        }
    }

    if (tx.inputs.empty() || tx.outputs.empty())
        return error::empty_transaction;

    if (tx.serialized_size() > max_transaction_size)
        return error::size_limits;

    // Check for negative or overflow output values
    uint64_t total_output_value = 0;

    for (const auto& output: tx.outputs)
    {
        if (output.value > max_money())
            return error::output_value_overflow;

        total_output_value += output.value;

        if (total_output_value > max_money())
            return error::output_value_overflow;
    }

    for(auto& output : const_cast<transaction&>(tx).outputs){
        if(output.is_asset_issue()) {
            if(!chain::output::is_valid_symbol(output.get_asset_symbol())) {
               return error::asset_symbol_invalid;
            }
        }
    }

    if (tx.is_coinbase())
    {
        const auto& coinbase_script = tx.inputs[0].script;
        const auto coinbase_size = coinbase_script.serialized_size(false);
        if (coinbase_size < 2 || coinbase_size > 100)
            return error::invalid_coinbase_script_size;
    }
    else
    {
        for (const auto& input: tx.inputs)
        {
            if (input.previous_output.is_null())
                return error::previous_output_null;

            if(chain::operation::is_sign_key_hash_with_lock_height_pattern(input.script.operations)){
                uint64_t prev_output_blockheight = 0;
                chain::transaction prev_tx;
                uint64_t current_blockheight = 0;

                chain.get_last_height(current_blockheight);
                chain.get_height(prev_output_blockheight, input.previous_output.hash);
                if(chain.get_transaction(prev_tx, prev_output_blockheight, input.previous_output.hash) == false){
                    return error::input_not_found;
                }

                uint64_t lock_height = chain::operation::get_lock_height_from_sign_key_hash_with_lock_height(input.script.operations);
                if(lock_height > current_blockheight - prev_output_blockheight){
                    return error::invalid_input_script_lock_height;
                }
            }
        }

        for(auto& output : tx.outputs) 
        {
            if(chain::operation::is_pay_key_hash_with_lock_height_pattern(output.script.operations)) {
                uint64_t lock_height = chain::operation::get_lock_height_from_pay_key_hash_with_lock_height(output.script.operations);
                if((int)lock_height < 0 
                    || consensus::miner::get_lock_heights_index(lock_height) < 0){
                    return error::invalid_output_script_lock_height;
                }
            }
        }
    }

    return error::success;
}

// Validate script consensus conformance based on flags provided.
bool validate_transaction::check_consensus(const script& prevout_script,
    const transaction& current_tx, size_t input_index, uint32_t flags)
{
    BITCOIN_ASSERT(input_index <= max_uint32);
    BITCOIN_ASSERT(input_index < current_tx.inputs.size());
    const auto input_index32 = static_cast<uint32_t>(input_index);

#ifdef WITH_CONSENSUS
    using namespace bc::consensus;
    const auto previous_output_script = prevout_script.to_data(false);
    data_chunk current_transaction = current_tx.to_data();

    // Convert native flags to libbitcoin-consensus flags.
    uint32_t consensus_flags = verify_flags_none;

    if ((flags & script_context::bip16_enabled) != 0)
        consensus_flags |= verify_flags_p2sh;

    if ((flags & script_context::bip65_enabled) != 0)
        consensus_flags |= verify_flags_checklocktimeverify;

    if ((flags & script_context::bip66_enabled) != 0)
        consensus_flags |= verify_flags_dersig;

    const auto result = verify_script(current_transaction.data(),
        current_transaction.size(), previous_output_script.data(),
        previous_output_script.size(), input_index32, consensus_flags);

    const auto valid = (result == verify_result::verify_result_eval_true);
#else
    // Copy the const prevout script so it can be run.
    auto previous_output_script = prevout_script;
    const auto& current_input_script = current_tx.inputs[input_index].script;

    const auto valid = script::verify(current_input_script,
        previous_output_script, current_tx, input_index32, flags);
#endif

    if (!valid)
        log::warning(LOG_BLOCKCHAIN)
            << "Invalid transaction ["
            << encode_hash(current_tx.hash()) << "]";

    return valid;
}

bool validate_transaction::connect_input(const transaction& tx,
    size_t current_input, const transaction& previous_tx,
    size_t parent_height, size_t last_block_height, uint64_t& value_in,
    uint32_t flags, uint64_t& asset_amount_in, std::string& old_symbol_in,
    std::string& new_symbol_in, uint32_t& business_tp_in)
{
    const auto& input = tx.inputs[current_input];
    const auto& previous_outpoint = tx.inputs[current_input].previous_output;

    if (previous_outpoint.index >= previous_tx.outputs.size())
        return false;

    const auto& previous_output = previous_tx.outputs[previous_outpoint.index];
    const auto output_value = previous_output.value;
	if (output_value > max_money())
		return false;

	uint64_t asset_transfer_amount = 0;
	if(previous_output.attach_data.get_type() == ASSET_TYPE) {
		// 1. do asset transfer amount check
		asset_transfer_amount = const_cast<output&>(previous_output).get_asset_amount();
		
		// 2. do asset symbol check
		new_symbol_in = const_cast<output&>(previous_output).get_asset_symbol();
		if(!new_symbol_in.empty()) { // asset input
			if(old_symbol_in.empty()) { // init old symbol
				old_symbol_in = new_symbol_in;
			} else {
				if(0 != old_symbol_in.compare(new_symbol_in)) // there are different asset symbol in this transaction
					return false;
			}
		}
		// 3. set business type
		if(const_cast<output&>(previous_output).is_asset_issue())
			business_tp_in = ASSET_DETAIL_TYPE;
		if(const_cast<output&>(previous_output).is_asset_transfer())
			business_tp_in = ASSET_TRANSFERABLE_TYPE;
	}

    if (previous_tx.is_coinbase())
    {
        const auto height_difference = last_block_height - parent_height;

        if (height_difference < coinbase_maturity)
            return false;
    }

    if (!check_consensus(previous_output.script, tx, current_input, flags))
        return false;

    value_in += output_value;
	asset_amount_in += asset_transfer_amount;
    return value_in <= max_money();
}

bool validate_transaction::tally_fees(const transaction& tx, uint64_t value_in,
    uint64_t& total_fees)
{
    const auto value_out = tx.total_output_value();

    if (value_in < value_out)
        return false;

    const auto fee = value_in - value_out;
    if(fee < min_tx_fee)
        return false;
    total_fees += fee;
    return total_fees <= max_money();
}

bool validate_transaction::check_asset_amount(const transaction& tx)
{
    const auto asset_amount_out = tx.total_output_transfer_amount();
    if (asset_amount_in_ != asset_amount_out) // asset amount must be equal
        return false;
	
    return true;
}
bool validate_transaction::check_asset_symbol(const transaction& tx)
{
	// check asset symbol in out
	std::string old_symbol = "";
	std::string new_symbol = "";
	for (auto elem: tx.outputs) {
		new_symbol = elem.get_asset_symbol();
		if(!new_symbol.empty()) {
			if(old_symbol.empty()) {
				old_symbol = new_symbol;
			} else {
				if(0 != old_symbol.compare(new_symbol))
					return false; // different asset in outputs
			}
		}
	}
	if(0 != old_symbol.compare(old_symbol_in_)) // symbol in input and output not match
		return false;
	return true;
}

} // namespace blockchain
} // namespace libbitcoin
