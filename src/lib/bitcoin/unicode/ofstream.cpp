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
#include <metaverse/bitcoin/unicode/ofstream.hpp>

#include <fstream>
#include <string>
#include <metaverse/bitcoin/unicode/unicode.hpp>

namespace libbitcoin {

// Construct bc::ofstream.
ofstream::ofstream(const std::string& path, std::ofstream::openmode mode)
#ifdef _MSC_VER
  : std::ofstream(bc::to_utf16(path), mode)
#else
  : std::ofstream(path, mode)
#endif
{
}

ofstream::ofstream(const std::wstring& path, std::ofstream::openmode mode)
#ifdef _MSC_VER
	: std::ofstream(path, mode)
#else
    : std::ofstream(bc::to_utf8(path), mode)
#endif
{
}

} // namespace libbitcoin
