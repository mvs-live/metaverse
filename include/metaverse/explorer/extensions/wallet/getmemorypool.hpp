/*
 * getmemorypool.hpp
 *
 *  Created on: Jul 3, 2017
 *      Author: jiang
 */

#ifndef INCLUDE_METAVERSE_EXPLORER_EXTENSIONS_WALLET_GETMEMORYPOOL_HPP_
#define INCLUDE_METAVERSE_EXPLORER_EXTENSIONS_WALLET_GETMEMORYPOOL_HPP_


#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <metaverse/bitcoin.hpp>
#include <metaverse/client.hpp>
#include <metaverse/explorer/define.hpp>
#include <metaverse/explorer/callback_state.hpp>
#include <metaverse/explorer/display.hpp>
#include <metaverse/explorer/prop_tree.hpp>
#include <metaverse/explorer/dispatch.hpp>
#include <metaverse/explorer/extensions/command_extension.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {

namespace pt = boost::property_tree;

/************************ getbalance *************************/

class getmemorypool: public command_extension
{
public:
    static const char* symbol(){ return "getmemorypool";}
    const char* name() override { return symbol();}
    const char* category() override { return "EXTENSION"; }
    const char* description() override { return "Returns all transactions in memory pool."; }

    arguments_metadata& load_arguments() override
    {
        return get_argument_metadata();
    }

    void load_fallbacks (std::istream& input,
        po::variables_map& variables) override
    {
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
        );

        return options;
    }

    void set_defaults_from_config (po::variables_map& variables) override
    {
    }

    console_result invoke (std::ostream& output,
        std::ostream& cerr, libbitcoin::server::server_node& node) override;

    struct argument
    {
    } argument_;

    struct option
    {
    } option_;

};




} // namespace commands
} // namespace explorer
} // namespace libbitcoin




#endif /* INCLUDE_METAVERSE_EXPLORER_EXTENSIONS_WALLET_GETMEMORYPOOL_HPP_ */
