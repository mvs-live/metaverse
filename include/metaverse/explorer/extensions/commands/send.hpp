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
#include <metaverse/macros_define.hpp>
#include <metaverse/explorer/define.hpp>
#include <metaverse/explorer/extensions/command_extension.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {


class send: public send_command
{
public:
    static const char* symbol(){ return "send";}
    const char* name() override { return symbol();}
    bool category(int bs) override { return (ex_online & bs ) == bs; }
    const char* description() override { return "send etp to a targert did/address."; }

    arguments_metadata& load_arguments() override
    {
        return get_argument_metadata()
            .add("ACCOUNTNAME", 1)
            .add("ACCOUNTAUTH", 1)
            .add("TO_", 1)
            .add("AMOUNT", 1);
    }

    void load_fallbacks (std::istream& input,
        po::variables_map& variables) override
    {
        const auto raw = requires_raw_input();
        load_input(auth_.name, "ACCOUNTNAME", variables, input, raw);
        load_input(auth_.auth, "ACCOUNTAUTH", variables, input, raw);
        load_input(argument_.to, "TO_", variables, input, raw);
        load_input(argument_.amount, "AMOUNT", variables, input, raw);
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
            "TO_",
            value<std::string>(&argument_.to)->required(),
            "Send to this did/address"
        )
        (
            "AMOUNT",
            value<uint64_t>(&argument_.amount)->required(),
            "ETP integer bits."
        )
        (
            "change,c",
            value<std::string>(&option_.change)->default_value(""),
            "Change to this did/address"
        )
        (
            "memo,m",
            value<std::string>(&option_.memo)->default_value(""),
            "Attached memo for this transaction."
        )
#ifdef ENABLE_LOCKTIME
        (
            "locktime,x",
            value<uint32_t>(&option_.locktime)->default_value(0),
            "Locktime. defaults to 0"
        )
#endif
        (
            "exclude,e",
            value<colon_delimited2_item<uint64_t, uint64_t>>(&option_.exclude),
            "Exclude utxo whose value is between this range [begin:end) the "
            "limit of which is split by a colon."
        )
        (
            "fee,f",
            value<uint64_t>(&option_.fee)->default_value(10000),
            "Transaction fee. defaults to 10000 etp bits"
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
        argument():to("")
        {};
        std::string to;
        uint64_t amount;
    } argument_;

    struct option
    {
        uint64_t fee;
        std::string memo;
        std::string change;
        uint32_t locktime;
        colon_delimited2_item<uint64_t, uint64_t> exclude = {0, 0};
    } option_;

};


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

