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

#pragma once
#include <metaverse/explorer/define.hpp>
#include <metaverse/explorer/extensions/command_extension.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {


class createmoremultisigtx: public command_extension
{
public:
    static const char* symbol() { return "createmoremultisigtx";}
    const char* name() override { return symbol();}
    bool category(int bs) override { return (ctgy_extension & bs ) == bs; }
    const char* description() override { return "Create more multiple signatures."; }

    arguments_metadata& load_arguments() override
    {
        return get_argument_metadata()
               .add("ACCOUNTNAME", 1)
               .add("ACCOUNTAUTH", 1)
               .add("FROMADDRESS", 1);
    }

    void load_fallbacks (std::istream& input,
                         po::variables_map& variables) override
    {
        const auto raw = requires_raw_input();
        load_input(auth_.name, "ACCOUNTNAME", variables, input, raw);
        load_input(auth_.auth, "ACCOUNTAUTH", variables, input, raw);
        load_input(argument_.from, "FROMADDRESS", variables, input, raw);
    }

    options_metadata& load_options() override
    {
        using namespace po;
        options_description& options = get_option_metadata();
        options.add_options()
        (
            BX_HELP_VARIABLE ",h",
            value<bool>()->zero_tokens(),
            "Get a description and instructions for this command."
        )
        (
            "ACCOUNTNAME",
            value<std::string>(&auth_.name)->required(),
            BX_ACCOUNT_NAME
        )
        (
            "ACCOUNTAUTH",
            value<std::string>(&auth_.auth)->required(),
            BX_ACCOUNT_AUTH
        )
        (
            "FROMADDRESS",
            value<std::string>(&argument_.from)->required(),
            "Send from this address, must be a multi-signature script address."
        )
        (
			"receivers,r",
			value<std::vector<std::string>>(&argument_.receivers)->required(),
			"Send to [address:etp_bits]."
        )
        (
            "symbol,s",
            value<std::string>(&option_.symbol),
            "asset name, not specify this option for etp tx"
        )
        (
            "type,t",
            value<uint16_t>(&option_.type)->default_value(0),
            "Transaction type, defaults to 0. 0 -- transfer etp, 3 -- transfer asset"
        )
        (
            "fee,f",
            value<uint64_t>(&argument_.fee)->default_value(10000),
            "Transaction fee. defaults to 10000 ETP bits"
        )
        ;

        return options;
    }

    void set_defaults_from_config (po::variables_map& variables) override
    {
    }

    console_result invoke (Json::Value& jv_output,
                           libbitcoin::server::server_node& node) override;

    struct argument
    {
        argument() :
            fee(0)
        {}
		std::vector<std::string> receivers;
        std::string from;
        uint64_t fee;
    } argument_;

    struct option
    {
        std::string symbol;
        uint16_t type;
    } option_;

};


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

