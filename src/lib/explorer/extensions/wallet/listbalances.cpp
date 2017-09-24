/**
 * Copyright (c) 2016-2017 mvs developers 
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

#include <boost/property_tree/ptree.hpp>      
#include <boost/property_tree/json_parser.hpp>

#include <metaverse/bitcoin.hpp>
#include <metaverse/client.hpp>
#include <metaverse/explorer/define.hpp>
#include <metaverse/explorer/callback_state.hpp>
#include <metaverse/explorer/display.hpp>
#include <metaverse/explorer/prop_tree.hpp>
#include <metaverse/explorer/dispatch.hpp>
#include <metaverse/explorer/extensions/wallet/listbalances.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/base_helper.hpp>
#include <metaverse/explorer/extensions/exception.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {

namespace pt = boost::property_tree;

#define IN_DEVELOPING "this command is in deliberation, or replace it with original command."

/************************ listbalances *************************/

console_result listbalances::invoke (std::ostream& output,
        std::ostream& cerr, libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    blockchain.is_account_passwd_valid(auth_.name, auth_.auth);

    pt::ptree aroot;
    pt::ptree all_balances;
    pt::ptree address_balances;
    
    auto vaddr = blockchain.get_account_addresses(auth_.name);
    if(!vaddr) throw address_list_nullptr_exception{"nullptr for address list"};

    std::string type("all");
    
    for (auto& i: *vaddr){
        pt::ptree address_balance;
        balances addr_balance{0, 0, 0, 0};
        auto waddr = wallet::payment_address(i.get_address());
        sync_fetchbalance(waddr, type, blockchain, addr_balance, 0);
        address_balance.put("address", i.get_address());
        address_balance.put("confirmed", addr_balance.confirmed_balance);
        address_balance.put("received", addr_balance.total_received);
        address_balance.put("unspent", addr_balance.unspent_balance);
        address_balance.put("available", addr_balance.unspent_balance - addr_balance.frozen_balance);
        address_balance.put("frozen", addr_balance.frozen_balance);
         
        pt::ptree null_balances;
        // non_zero display options
        if (option_.non_zero){
            if (addr_balance.unspent_balance){
                null_balances.add_child("balance", address_balance);
                all_balances.push_back(std::make_pair("", null_balances));
            }
        } else {
            null_balances.add_child("balance", address_balance);
            all_balances.push_back(std::make_pair("", null_balances));
        }
    }
    
    aroot.add_child("balances", all_balances);
    pt::write_json(output, aroot);
    return console_result::okay;

}


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

