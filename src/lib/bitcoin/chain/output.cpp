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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <metaverse/bitcoin/chain/output.hpp>
#include <cctype>
#include <sstream>
#include <boost/iostreams/stream.hpp>
#include <metaverse/bitcoin/utility/container_sink.hpp>
#include <metaverse/bitcoin/utility/container_source.hpp>
#include <metaverse/bitcoin/utility/istream_reader.hpp>
#include <metaverse/bitcoin/utility/ostream_writer.hpp>

namespace libbitcoin {
namespace chain {

output output::factory_from_data(const data_chunk& data)
{
    output instance;
    instance.from_data(data);
    return instance;
}

output output::factory_from_data(std::istream& stream)
{
    output instance;
    instance.from_data(stream);
    return instance;
}

output output::factory_from_data(reader& source)
{
    output instance;
    instance.from_data(source);
    return instance;
}
bool output::is_valid_symbol(const std::string& symbol)
{
    // length check	
    if (symbol.length() > ASSET_DETAIL_SYMBOL_FIX_SIZE)
		return false;
	// char check
    for (auto& i : symbol){
        if (!(std::isalnum(i) || i=='.'))
            return false;
    }
	return true;
}

bool output::is_valid() const
{
    return (value != 0) || script.is_valid()
		|| attach_data.is_valid(); // added for asset issue/transfer
}

void output::reset()
{
    value = 0;
    script.reset();
	attach_data.reset(); // added for asset issue/transfer
}

bool output::from_data(const data_chunk& data)
{
    data_source istream(data);
    return from_data(istream);
}

bool output::from_data(std::istream& stream)
{
    istream_reader source(stream);
    return from_data(source);
}

bool output::from_data(reader& source)
{
    reset();

    value = source.read_8_bytes_little_endian();
    auto result = static_cast<bool>(source);

    if (result)
        result = script.from_data(source, true, 
            script::parse_mode::raw_data_fallback);

	/* begin added for asset issue/transfer */
    if (result)
        result = attach_data.from_data(source);
	/* end added for asset issue/transfer */

    if (!result)
        reset();

    return result;
}

data_chunk output::to_data() const
{
    data_chunk data;
    data_sink ostream(data);
    to_data(ostream);
    ostream.flush();
	log::debug("output::to_data") << "data.size=" << data.size();
	log::debug("output::to_data") << "serialized_size=" << serialized_size();
    //BITCOIN_ASSERT(data.size() == serialized_size());
    return data;
}

void output::to_data(std::ostream& stream) const
{
    ostream_writer sink(stream);
    to_data(sink);
}

void output::to_data(writer& sink) const
{
    sink.write_8_bytes_little_endian(value);
    script.to_data(sink, true);
	/* begin added for asset issue/transfer */
	attach_data.to_data(sink);
	/* end added for asset issue/transfer */
}

uint64_t output::serialized_size() const
{
    return 8 + script.serialized_size(true)
		+ attach_data.serialized_size(); // added for asset issue/transfer
}

std::string output::to_string(uint32_t flags) const
{
    std::ostringstream ss;

    ss << "\tvalue = " << value << "\n"
        << "\t" << script.to_string(flags) << "\n"
        << "\t" << attach_data.to_string() << "\n"; // added for asset issue/transfer

    return ss.str();
}

uint64_t output::get_asset_amount() const // for validate_transaction.cpp to calculate asset transfer amount
{
	if(attach_data.get_type() == ASSET_TYPE) {
		auto asset_info = boost::get<asset>(const_cast<attachment&>(attach_data).get_attach());
		if(asset_info.get_status() == ASSET_DETAIL_TYPE) {
			auto detail_info = boost::get<asset_detail>(asset_info.get_data());
			return detail_info.get_maximum_supply();
		}
		if(asset_info.get_status() == ASSET_TRANSFERABLE_TYPE) {
			auto trans_info = boost::get<asset_transfer>(asset_info.get_data());
			return trans_info.get_quantity();
		}
	}
	return 0;
}
bool output::is_asset_transfer()
{
	if(attach_data.get_type() == ASSET_TYPE) {
		auto asset_info = boost::get<asset>(attach_data.get_attach());
		return (asset_info.get_status() == ASSET_TRANSFERABLE_TYPE); 
	}
	return false;
}
bool output::is_asset_issue()
{
	if(attach_data.get_type() == ASSET_TYPE) {
		auto asset_info = boost::get<asset>(attach_data.get_attach());
		return (asset_info.get_status() == ASSET_DETAIL_TYPE); 
	}
	return false;
}

bool output::is_etp()
{
	return (attach_data.get_type() == ETP_TYPE);
}

std::string output::get_asset_symbol() // for validate_transaction.cpp to calculate asset transfer amount
{
	if(attach_data.get_type() == ASSET_TYPE) {
		auto asset_info = boost::get<asset>(attach_data.get_attach());
		if(asset_info.get_status() == ASSET_DETAIL_TYPE) {
			auto detail_info = boost::get<asset_detail>(asset_info.get_data());
			return detail_info.get_symbol();
		}
		if(asset_info.get_status() == ASSET_TRANSFERABLE_TYPE) {
			auto trans_info = boost::get<asset_transfer>(asset_info.get_data());
			return trans_info.get_address();
		}
	}
	return std::string("");
}

} // namspace chain
} // namspace libbitcoin
