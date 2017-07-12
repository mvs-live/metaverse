/*
 * getmemorypool.cpp
 *
 *  Created on: Jul 3, 2017
 *      Author: jiang
 */




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
#include <metaverse/explorer/extensions/wallet/getmemorypool.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/base_helper.hpp>
#include <metaverse/explorer/extensions/exception.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {

namespace pt = boost::property_tree;


/************************ getbalance *************************/

console_result getmemorypool::invoke (std::ostream& output,
        std::ostream& cerr, libbitcoin::server::server_node& node)
{
    using transaction_ptr = message::transaction_message::ptr ;
    auto& blockchain = node.chain_impl();
    std::promise<code> p;
    blockchain.pool().fetch([&output, &p](const code& ec, const std::vector<transaction_ptr>& txs){
        if (ec)
        {
            p.set_value(ec);
            return;
        }
        std::vector<config::transaction> txs1;
        txs1.reserve(txs.size());
        for (auto tp:txs) {
            txs1.push_back(*tp);
        }
        pt::write_json(output, config::prop_tree(txs1, true));
        p.set_value(ec);
    });

    auto result = p.get_future().get();
    if(result){
        throw tx_fetch_exception{result.message()};
    }
    return console_result::okay;
}


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

