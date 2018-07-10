/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 * Copyright (c) 2016-2018 metaverse core developers (see MVS-AUTHORS)
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
#include <metaverse/blockchain/validate_block.hpp>
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
    const chain::transaction& tx, const validate_block& validate_block)
    : blockchain_(static_cast<blockchain::block_chain_impl&>(chain)),
      tx_(std::make_shared<message::transaction_message>(tx)),
      pool_(nullptr),
      dispatch_(nullptr),
      validate_block_(&validate_block),
      tx_hash_(tx.hash())
{
}

validate_transaction::validate_transaction(block_chain& chain,
    const chain::transaction& tx, const transaction_pool& pool, dispatcher& dispatch)
    : blockchain_(static_cast<blockchain::block_chain_impl&>(chain)),
      tx_(std::make_shared<message::transaction_message>(tx)),
      pool_(&pool),
      dispatch_(&dispatch),
      validate_block_(nullptr),
      tx_hash_(tx.hash())
{
}

void validate_transaction::start(validate_handler handler)
{
    BITCOIN_ASSERT(tx_ && pool_ && dispatch_);

    handle_validate_ = handler;
    const auto ec = basic_checks();

    if (ec) {
        if (ec == error::input_not_found) {
            handle_validate_(ec, tx_, {current_input_});
            return;
        }

        handle_validate_(ec, tx_, {});
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: change to fetch_unspent_transaction, spent dups ok (BIP30).
    ///////////////////////////////////////////////////////////////////////////
    // Check for duplicates in the blockchain.
    blockchain_.fetch_transaction(tx_hash_,
                                  dispatch_->unordered_delegate(
                                      &validate_transaction::handle_duplicate_check,
                                      shared_from_this(), _1));
}

code validate_transaction::basic_checks() const
{
    const auto ec = check_transaction();

    if (ec)
        return ec;

    // This should probably preceed check_transaction.
    if (tx_->is_coinbase())
        return error::coinbase_transaction;

    // Ummm...
    //if ((int64)nLockTime > INT_MAX)

    if (!is_standard())
        return error::is_not_standard;

    if (pool_->is_in_pool(tx_hash_))
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
    if (pool_->is_spent_in_pool(tx_))
    {
        handle_validate_(error::double_spend, tx_, {});
        return;
    }

    // Check inputs, we already know it is not a coinbase tx.
    blockchain_.fetch_last_height(
        dispatch_->unordered_delegate(&validate_transaction::set_last_height,
                                     shared_from_this(), _1, _2));
}

void validate_transaction::reset(size_t last_height)
{
    // Used for checking coinbase maturity
    last_block_height_ = last_height;
    current_input_ = 0;
    value_in_ = 0;
    asset_amount_in_ = 0;
    asset_certs_in_.clear();
    old_symbol_in_ = "";
    old_cert_symbol_in_ = "";
}

void validate_transaction::set_last_height(const code& ec,
        size_t last_height)
{
    if (ec)
    {
        handle_validate_(ec, tx_, {});
        return;
    }

    reset(last_height);

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
        dispatch_->unordered_delegate(
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
                                  dispatch_->unordered_delegate(&validate_transaction::handle_previous_tx,
                                          shared_from_this(), _1, _2, parent_height));
}

bool validate_transaction::get_previous_tx(chain::transaction& prev_tx,
    uint64_t& prev_height, const chain::input& input) const
{
    prev_height = 0;
    if (pool_) {
        if (blockchain_.get_transaction(prev_tx, prev_height, input.previous_output.hash)) {
            return true; // find in block chain
        }
        if (pool_->find(prev_tx, input.previous_output.hash)) {
            return true; // find in memory pool
        }
    }
    else {
        size_t temp_height = 0;
        if (validate_block_ &&
            validate_block_->get_transaction(input.previous_output.hash, prev_tx, temp_height)) {
            prev_height = temp_height;
            return true; // find in block chain or orphan pool
        }
    }
    return false; // failed
}

void validate_transaction::search_pool_previous_tx()
{
    transaction previous_tx;
    const auto& current_input = tx_->inputs[current_input_];

    if (!pool_->find(previous_tx, current_input.previous_output.hash))
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
    if (!connect_input(previous_tx, parent_height))
    {
        const auto list = point::indexes{ current_input_ };
        handle_validate_(error::validate_inputs_failed, tx_, list);
        return;
    }

    // Search for double spends...
    blockchain_.fetch_spend(tx_->inputs[current_input_].previous_output,
                            dispatch_->unordered_delegate(&validate_transaction::check_double_spend,
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

void validate_transaction::check_fees() const
{
    code ec = check_tx_connect_input();
    if (ec != error::success) {
        handle_validate_(ec, tx_, {});
        return;
    }

    // Who cares?
    // Fuck the police
    // Every tx equal!
    handle_validate_(error::success, tx_, unconfirmed_);
}

code validate_transaction::check_tx_connect_input() const
{
    uint64_t fee = 0;

    if (!tally_fees(*tx_, value_in_, fee)) {
        return error::fees_out_of_range;
    }

    if (tx_->has_asset_transfer()) {
        if (!check_asset_amount(*tx_)) {
            return error::asset_amount_not_equal;
        }
        if (!check_asset_symbol(*tx_)) {
            return error::asset_symbol_not_match;
        }
    }

    if (tx_->has_asset_cert()) {
        if (!check_asset_certs(*tx_)) {
            log::debug(LOG_BLOCKCHAIN) << "failed to check asset cert." << tx_->to_string(1);
            return error::asset_cert_error;
        }
    }

    if (tx_->has_asset_mit_transfer()) {
        if (!check_asset_mit(*tx_)) {
            log::debug(LOG_BLOCKCHAIN) << "failed to check MIT token." << tx_->to_string(1);
            return error::mit_error;
        }
    }

    if (tx_->has_did_transfer()) {
        if (!check_did_symbol_match(*tx_)) {
            return error::did_symbol_not_match;
        }
    }

    return error::success;
}

static bool check_same(std::string& dest, const std::string& src)
{
    if (dest.empty()) {
        dest = src;
    } else if (dest != src) {
        log::debug(LOG_BLOCKCHAIN) << "check_same: " << dest << " != " << src;
        return false;
    }
    return true;
}

code validate_transaction::check_secondaryissue_transaction() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& blockchain = blockchain_;

    bool is_asset_secondaryissue{false};
    for (auto& output : tx.outputs) {
        if (output.is_asset_secondaryissue()) {
            is_asset_secondaryissue = true;
            break;
        }
    }
    if (!is_asset_secondaryissue) {
        return error::success;
    }

    is_asset_secondaryissue = false;
    std::string asset_symbol;
    std::string asset_address;
    std::string asset_cert_owner;
    uint8_t secondaryissue_threshold{0};
    uint64_t secondaryissue_asset_amount{0};
    uint64_t asset_transfer_volume{0};
    int num_asset_secondaryissue{0};
    int num_asset_transfer{0};
    int num_asset_cert{0};
    std::vector<asset_cert_type> certs_out;
    for (auto& output : tx.outputs)
    {
        if (output.is_asset_secondaryissue())
        {
            ++num_asset_secondaryissue;
            if (num_asset_secondaryissue > 1) {
                log::debug(LOG_BLOCKCHAIN) << "secondaryissue: num of secondaryissue output > 1, " << asset_symbol;
                return error::asset_secondaryissue_error;
            }

            auto&& asset_detail = output.get_asset_detail();
            if (!asset_detail.is_asset_secondaryissue()
                    || !asset_detail.is_secondaryissue_threshold_value_ok()) {
                log::debug(LOG_BLOCKCHAIN) << "secondaryissue: threshold value invalid, " << asset_symbol;
                return error::asset_secondaryissue_threshold_invalid;
            }
            if (!check_same(asset_symbol, asset_detail.get_symbol())) {
                return error::asset_secondaryissue_error;
            }
            if (!check_same(asset_address, asset_detail.get_address())) {
                return error::asset_secondaryissue_error;
            }
            if (operation::is_pay_key_hash_with_attenuation_model_pattern(output.script.operations)) {
                const auto& model_param = output.get_attenuation_model_param();
                if (!attenuation_model::check_model_param(
                            model_param, asset_detail.get_maximum_supply())) {
                    log::debug(LOG_BLOCKCHAIN) << "secondaryissue: model param invalid, "
                                               << asset_symbol << " " << model_param;
                    return error::attenuation_model_param_error;
                }
            }
            secondaryissue_threshold = asset_detail.get_secondaryissue_threshold();
            secondaryissue_asset_amount = asset_detail.get_maximum_supply();
        }
        else if (output.is_asset_transfer())
        {
            ++num_asset_transfer;
            auto&& asset_transfer = output.get_asset_transfer();
            if (!check_same(asset_symbol, asset_transfer.get_symbol())) {
                return error::asset_secondaryissue_error;
            }
            if (!check_same(asset_address, output.get_script_address())) {
                return error::asset_secondaryissue_error;
            }
            asset_transfer_volume += asset_transfer.get_quantity();
        }
        else if (output.is_asset_cert())
        {
            ++num_asset_cert;
            if (num_asset_cert > 1) {
                log::debug(LOG_BLOCKCHAIN) << "secondaryissue: cert numbers > 1, " << asset_symbol;
                return error::asset_secondaryissue_error;
            }
            auto&& asset_cert = output.get_asset_cert();
            auto cur_cert_type = asset_cert.get_type();
            if (cur_cert_type == asset_cert_ns::issue) {
                if (!check_same(asset_symbol, asset_cert.get_symbol())) {
                    return error::asset_secondaryissue_error;
                }
                if (!check_same(asset_cert_owner, asset_cert.get_owner())) {
                    return error::asset_secondaryissue_error;
                }
                certs_out.push_back(cur_cert_type);
            }
            else {
                log::debug(LOG_BLOCKCHAIN) << "secondaryissue: invalid output of cert "
                                           << asset_cert.to_string();
                return error::asset_secondaryissue_error;
            }
        }
        else if (!output.is_etp() && !output.is_message())
        {
            log::debug(LOG_BLOCKCHAIN) << "secondaryissue: illega output, "
                                       << asset_symbol << " : " << output.to_string(1);
            return error::asset_secondaryissue_error;
        }
    }

    if (tx.version >= transaction_version::check_nova_feature
        && !asset_cert::test_certs(certs_out, asset_cert_ns::issue)) {
        log::debug(LOG_BLOCKCHAIN) << "secondaryissue: no issue asset cert, " << asset_symbol;
        return error::asset_cert_error;
    }

    auto total_volume = blockchain.get_asset_volume(asset_symbol);
    if (total_volume > max_uint64 - secondaryissue_asset_amount) {
        log::debug(LOG_BLOCKCHAIN)
                << "secondaryissue: total asset volume cannot exceed maximum value, "
                << asset_symbol;
        return error::asset_secondaryissue_error;
    }

    if (!asset_detail::is_secondaryissue_owns_enough(asset_transfer_volume, total_volume, secondaryissue_threshold)) {
        log::debug(LOG_BLOCKCHAIN) << "secondaryissue: no enough asset volume, " << asset_symbol;
        return error::asset_secondaryissue_share_not_enough;
    }

    // check inputs asset address
    for (const auto& input : tx.inputs) {
        chain::transaction prev_tx;
        uint64_t prev_height{0};
        if (!get_previous_tx(prev_tx, prev_height, input)) {
            log::debug(LOG_BLOCKCHAIN) << "secondaryissue: input not found: "
                                       << encode_hash(input.previous_output.hash);
            return error::input_not_found;
        }
        auto prev_output = prev_tx.outputs.at(input.previous_output.index);
        if (prev_output.is_asset() || prev_output.is_asset_cert()) {
            auto&& asset_address_in = prev_output.get_script_address();
            if (prev_output.is_asset_cert()) {
                auto&& prev_asset_cert = prev_output.get_asset_cert();
                if (prev_asset_cert.get_symbol() != asset_symbol
                    || prev_asset_cert.get_type() != asset_cert_ns::issue) {
                    log::debug(LOG_BLOCKCHAIN) << "secondaryissue: invalid cert input, " << asset_symbol;
                    return error::validate_inputs_failed;
                }
            } else if (asset_address != asset_address_in) {
                log::debug(LOG_BLOCKCHAIN) << "secondaryissue: invalid asset input, " << asset_symbol;
                return error::validate_inputs_failed;
            }
        }
    }

    return error::success;
}

code validate_transaction::check_asset_issue_transaction() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    bool is_asset_issue{false};
    for (auto& output : tx.outputs) {
        if (output.is_asset_issue()) {
            is_asset_issue = true;
            break;
        }
    }
    if (!is_asset_issue) {
        return error::success;
    }

    is_asset_issue = false;
    int num_cert_issue{0};
    int num_cert_domain_or_naming{0};
    std::vector<asset_cert_type> cert_mask;
    std::vector<asset_cert_type> cert_type;
    std::string asset_symbol;
    std::string asset_address;
    std::string cert_owner;
    for (auto& output : tx.outputs)
    {
        if (output.is_asset_issue())
        {
            if (is_asset_issue) {
                // can not issue multiple assets at the same transaction
                return error::asset_issue_error;
            }
            is_asset_issue = true;
            asset_detail&& detail = output.get_asset_detail();
            if (!detail.is_secondaryissue_threshold_value_ok()) {
                return error::asset_secondaryissue_threshold_invalid;
            }
            if (!check_same(asset_symbol, detail.get_symbol())) {
                return error::asset_issue_error;
            }
            if (!check_same(asset_address, detail.get_address())) {
                return error::asset_issue_error;
            }
            if (chain.is_asset_exist(asset_symbol, false)) {
                return error::asset_exist;
            }
            if (operation::is_pay_key_hash_with_attenuation_model_pattern(output.script.operations)) {
                const auto& model_param = output.get_attenuation_model_param();
                if (!attenuation_model::check_model_param(
                            model_param, detail.get_maximum_supply())) {
                    log::debug(LOG_BLOCKCHAIN) << "issue: model param invalid, "
                                               << asset_symbol << " " << model_param;
                    return error::attenuation_model_param_error;
                }
            }
            cert_mask = detail.get_asset_cert_mask();
        }
        else if (output.is_asset_cert()) {
            asset_cert&& cert_info = output.get_asset_cert();

            // check cert
            asset_cert_type cur_cert_type = cert_info.get_type();
            if (cur_cert_type == asset_cert_ns::issue) {
                ++num_cert_issue;
                if (num_cert_issue > 1) {
                    return error::asset_issue_error;
                }

                if (!check_same(asset_symbol, cert_info.get_symbol())) {
                    return error::asset_issue_error;
                }

                if (!check_same(asset_address, output.get_script_address())) {
                    return error::asset_issue_error;
                }
            }
            else if (cur_cert_type == asset_cert_ns::domain) {
                ++num_cert_domain_or_naming;
                if (num_cert_domain_or_naming > 1) {
                    return error::asset_issue_error;
                }

                if (!asset_symbol.empty()) {
                    auto&& domain = asset_cert::get_domain(asset_symbol);
                    if (domain != cert_info.get_symbol()) {
                        return error::asset_issue_error;
                    }
                }

                if (!check_same(cert_owner, cert_info.get_owner())) {
                    return error::asset_issue_error;
                }
            }
            else if (cur_cert_type == asset_cert_ns::naming) {
                ++num_cert_domain_or_naming;
                if (num_cert_domain_or_naming > 1) {
                    return error::asset_issue_error;
                }

                if (!check_same(asset_symbol, cert_info.get_symbol())) {
                    return error::asset_issue_error;
                }

                if (!check_same(cert_owner, cert_info.get_owner())) {
                    return error::asset_issue_error;
                }
            }
            else {
                log::debug(LOG_BLOCKCHAIN) << "issue: invalid output of cert "
                                           <<  cert_info.to_string();
                return error::asset_issue_error;
            }

            cert_type.push_back(cur_cert_type);
        }
        else if (!output.is_etp() && !output.is_message())
        {
            log::debug(LOG_BLOCKCHAIN) << "issue: illega output, "
                                       << asset_symbol << " : " << output.to_string(1);
            return error::asset_issue_error;
        }
    }

    // check cert for transactions after check_nova_feature version.
    if (tx.version >= transaction_version::check_nova_feature) {
        if (!asset_cert::test_certs(cert_type, cert_mask)) {
            log::debug(LOG_BLOCKCHAIN) << "issue asset: "
                                       << "not enough cert.";
            return error::asset_issue_error;
        }

        auto&& domain = asset_cert::get_domain(asset_symbol);
        if (asset_cert::is_valid_domain(domain)) {
            if (cert_owner.empty()) {
                log::debug(LOG_BLOCKCHAIN) << "issue asset: owner of cert "
                                           << asset_symbol << " is empty!";
                return error::asset_cert_error;
            }

            if (num_cert_domain_or_naming < 1) {
                // no valid domain or naming cert
                log::debug(LOG_BLOCKCHAIN) << "issue asset: not cert provided!";
                return error::asset_cert_not_provided;
            }
        }
    }

    return error::success;
}

code validate_transaction::check_asset_cert_transaction() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    bool is_cert{false};
    for (auto& output : tx.outputs) {
        if (output.is_asset_cert_issue() || output.is_asset_cert_transfer()) {
            is_cert = true;
            break;
        }
    }

    if (!is_cert) {
        return error::success;
    }

    int num_cert_issue{0};
    int num_cert_domain{0};
    int num_cert_transfer{0};
    asset_cert_type issue_cert_type{asset_cert_ns::none};
    std::vector<asset_cert_type> cert_type;
    std::string cert_symbol;
    std::string domain_symbol;
    std::string cert_owner;
    for (auto& output : tx.outputs)
    {
        if (output.is_asset_cert_issue()) {
            ++num_cert_issue;
            if (num_cert_issue > 1) {
                // can not issue multiple asset cert at the same transaction
                return error::asset_cert_issue_error;
            }

            asset_cert&& cert_info = output.get_asset_cert();
            asset_cert_type cur_cert_type = cert_info.get_type();

            if (!check_same(cert_symbol, cert_info.get_symbol())) {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: "
                                           << cert_info.get_symbol() << " does not match.";
                return error::asset_cert_issue_error;
            }

            // check cert not exists
            if (chain.is_asset_cert_exist(cert_symbol, cur_cert_type)) {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: "
                                           << cert_info.get_symbol() << " already exists.";
                return error::asset_cert_exist;
            }

            issue_cert_type = cur_cert_type;
        }
        else if (output.is_asset_cert_transfer()) {
            ++num_cert_transfer;
            if (num_cert_transfer > 1) {
                // can not transfer multiple asset cert at the same transaction
                return error::asset_cert_error;
            }

            asset_cert&& cert_info = output.get_asset_cert();
            if (!check_same(cert_symbol, cert_info.get_symbol())) {
                log::debug(LOG_BLOCKCHAIN) << "transfer cert: "
                                           << cert_info.get_symbol() << " does not match.";
                return error::asset_cert_error;
            }

            // check cert exists
            auto cur_cert_type = cert_info.get_type();
            if (!chain.is_asset_cert_exist(cert_symbol, cur_cert_type)) {
                log::debug(LOG_BLOCKCHAIN) << "transfer cert: "
                                           << cert_info.get_symbol() << " not exists.";
                return error::asset_cert_not_exist;
            }
        }
        else if (output.is_asset_cert()) {
            asset_cert&& cert_info = output.get_asset_cert();

            // check cert
            asset_cert_type cur_cert_type = cert_info.get_type();
            if (cur_cert_type == asset_cert_ns::domain) {
                if (issue_cert_type != asset_cert_ns::naming) {
                    log::debug(LOG_BLOCKCHAIN) << "issue cert: redundant output of domain cert.";
                    return error::asset_cert_issue_error;
                }

                ++num_cert_domain;
                if (num_cert_domain > 1) {
                    return error::asset_cert_issue_error;
                }

                domain_symbol = cert_info.get_symbol();

                // check owner
                cert_owner = cert_info.get_owner();
                auto diddetail = chain.get_registered_did(cert_owner);
                auto address = cert_info.get_address();
                if (!diddetail) {
                    log::debug(LOG_BLOCKCHAIN) << "issue cert: cert owner is not issued. "
                                               <<  cert_info.to_string();
                    return error::asset_cert_issue_error;
                }
                if (address != diddetail->get_address()) {
                    log::debug(LOG_BLOCKCHAIN) << "issue cert: cert address dismatch cert owner. "
                                               <<  cert_info.to_string();
                    return error::asset_cert_issue_error;
                }
            }
            else {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: invalid output of cert "
                                           <<  cert_info.to_string();
                return error::asset_cert_issue_error;
            }

            cert_type.push_back(cur_cert_type);
        }
        else if (!output.is_etp() && !output.is_message())
        {
            log::debug(LOG_BLOCKCHAIN) << "cert: illegal output attachment type:"
                                       << output.attach_data.get_type()
                                       << ", tx: " << output.to_string(1);
            return error::asset_cert_issue_error;
        }
    }

    if ((num_cert_issue == 0 && num_cert_transfer == 0)
        || (num_cert_issue > 0 && num_cert_transfer > 0)
        || (num_cert_transfer > 0 && num_cert_domain > 0)) {
        log::debug(LOG_BLOCKCHAIN) << "cert: illegal output.";
        return error::asset_cert_error;
    }

    if (num_cert_issue == 1) {
        if (issue_cert_type == asset_cert_ns::none) {
            return error::asset_cert_issue_error;
        }

        if (issue_cert_type == asset_cert_ns::naming) {
            if (!asset_cert::test_certs(cert_type, asset_cert_ns::domain)
                || cert_owner.empty()) {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: "
                                           << "no domain cert provided to issue naming cert.";
                return error::asset_cert_issue_error;
            }

            auto&& domain = asset_cert::get_domain(cert_symbol);
            if (domain != domain_symbol) {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: "
                                           << "invalid domain cert provided to issue naming cert.";
                return error::asset_cert_issue_error;
            }

            // check asset not exist.
            if (chain.is_asset_exist(cert_symbol, false)) {
                log::debug(LOG_BLOCKCHAIN) << "issue cert: "
                                           << "asset symbol '" + cert_symbol + "' already exists in blockchain!";
                return error::asset_exist;
            }
        }
    }

    return error::success;
}

code validate_transaction::check_asset_mit_transaction() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    bool is_asset_mit{false};
    for (auto& output : tx.outputs) {
        if (output.is_asset_mit()) {
            is_asset_mit = true;
            break;
        }
    }

    if (!is_asset_mit) {
        return error::success;
    }

    std::string asset_symbol;
    std::string asset_address;
    size_t num_mit_transfer = 0;
    size_t num_mit_register = 0;
    for (auto& output : tx.outputs)
    {
        if (output.is_asset_mit_register()) {
            ++num_mit_register;

            auto&& asset_info = output.get_asset_mit();
            asset_symbol = asset_info.get_symbol();

            if (!check_same(asset_address, asset_info.get_address())) {
                log::debug(LOG_BLOCKCHAIN) << "register MIT: "
                                           << " address is not same. "
                                           << asset_address << " != " << asset_info.get_address();
                return error::mit_register_error;
            }

            // check asset not exists
            if (nullptr != chain.get_registered_mit(asset_symbol)) {
                log::debug(LOG_BLOCKCHAIN) << "register MIT: "
                                           << asset_symbol << " already exists.";
                return error::mit_exist;
            }
        }
        else if (output.is_asset_mit_transfer()) {
            if (++num_mit_transfer > 1) {
                log::debug(LOG_BLOCKCHAIN) << "transfer MIT: more than on MIT output." << output.to_string(1);
                return error::mit_error;
            }

            auto&& asset_info = output.get_asset_mit();
            asset_symbol = asset_info.get_symbol();

            // check asset exists
            if (nullptr == chain.get_registered_mit(asset_symbol)) {
                log::debug(LOG_BLOCKCHAIN) << "transfer MIT: "
                                           << asset_symbol << " not exists.";
                return error::mit_not_exist;
            }
        }
        else if (output.is_etp()) {
            if (!check_same(asset_address, output.get_script_address())) {
                log::debug(LOG_BLOCKCHAIN) << "MIT: "
                                           << " address is not same. "
                                           << asset_address << " != " << output.get_script_address();
                return error::mit_register_error;
            }
        }
        else if (!output.is_message()) {
            log::debug(LOG_BLOCKCHAIN) << "MIT: illegal output, "
                                       << asset_symbol << " : " << output.to_string(1);
            return error::mit_error;
        }
    }

    if ((num_mit_register == 0 && num_mit_transfer == 0)
        || (num_mit_register > 0 && num_mit_transfer > 0)) {
        log::debug(LOG_BLOCKCHAIN) << "MIT: illegal output.";
        return error::mit_error;
    }

    // check inputs
    bool has_input_transfer = false;
    for (const auto& input : tx.inputs) {
        chain::transaction prev_tx;
        uint64_t prev_height{0};
        if (!get_previous_tx(prev_tx, prev_height, input)) {
            log::debug(LOG_BLOCKCHAIN) << "MIT: input not found: "
                                       << encode_hash(input.previous_output.hash);
            return error::input_not_found;
        }

        auto prev_output = prev_tx.outputs.at(input.previous_output.index);
        if (prev_output.is_etp()) {
            auto&& asset_address_in = prev_output.get_script_address();
            if (asset_address != asset_address_in) {
                log::debug(LOG_BLOCKCHAIN) << "MIT: invalid input address to pay fee: "
                                            << asset_address_in << " != " << asset_address;
                return error::validate_inputs_failed;
            }
        }
        else if (prev_output.is_asset_mit()) {
            auto&& asset_info = prev_output.get_asset_mit();
            if (asset_symbol != asset_info.get_symbol()) {
                log::debug(LOG_BLOCKCHAIN) << "MIT: invalid MIT to transfer: "
                                            << asset_info.get_symbol() << " != " << asset_symbol;
                return error::validate_inputs_failed;
            }

            has_input_transfer = true;
        }
    }

    if (num_mit_transfer > 0 && !has_input_transfer) {
        log::debug(LOG_BLOCKCHAIN) << "MIT: no input MIT to transfer " << asset_symbol;
        return error::validate_inputs_failed;
    }

    return error::success;
}

code validate_transaction::check_did_transaction() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    code ret = error::success;

    uint8_t type = 255;

    for (const auto& output : tx.outputs)
    {
        if ((ret = output.check_attachment_address(chain)) != error::success)
            return ret;

        //to_did check(strong check)
        if ((ret = check_attachment_to_did(output)) != error::success)
            return ret;

        //from_did (weak check)
        if ((ret = connect_attachment_from_did(output)) != error::success) {
            return ret;
        }

        if (output.is_did_register()) {
            if (chain.is_valid_address(output.get_did_symbol())) {
                return error::did_symbol_invalid;
            }

            if (chain.is_did_exist(output.get_did_symbol())) {
                return error::did_exist;
            }

            if (chain.is_address_registered_did(output.get_did_address())) {
                return error::address_registered_did;
            }

            if (type != 255) {
                return error::did_multi_type_exist;
            }
            type = DID_DETAIL_TYPE;

            if (!connect_did_input(boost::get<did>(output.get_did()))) {
                return error::did_input_error;
            }
        }
        else if (output.is_did_transfer()) {
            //did transfer only for did is exist
            if (!chain.is_did_exist(output.get_did_symbol())) {
                return error::did_not_exist;
            }

            if (chain.is_address_registered_did(output.get_did_address())) {
                return error::address_registered_did;
            }

            if (type != 255) {
                return error::did_multi_type_exist;
            }
            type = DID_TRANSFERABLE_TYPE;

            if (!connect_did_input(boost::get<did>(output.get_did()))) {
                return error::did_input_error;
            }
        }
        else if (output.is_asset_issue() || output.is_asset_secondaryissue()) {
            if (output.attach_data.get_version() == DID_ATTACH_VERIFY_VERSION
                    && output.get_asset_issuer() != output.attach_data.get_to_did()) {
                log::debug(LOG_BLOCKCHAIN)
                        << "asset issuer " << output.get_asset_issuer()
                        << " , does not match did " << output.attach_data.get_to_did()
                        << " , attach_data: " << output.attach_data.to_string();
                return error::asset_did_registerr_not_match;
            }
        }
        else if (output.is_asset_cert()) {
            if (output.attach_data.get_version() == DID_ATTACH_VERIFY_VERSION) {
                if (output.get_asset_cert_owner() != output.attach_data.get_to_did()) {
                    log::debug(LOG_BLOCKCHAIN)
                            << "cert owner " << output.get_asset_cert_owner()
                            << " , does not match did " << output.attach_data.get_to_did()
                            << " , attach_data: " << output.attach_data.to_string();
                    return error::asset_did_registerr_not_match;
                }
            }
        }
    }

    return ret;
}

bool validate_transaction::connect_did_input(const did& info) const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    if (info.get_status() ==  DID_TRANSFERABLE_TYPE && tx.inputs.size() != 2) {
        return false;
    }

    auto detail_info = boost::get<did_detail>(info.get_data());
    bool found_did_info = false;
    bool found_address_info = false;

    for (const auto& input : tx.inputs) {
        chain::transaction prev_tx;
        uint64_t prev_height{0};
        if (!get_previous_tx(prev_tx, prev_height, input)) {
            log::debug(LOG_BLOCKCHAIN) << "connect_did_input: input not found: "
                                       << encode_hash(input.previous_output.hash);
            return false;
        }

        auto prev_output = prev_tx.outputs.at(input.previous_output.index);

        if (prev_output.is_did_register() || prev_output.is_did_transfer()) {
            if (info.get_status() ==  DID_TRANSFERABLE_TYPE) {
                if (detail_info.get_symbol() == prev_output.get_did_symbol()) {
                    found_did_info = true;
                }
            }
        }
        else if (prev_output.is_etp()) {
            auto did_address_in = prev_output.get_script_address();
            if (detail_info.get_address() == did_address_in) {
                found_address_info = true;
            }
        }
    }

    return (found_did_info && found_address_info && info.get_status() ==  DID_TRANSFERABLE_TYPE)
           || (found_address_info && info.get_status() ==  DID_DETAIL_TYPE);
}

bool validate_transaction::is_did_in_orphan_chain(const std::string& did, const std::string& address) const
{
    if (validate_block_ && validate_block_->is_did_in_orphan_chain(did, address)) {
        log::debug(LOG_BLOCKCHAIN) << "did_in_orphan_chain: "
            << did << ", address: " << address;
        return true;
    }

    return false;
}

code validate_transaction::check_attachment_to_did(const output& output) const
{
    auto todid = output.attach_data.get_to_did();
    if (todid.empty()) {
        return error::success;
    }

    auto address = output.get_script_address();
    auto did = blockchain_.get_did_from_address(address);
    if (todid == did) {
        return error::success;
    }

    if (is_did_in_orphan_chain(todid, address)) {
        return error::success;
    }

    log::debug(LOG_BLOCKCHAIN) << "check_attachment_to_did: "
        << todid << ", address: " << address;
    return error::did_address_not_match;
}

code validate_transaction::connect_attachment_from_did(const output& output) const
{
    auto from_did = output.attach_data.get_from_did();
    if (from_did.empty()) {
        return error::success;
    }

    for (auto& input : tx_->inputs) {
        chain::transaction prev_tx;
        uint64_t prev_height{0};
        if (!get_previous_tx(prev_tx, prev_height, input)) {
            log::debug(LOG_BLOCKCHAIN) << "connect_attachment_from_did: input not found: "
                                       << encode_hash(input.previous_output.hash);
            return error::input_not_found;
        }

        auto prev_output = prev_tx.outputs.at(input.previous_output.index);
        auto address = prev_output.get_script_address();
        if (from_did == blockchain_.get_did_from_address(address)) {
            return error::success;
        }

        if (is_did_in_orphan_chain(from_did, address)) {
            return error::success;
        }
    }

    log::debug(LOG_BLOCKCHAIN) << "connect_attachment_from_did: input not found for from_did: "
                               << from_did;
    return error::did_address_not_match;
}

code validate_transaction::check_transaction_connect_input(size_t last_height)
{
    if (last_height == 0 || tx_->is_coinbase()) {
        return error::success;
    }

    reset(last_height);

    for (const auto& input : tx_->inputs) {
        chain::transaction prev_tx;
        uint64_t prev_height{0};
        if (!get_previous_tx(prev_tx, prev_height, input)) {
            log::debug(LOG_BLOCKCHAIN) << "check_transaction_connect_input: input not found: "
                                       << encode_hash(input.previous_output.hash);
            return error::input_not_found;
        }
        if (!connect_input(prev_tx, prev_height)) {
            return error::validate_inputs_failed;
        }
        ++current_input_;
    }

    return check_tx_connect_input();
}

code validate_transaction::check_transaction() const
{
    code ret = error::success;

    if ((ret = check_transaction_basic()) != error::success) {
        return ret;
    }

    if ((ret = check_asset_issue_transaction()) != error::success) {
        return ret;
    }

    if (tx_->version >= transaction_version::check_nova_feature) {
        if ((ret = check_asset_cert_transaction()) != error::success) {
            return ret;
        }

        if ((ret = check_secondaryissue_transaction()) != error::success) {
            return ret;
        }

        if ((ret = check_asset_mit_transaction()) != error::success) {
            return ret;
        }

        if ((ret = check_did_transaction()) != error::success) {
            return ret;
        }

        if ((ret = attenuation_model::check_model_param(*this)) != error::success) {
            return ret;
        }
    }

    return ret;
}

code validate_transaction::check_transaction_basic() const
{
    const chain::transaction& tx = *tx_;
    blockchain::block_chain_impl& chain = blockchain_;

    if (tx.version >= transaction_version::max_version) {
        return error::transaction_version_error;
    }

    if (tx.version == transaction_version::check_nova_feature
        && !is_nova_feature_activated(chain)) {
        return error::nova_feature_not_activated;
    }

    if (tx.version == transaction_version::check_nova_testnet
        && !chain.chain_settings().use_testnet_rules) {
        return error::transaction_version_error;
    }

    if (tx.version >= transaction_version::check_output_script) {
        for (auto& i : tx.outputs) {
            if (i.script.pattern() == script_pattern::non_standard) {
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

    for (const auto& output : tx.outputs)
    {
        if (output.value > max_money())
            return error::output_value_overflow;

        total_output_value += output.value;

        if (total_output_value > max_money())
            return error::output_value_overflow;
    }

    for (auto& output : tx.outputs) {
        if (output.is_asset_issue()) {
            if (!chain::output::is_valid_symbol(output.get_asset_symbol(), tx.version)) {
                return error::asset_symbol_invalid;
            }
        }
        else if (output.is_asset_cert()) {
            auto&& asset_cert = output.get_asset_cert();
            if (!chain.is_did_exist(asset_cert.get_owner())) {
                return error::did_address_needed;
            }
        }
        else if (output.is_did_register()) {
            auto is_test = chain.chain_settings().use_testnet_rules;
            if (!chain::output::is_valid_did_symbol(output.get_did_symbol(), !is_test)) {
                return error::did_symbol_invalid;
            }
        }
        else if (output.is_asset_mit_register()) {
            if (!chain::output::is_valid_mit_symbol(output.get_asset_symbol(), true)) {
                return error::mit_symbol_invalid;
            }
        }

        // check attachment, from nova version.
        if ((tx.version >= transaction_version::check_nova_feature)
            && (!output.attach_data.is_valid())) {
            log::debug(LOG_BLOCKCHAIN) << "invalid attachment : "
                << output.attach_data.to_string();
            return error::attachment_invalid;
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
        for (const auto& input : tx.inputs)
        {
            if (input.previous_output.is_null())
                return error::previous_output_null;

            if (chain::operation::is_sign_key_hash_with_lock_height_pattern(input.script.operations)) {
                uint64_t prev_output_blockheight = 0;
                chain::transaction prev_tx;
                uint64_t current_blockheight = 0;

                chain.get_last_height(current_blockheight);
                if (!get_previous_tx(prev_tx, prev_output_blockheight, input)) {
                    log::debug(LOG_BLOCKCHAIN) << "check_transaction_basic deposit : input not found: "
                                               << encode_hash(input.previous_output.hash);
                    return error::input_not_found;
                }

                uint64_t lock_height = chain::operation::get_lock_height_from_sign_key_hash_with_lock_height(input.script.operations);
                if (lock_height > current_blockheight - prev_output_blockheight) {
                    return error::invalid_input_script_lock_height;
                }
            }
        }

        for (auto& output : tx.outputs)
        {
            if (chain::operation::is_pay_key_hash_with_lock_height_pattern(output.script.operations)) {
                uint64_t lock_height = chain::operation::get_lock_height_from_pay_key_hash_with_lock_height(output.script.operations);
                if ((int)lock_height < 0
                        || consensus::miner::get_lock_heights_index(lock_height) < 0) {
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

    if ((flags & script_context::attenuation_enabled) != 0)
        consensus_flags |= verify_flags_checkattenuationverify;

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

    if (!valid) {
        log::warning(LOG_BLOCKCHAIN)
                << "Invalid transaction ["
                << encode_hash(current_tx.hash()) << "]";
    }

    return valid;
}

bool validate_transaction::connect_input( const transaction& previous_tx, size_t parent_height)
{
    const auto& input = tx_->inputs[current_input_];
    const auto& previous_outpoint = input.previous_output;

    if (previous_outpoint.index >= previous_tx.outputs.size()) {
        log::debug(LOG_BLOCKCHAIN) << "output point index outof bounds!";
        return false;
    }

    const auto& previous_output = previous_tx.outputs[previous_outpoint.index];
    const auto output_value = previous_output.value;
    if (output_value > max_money()) {
        log::debug(LOG_BLOCKCHAIN) << "output etp value exceeds max amount!";
        return false;
    }

    asset_cert_type asset_certs = asset_cert_ns::none;
    uint64_t asset_transfer_amount = 0;
    if (previous_output.is_asset()) {
        auto new_symbol_in = previous_output.get_asset_symbol();
        // 1. do asset transfer amount check
        asset_transfer_amount = previous_output.get_asset_amount();

        // 2. do asset symbol check
        if (!check_same(old_symbol_in_, new_symbol_in)) {
            return false;
        }
        // check forbidden symbol
        if (bc::wallet::symbol::is_forbidden(new_symbol_in)) {
            return false;
        }
    }
    else if (previous_output.is_asset_cert()) {
        asset_certs = previous_output.get_asset_cert_type();
        if (!check_same(old_cert_symbol_in_, previous_output.get_asset_cert_symbol())) {
            return false;
        }
    }
    else if (previous_output.is_asset_mit()) {
        if (!check_same(old_symbol_in_, previous_output.get_asset_mit_symbol())) {
            return false;
        }
    }
    else if (previous_output.is_did()) {
        // 1. do did symbol check
        if (!check_same(old_symbol_in_, previous_output.get_did_symbol())) {
            return false;
        }
    }

    if (previous_tx.is_coinbase()) {
        const auto height_difference = last_block_height_ - parent_height;
        if (height_difference < coinbase_maturity) {
            return false;
        }
    }

    if (!check_consensus(previous_output.script, *tx_, current_input_, script_context::all_enabled)) {
        log::debug(LOG_BLOCKCHAIN) << "check_consensus failed";
        return false;
    }

    value_in_ += output_value;
    asset_amount_in_ += asset_transfer_amount;
    if (asset_certs != asset_cert_ns::none) {
        asset_certs_in_.push_back(asset_certs);
    }
    return value_in_ <= max_money();
}

bool validate_transaction::tally_fees(const transaction& tx, uint64_t value_in,
                                      uint64_t& total_fees)
{
    const auto value_out = tx.total_output_value();

    if (value_in < value_out)
        return false;

    const auto fee = value_in - value_out;
    if (fee < min_tx_fee)
        return false;
    total_fees += fee;
    return total_fees <= max_money();
}

bool validate_transaction::check_asset_amount(const transaction& tx) const
{
    const auto asset_amount_out = tx.total_output_transfer_amount();
    if (asset_amount_in_ != asset_amount_out) // asset amount must be equal
        return false;

    return true;
}

bool validate_transaction::check_asset_symbol(const transaction& tx) const
{
    for (const auto& output : tx.outputs) {
        if (output.is_asset()) {
            if (old_symbol_in_ != output.get_asset_symbol()) {
                return false;
            }
        }
        else if (output.is_asset_cert()) { // asset cert related
            continue;
        }
        else if (!output.is_etp() && !output.is_message()) {
            // asset tx only related to asset_cert and etp output
            return false;
        }
    }
    return true;
}

bool validate_transaction::check_asset_certs(const transaction& tx) const
{
    bool is_cert_transfer = false;
    bool is_cert_issue = false;
    bool has_cert_autoissue = false;
    bool has_asset_issue = false;
    bool has_asset_output = false;
    std::vector<asset_cert_type> asset_certs_out;
    for (auto& output : tx.outputs) {
        if (output.is_asset_cert()) {
            auto&& asset_cert = output.get_asset_cert();
            auto cert_type = asset_cert.get_type();

            if (asset_cert.get_status() == ASSET_CERT_TRANSFER_TYPE) {
                is_cert_transfer = true;
            }
            else if (asset_cert.get_status() == ASSET_CERT_ISSUE_TYPE) {
                is_cert_issue = true;
            }
            else if (asset_cert.get_status() == ASSET_CERT_AUTOISSUE_TYPE) {
                has_cert_autoissue = true;
            }

            if (asset_cert::test_certs(asset_certs_out, cert_type)) { // double certs exists
                return false;
            }

            // check asset cert symbol
            if (asset_cert::test_certs(asset_certs_in_, asset_cert_ns::domain)
                && (cert_type == asset_cert_ns::naming)) {
                auto&& domain = asset_cert::get_domain(asset_cert.get_symbol());
                if (domain != old_cert_symbol_in_) {
                    log::debug(LOG_BLOCKCHAIN) << "cert domain error: "
                        << "symbol : " << asset_cert.get_symbol() << "; "
                        << old_cert_symbol_in_ << " != " << domain;
                    return false;
                }
            }
            else if (!asset_cert.is_newly_generated()) {
                if (old_cert_symbol_in_ != asset_cert.get_symbol()) {
                    log::debug(LOG_BLOCKCHAIN) << "cert symbol error: "
                        << old_cert_symbol_in_ << " != " << asset_cert.get_symbol();
                    return false;
                }
            }

            if (!asset_cert.is_newly_generated()) {
                if (!asset_cert::test_certs(asset_certs_in_, cert_type)) {
                    log::debug(LOG_BLOCKCHAIN) << "input lack of cert: " << asset_cert.get_type_name();
                    return false;
                }
            }

            asset_certs_out.push_back(cert_type);
        }
        else if (output.is_asset()) { // asset related
            has_asset_output = true;
            if (output.is_asset_issue()) {
                has_asset_issue = true;
            }
        }
        else if (!output.is_etp() && !output.is_message()) {
            // asset cert transfer tx only related to asset_cert and etp output
            log::debug(LOG_BLOCKCHAIN) << "cert tx mix other illegal output";
            return false;
        }
    }

    if ((is_cert_issue || is_cert_transfer) && has_asset_output) {
        log::debug(LOG_BLOCKCHAIN) << "issue/transfer cert can not mix with asset output";
        return false;
    }

    if (has_cert_autoissue && !has_asset_issue) {
        log::debug(LOG_BLOCKCHAIN) << "only issue command can has cert with autoissue status";
        return false;
    }

    if (is_cert_transfer) {
        if (asset_certs_in_.size() != 1) {
            log::debug(LOG_BLOCKCHAIN) << "transfer cert: invalid number of cert in inputs: "
                                       << asset_certs_in_.size();
            return false;
        }
        if (asset_certs_out.size() != 1) {
            log::debug(LOG_BLOCKCHAIN) << "transfer cert: invalid number of cert in outputs: "
                                       << asset_certs_out.size();
            return false;
        }
    }

    if (!asset_cert::test_certs(asset_certs_out, asset_certs_in_)) {
        log::debug(LOG_BLOCKCHAIN) << "some inputed certs is missed in outputs";
        return false;
    }
    return true;
}

bool validate_transaction::check_asset_mit(const transaction& tx) const
{
    size_t num_mit = 0;
    for (const auto& output : tx.outputs) {
        if (output.is_asset_mit_transfer()) {
            if (++num_mit > 1) {
                return false;
            }

            auto&& asset_info = output.get_asset_mit();
            if (old_symbol_in_ != asset_info.get_symbol()) {
                return false;
            }
        }
        else if (!output.is_etp() && !output.is_message()) {
            return false;
        }
    }

    return (num_mit == 1);
}

bool validate_transaction::check_did_symbol_match(const transaction& tx) const
{
    for (const auto& output : tx.outputs) {
        if (output.is_did()) {
            if (old_symbol_in_ != output.get_did_symbol()) {
                return false;
            }
        }
        else if (!output.is_etp() && !output.is_message()) {
            return false;
        }
    }
    return true;
}

bool validate_transaction::is_nova_feature_activated(blockchain::block_chain_impl& chain)
{
    if (chain.chain_settings().use_testnet_rules) {
        return true;
    }

    uint64_t current_blockheight = 0;
    chain.get_last_height(current_blockheight);

    // active SuperNove on 2018-06-18 (duanwu festival)
    return (current_blockheight > 1270000);
}

} // namespace blockchain
} // namespace libbitcoin
