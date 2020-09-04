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



/************************ sendmore *************************/

class sendmore: public send_command
{
public:
    static const char* symbol(){ return "sendmore";}
    const char* name() override { return symbol();}
    bool category(int bs) override { return (ex_online & bs ) == bs; }
    const char* description() override { return "send etp to multi target."; }

    arguments_metadata& load_arguments() override
    {
        return get_argument_metadata()
            .add("ACCOUNTNAME", 1)
            .add("ACCOUNTAUTH", 1);
    }

    void load_fallbacks (std::istream& input,
        po::variables_map& variables) override
    {
        const auto raw = requires_raw_input();
        load_input(auth_.name, "ACCOUNTNAME", variables, input, raw);
        load_input(auth_.auth, "ACCOUNTAUTH", variables, input, raw);
    }

    options_metadata& load_options() override
    {
        using namespace po;
        options_description& options = get_option_metadata();
        options.add_options()
        (
            BX_HELP_VARIABLE ",h",
            value<bool>()->zero_tokens(),
            "Send to more target. "
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
            "receivers,r",
            value<std::vector<std::string>>(&argument_.receivers)->required(),
            "Send to [did/address:etp_bits]."
        )
        (
            "mychange,m",
            value<std::string>(&option_.change)->default_value(""),
            "Change to this did/address"
        )
        (
            "from,s",
            value<std::string>(&option_.from)->default_value(""),
            "Send from this did/address"
        )
        (
            "memo,i",
            value<std::string>(&option_.memo)->default_value(""),
            "The memo to descript transaction"
        )
        (
	    "exclude,e",
	    value<colon_delimited2_item<uint64_t, uint64_t>>(&option_.exclude),
            "Exclude utxo whose value is between this range [begin:end) the "
            "limit of which is split by a colon."
	)
        (
            "fee,f",
            value<uint64_t>(&option_.fee)->default_value(10000),
            "Transaction fee. defaults to 10000 ETP bits"
        );

        return options;
    }

    void set_defaults_from_config (po::variables_map& variables) override
    {
    }

    console_result invoke (Json::Value& jv_output,
         libbitcoin::server::server_node& node) override;

    struct argument
    {
        std::vector<std::string> receivers;
    } argument_;

    struct option
    {
        option():fee(10000), from(""), change(""), memo("")
        {};

        uint64_t fee;
        std::string from;
        std::string change;
        std::string memo;
        colon_delimited2_item<uint64_t, uint64_t> exclude = { 0, 0 };
    } option_;

};



} // namespace commands
} // namespace explorer
} // namespace libbitcoin

