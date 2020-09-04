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
#include <metaverse/explorer/extensions/commands/sendmore.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/exception.hpp>
#include <metaverse/explorer/extensions/base_helper.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {


console_result sendmore::invoke (Json::Value& jv_output,
    libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    blockchain.is_account_passwd_valid(auth_.name, auth_.auth);

    std::string change_address = get_address(option_.change, blockchain);
    std::string from_address = get_address(option_.from, blockchain);

    std::string msg_address;
    std::string from_did;
    if (!from_address.empty() && !blockchain.is_valid_address(option_.from)) {
        from_did = option_.from;
    }

    if (!option_.exclude.is_valid()) {
		throw argument_legality_exception("invalid exclude option! "
			+ option_.exclude.encode_colon_delimited());
	}
    
    // receiver
    std::vector<receiver_record> receiver;

    for (auto& each : argument_.receivers) {
        colon_delimited2_item<std::string, uint64_t> item(each);

        chain::attachment attach;
        std::string to_address = get_address(item.first(), attach, false, blockchain);
        if (!from_did.empty()) {
            attach.set_version(DID_ATTACH_VERIFY_VERSION);
            attach.set_from_did(from_did);
        }

        if (item.second() <= 0) {
            throw argument_legality_exception("invalid amount parameter for " + item.first());
        }

        receiver.push_back({to_address,"", item.second(), 0, utxo_attach_type::etp, attach});

        if (msg_address.empty()) {
            msg_address = to_address;
        }
    }


    // memo
    if (!option_.memo.empty()) {
        check_message(option_.memo);

        if (!change_address.empty()) {
            msg_address = change_address;
        }

        receiver.push_back({msg_address, "", 0, 0, utxo_attach_type::message,
            chain::attachment(0, 0, chain::blockchain_message(option_.memo))});
    }

    auto send_helper = sending_etp(
		*this, blockchain,
		std::move(auth_.name), std::move(auth_.auth),
		std::move(from_address), std::move(receiver),
		std::move(change_address), option_.fee, 0,
		std::make_pair(option_.exclude.first(), option_.exclude.second()));

    send_helper.exec();

    // json output
    auto tx = send_helper.get_transaction();
    jv_output =  config::json_helper(get_api_version()).prop_tree(tx, true);

    return console_result::okay;
}


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

