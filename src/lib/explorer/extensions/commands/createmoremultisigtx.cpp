/**
 * Copyright (c) 2016-2020 mvs developers
 *
 * This file is part of metaverse-explorer.
 *
 * metaverse-explorer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <metaverse/explorer/json_helper.hpp>
#include <metaverse/explorer/dispatch.hpp>
#include <metaverse/explorer/extensions/commands/createmoremultisigtx.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/exception.hpp>
#include <metaverse/explorer/extensions/base_helper.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {

using namespace bc::explorer::config;

console_result createmoremultisigtx::invoke(
    Json::Value& jv_output,
    libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    auto account = blockchain.is_account_passwd_valid(auth_.name, auth_.auth);

    // check from address
    if (!blockchain.is_valid_address(argument_.from)) {
        throw fromaddress_invalid_exception{"invalid from address!"};
    }

    auto addr = bc::wallet::payment_address(argument_.from);
    if (addr.version() != bc::wallet::payment_address::mainnet_p2sh) {
        throw fromaddress_invalid_exception{"from address is not a script address."};
    }

    auto multisig_vec = account->get_multisig(argument_.from);
    if (!multisig_vec || multisig_vec->empty()) {
        throw multisig_notfound_exception{"multisig of from address not found."};
    }

    auto acc_multisig = *(multisig_vec->begin());

    std::vector<receiver_record> receiver;
    auto type = static_cast<utxo_attach_type>(option_.type);
    switch (type) {
        case utxo_attach_type::etp: {
			for (auto& each : argument_.receivers) {
				colon_delimited2_item<std::string, uint64_t> item(each);
				receiver.push_back({ item.first(), "", item.second(), 0, type, chain::attachment() });
			}
            break;
        }

        case utxo_attach_type::asset_transfer: {
            blockchain.uppercase_symbol(option_.symbol);
            check_asset_symbol(option_.symbol);
			for (auto& each : argument_.receivers) {
				colon_delimited2_item<std::string, uint64_t> item(each);
				receiver.push_back({ item.first(), option_.symbol, 0, item.second(), type, chain::attachment() });
			}
            break;
        }

        default: {
            throw argument_legality_exception{"invalid transaction type."};
        }
        break;
    }

    auto sp_send_helper = std::make_shared<sending_multisig_tx>(*this, blockchain,
        std::move(auth_.name), std::move(auth_.auth),
        std::move(argument_.from), std::move(receiver),
        argument_.fee, acc_multisig, std::move(option_.symbol));

    sp_send_helper->exec();

    // output json
    auto && tx = sp_send_helper->get_transaction();
    jv_output = config::json_helper(get_api_version()).prop_list_of_rawtx(tx, false, true);

    return console_result::okay;
}


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

