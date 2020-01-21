/**
 * Copyright (c) 2019-2020 libbitcoin developers (see AUTHORS)
 * Copyright (c) 2019-2020 metaverse core developers (see MVS-AUTHORS)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <metaverse/bitcoin/constants.hpp>

namespace libbitcoin {
uint32_t coinbase_maturity = 1000;

hash_number max_target()
{
    hash_number max_target;
    max_target.set_compact(max_work_bits);
    return max_target;
}

std::string get_developer_community_address(bool is_testnet)
{
    std::string address("MAwLwVGwJyFsTBfNj2j5nCUrQXGVRvHzPh");  // developer-community address for mainnet
    if (is_testnet) {
        address = "tJNo92g6DavpaCZbYjrH45iQ8eAKnLqmms";         // developer-community address for testnet
    }
    return address;
}

} // namespace libbitcoin
