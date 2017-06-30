﻿/**
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
#ifndef MVS_OFSTREAM_HPP
#define MVS_OFSTREAM_HPP

#include <fstream>
#include <string>
#include <metaverse/bitcoin/define.hpp>

namespace libbitcoin {

/**
 * Use bc::ofstream in place of std::ofstream.
 * This provides utf8 to utf16 path translation for Windows.
 */
class BC_API ofstream
  : public std::ofstream
{
public:
    /**
     * Construct bc::ofstream.
     * @param[in]  path  The utf8 path to the file.
     * @param[in]  mode  The file opening mode.
     */
    ofstream(const std::string& path,
        std::ofstream::openmode mode=std::ofstream::out);

	/**
	* Construct bc::ofstream.
	* @param[in]  path  The unicode path to the file.
	* @param[in]  mode  The file opening mode.
	*/
	ofstream(const std::wstring &path,
		std::ofstream::openmode mode = std::ofstream::out);
};

} // namespace libbitcoin

#endif
