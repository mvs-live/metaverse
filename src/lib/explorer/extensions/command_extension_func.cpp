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


#include <functional>
#include <memory>
#include <string>
#include <array>

#include <metaverse/explorer/command.hpp>
#include <metaverse/explorer/dispatch.hpp>
#include <metaverse/explorer/extensions/command_extension.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/commands/shutdown.hpp>
#include <metaverse/explorer/extensions/commands/stopmining.hpp>
#include <metaverse/explorer/extensions/commands/startmining.hpp>
#include <metaverse/explorer/extensions/commands/getinfo.hpp>
#include <metaverse/explorer/extensions/commands/getheight.hpp>
#include <metaverse/explorer/extensions/commands/getpeerinfo.hpp>
#include <metaverse/explorer/extensions/commands/getrandom.hpp>
#include <metaverse/explorer/extensions/commands/verifyrandom.hpp>
#include <metaverse/explorer/extensions/commands/getaddressetp.hpp>
#include <metaverse/explorer/extensions/commands/addnode.hpp>
#include <metaverse/explorer/extensions/commands/getmininginfo.hpp>
#include <metaverse/explorer/extensions/commands/getstakeinfo.hpp>
#include <metaverse/explorer/extensions/commands/getblockheader.hpp>
#include <metaverse/explorer/extensions/commands/fetchheaderext.hpp>
#include <metaverse/explorer/extensions/commands/gettx.hpp>
#include <metaverse/explorer/extensions/commands/popblock.hpp>
#include <metaverse/explorer/extensions/commands/dumpkeyfile.hpp>
#include <metaverse/explorer/extensions/commands/importkeyfile.hpp>
#include <metaverse/explorer/extensions/commands/importaccount.hpp>
#include <metaverse/explorer/extensions/commands/importaddress.hpp>
#include <metaverse/explorer/extensions/commands/getnewaccount.hpp>
#include <metaverse/explorer/extensions/commands/getaccount.hpp>
#include <metaverse/explorer/extensions/commands/deleteaccount.hpp>
#include <metaverse/explorer/extensions/commands/listaddresses.hpp>
#include <metaverse/explorer/extensions/commands/getnewaddress.hpp>
#include <metaverse/explorer/extensions/commands/getblock.hpp>
#include <metaverse/explorer/extensions/commands/validateaddress.hpp>
#include <metaverse/explorer/extensions/commands/validatesymbol.hpp>
#include <metaverse/explorer/extensions/commands/listbalances.hpp>
#include <metaverse/explorer/extensions/commands/getbalance.hpp>
#include <metaverse/explorer/extensions/commands/listtxs.hpp>
#include <metaverse/explorer/extensions/commands/listassets.hpp>
#include <metaverse/explorer/extensions/commands/getasset.hpp>
#include <metaverse/explorer/extensions/commands/secondaryissue.hpp>
#include <metaverse/explorer/extensions/commands/getaddressasset.hpp>
#include <metaverse/explorer/extensions/commands/getaccountasset.hpp>
#include <metaverse/explorer/extensions/commands/getassetview.hpp>
#include <metaverse/explorer/extensions/commands/createasset.hpp>
#include <metaverse/explorer/extensions/commands/registermit.hpp>
#include <metaverse/explorer/extensions/commands/transfermit.hpp>
#include <metaverse/explorer/extensions/commands/listmits.hpp>
#include <metaverse/explorer/extensions/commands/getmit.hpp>
#include <metaverse/explorer/extensions/commands/registerdid.hpp>
#include <metaverse/explorer/extensions/commands/send.hpp>
#include <metaverse/explorer/extensions/commands/sendfrom.hpp>
#include <metaverse/explorer/extensions/commands/sendmore.hpp>
#include <metaverse/explorer/extensions/commands/sendasset.hpp>
#include <metaverse/explorer/extensions/commands/sendassetfrom.hpp>
#include <metaverse/explorer/extensions/commands/sendmoreasset.hpp>
#include <metaverse/explorer/extensions/commands/swaptoken.hpp>
#include <metaverse/explorer/extensions/commands/listdids.hpp>
#include <metaverse/explorer/extensions/commands/deletelocalasset.hpp>
#include <metaverse/explorer/extensions/commands/issue.hpp>
#include <metaverse/explorer/extensions/commands/burn.hpp>
#include <metaverse/explorer/extensions/commands/transfercert.hpp>
#include <metaverse/explorer/extensions/commands/issuecert.hpp>
#include <metaverse/explorer/extensions/commands/getwork.hpp>
#include <metaverse/explorer/extensions/commands/submitwork.hpp>
#include <metaverse/explorer/extensions/commands/setminingaccount.hpp>
#include <metaverse/explorer/extensions/commands/changepasswd.hpp>
#include <metaverse/explorer/extensions/commands/getmemorypool.hpp>
#include <metaverse/explorer/extensions/commands/createmultisigtx.hpp>
#include <metaverse/explorer/extensions/commands/createmoremultisigtx.hpp>
#include <metaverse/explorer/extensions/commands/createrawtx.hpp>
#include <metaverse/explorer/extensions/commands/decoderawtx.hpp>
#include <metaverse/explorer/extensions/commands/deletemultisig.hpp>
#include <metaverse/explorer/extensions/commands/getnewmultisig.hpp>
#include <metaverse/explorer/extensions/commands/getpublickey.hpp>
#include <metaverse/explorer/extensions/commands/listmultisig.hpp>
#include <metaverse/explorer/extensions/commands/sendrawtx.hpp>
#include <metaverse/explorer/extensions/commands/signmultisigtx.hpp>
#include <metaverse/explorer/extensions/commands/signrawtx.hpp>
#include <metaverse/explorer/extensions/commands/didchangeaddress.hpp>
#include <metaverse/explorer/extensions/commands/getdid.hpp>
#include <metaverse/explorer/extensions/commands/lock.hpp>
#include <metaverse/explorer/extensions/commands/getlocked.hpp>
#include <metaverse/explorer/extensions/commands/registerwitness.hpp>


namespace libbitcoin {
namespace explorer {

using namespace std;
using namespace commands;

void broadcast_extension(const function<void(shared_ptr<command>)> func, std::ostream& os)
{
    os <<"\r\n";
    // account
    func(make_shared<getnewaccount>());
    func(make_shared<getaccount>());
    func(make_shared<deleteaccount>());
    func(make_shared<importaccount>());
    func(make_shared<changepasswd>());
    func(make_shared<getnewaddress>());
    func(make_shared<validateaddress>());
    func(make_shared<listaddresses>());
    func(make_shared<dumpkeyfile>());
    func(make_shared<importkeyfile>());
    func(make_shared<importaddress>());

    os <<"\r\n";
    // system
    func(make_shared<shutdown>());
    func(make_shared<getinfo>());
    func(make_shared<addnode>());
    func(make_shared<getpeerinfo>());
    func(make_shared<getrandom>());
    func(make_shared<verifyrandom>());

    // miming
    func(make_shared<startmining>());
    func(make_shared<stopmining>());
    func(make_shared<getmininginfo>());
    func(make_shared<getstakeinfo>());
    func(make_shared<setminingaccount>());
    func(make_shared<getwork>());
    func(make_shared<submitwork>());
    func(make_shared<getmemorypool>());
    func(make_shared<registerwitness>());

    os <<"\r\n";
    // block & tx
    func(make_shared<getheight>());
    func(make_shared<getblock>());
    func(make_shared<getblockheader>());
    func(make_shared<fetchheaderext>());
    func(make_shared<gettx>());
    func(make_shared<listtxs>());
    func(make_shared<popblock>());

    // raw tx
    func(make_shared<createrawtx>());
    func(make_shared<decoderawtx>());
    func(make_shared<signrawtx>());
    func(make_shared<sendrawtx>());

    os <<"\r\n";
    // multi-sig
    func(make_shared<getpublickey>());
    func(make_shared<createmultisigtx>());
	func(make_shared<createmoremultisigtx>());
    func(make_shared<getnewmultisig>());
    func(make_shared<listmultisig>());
    func(make_shared<deletemultisig>());
    func(make_shared<signmultisigtx>());

    os <<"\r\n";
    // etp
    func(make_shared<send>());
    func(make_shared<sendmore>());
    func(make_shared<sendfrom>());
    func(make_shared<lock>());
    func(make_shared<listbalances>());
    func(make_shared<getbalance>());
    func(make_shared<getaddressetp>());
    func(make_shared<getlocked>());

    os <<"\r\n";
    // asset
    func(make_shared<validatesymbol>());
    func(make_shared<createasset>());
    func(make_shared<deletelocalasset>());
    func(make_shared<issue>());
    func(make_shared<secondaryissue>());
    func(make_shared<sendasset>());
    func(make_shared<sendmoreasset>());
    func(make_shared<sendassetfrom>());
    func(make_shared<listassets>());
    func(make_shared<getasset>());
    func(make_shared<getaccountasset>());
    // func(make_shared<getassetview>());
    func(make_shared<getaddressasset>());
    func(make_shared<burn>());
    func(make_shared<swaptoken>());

    os <<"\r\n";
    // cert
    func(make_shared<issuecert>());
    func(make_shared<transfercert>());

    os <<"\r\n";
    // mit
    func(make_shared<registermit>());
    func(make_shared<transfermit>());
    func(make_shared<listmits>());
    func(make_shared<getmit>());

    os <<"\r\n";
    //did
    func(make_shared<registerdid>());
    func(make_shared<didchangeaddress>());
    func(make_shared<listdids>());
    func(make_shared<getdid>());
}

shared_ptr<command> find_extension(const string& symbol)
{
    // account
    if (symbol == getnewaccount::symbol())
        return make_shared<getnewaccount>();
    if (symbol == getaccount::symbol())
        return make_shared<getaccount>();
    if (symbol == deleteaccount::symbol())
        return make_shared<deleteaccount>();
    if (symbol == changepasswd::symbol())
        return make_shared<changepasswd>();
    if (symbol == validateaddress::symbol())
        return make_shared<validateaddress>();
    if (symbol == getnewaddress::symbol())
        return make_shared<getnewaddress>();
    if (symbol == listaddresses::symbol())
        return make_shared<listaddresses>();
    if (symbol == importaccount::symbol())
        return make_shared<importaccount>();
    if (symbol == dumpkeyfile::symbol() || symbol == "exportaccountasfile")
        return make_shared<dumpkeyfile>();
    if (symbol == importkeyfile::symbol() || symbol == "importaccountfromfile")
        return make_shared<importkeyfile>();
    if (symbol == importaddress::symbol())
        return make_shared<importaddress>();

    // system
    if (symbol == shutdown::symbol())
        return make_shared<shutdown>();
    if (symbol == getinfo::symbol())
        return make_shared<getinfo>();
    if (symbol == addnode::symbol())
        return make_shared<addnode>();
    if (symbol == getpeerinfo::symbol())
        return make_shared<getpeerinfo>();
    if (symbol == getrandom::symbol())
        return make_shared<getrandom>();
    if (symbol == verifyrandom::symbol())
        return make_shared<verifyrandom>();

    // mining
    if (symbol == stopmining::symbol() || symbol == "stop")
        return make_shared<stopmining>();
    if (symbol == startmining::symbol() || symbol == "start")
        return make_shared<startmining>();
    if (symbol == setminingaccount::symbol())
        return make_shared<setminingaccount>();
    if (symbol == getmininginfo::symbol())
        return make_shared<getmininginfo>();
    if (symbol == getstakeinfo::symbol())
        return make_shared<getstakeinfo>();
    if ((symbol == getwork::symbol()) || (symbol == "eth_getWork"))
        return make_shared<getwork>();
    if ((symbol == submitwork::symbol()) || ( symbol == "eth_submitWork"))
        return make_shared<submitwork>();
    if (symbol == getmemorypool::symbol())
        return make_shared<getmemorypool>();
    if (symbol == registerwitness::symbol())
        return make_shared<registerwitness>();

    // block & tx
    if (symbol == getheight::symbol())
        return make_shared<getheight>();
    if (symbol == "fetch-height")
        return make_shared<getheight>(symbol);
    if (symbol == getblock::symbol())
        return make_shared<getblock>();
    if (symbol == "getbestblockhash")
        return make_shared<getblockheader>(symbol);
    if (symbol == getblockheader::symbol() || symbol == "fetch-header" || symbol == "getbestblockheader")
        return make_shared<getblockheader>();
    if (symbol == fetchheaderext::symbol())
        return make_shared<fetchheaderext>();
    if (symbol == gettx::symbol() || symbol == "gettransaction")
        return make_shared<gettx>();
    if (symbol == popblock::symbol())
        return make_shared<popblock>();
    if (symbol == "fetch-tx")
        return make_shared<gettx>(symbol);
    if (symbol == listtxs::symbol())
        return make_shared<listtxs>();

    // raw tx
    if (symbol == createrawtx::symbol())
        return make_shared<createrawtx>();
    if (symbol == decoderawtx::symbol())
        return make_shared<decoderawtx>();
    if (symbol == signrawtx::symbol())
        return make_shared<signrawtx>();
    if (symbol == sendrawtx::symbol())
        return make_shared<sendrawtx>();

    // multi-sig
    if (symbol == getpublickey::symbol())
        return make_shared<getpublickey>();
    if (symbol == getnewmultisig::symbol())
        return make_shared<getnewmultisig>();
    if (symbol == listmultisig::symbol())
        return make_shared<listmultisig>();
    if (symbol == deletemultisig::symbol())
        return make_shared<deletemultisig>();
    if (symbol == createmultisigtx::symbol())
        return make_shared<createmultisigtx>();
	if (symbol == createmoremultisigtx::symbol())
		return make_shared<createmoremultisigtx>();
    if (symbol == signmultisigtx::symbol())
        return make_shared<signmultisigtx>();

    // etp
    if (symbol == listbalances::symbol())
        return make_shared<listbalances>();
    if (symbol == getbalance::symbol())
        return make_shared<getbalance>();
    if (symbol == getaddressetp::symbol() || symbol == "fetch-balance")
        return make_shared<getaddressetp>();
    if (symbol == lock::symbol())
        return make_shared<lock>();
    if (symbol == getlocked::symbol())
        return make_shared<getlocked>();
    if (symbol == send::symbol() || symbol == "didsend")
        return make_shared<send>();
    if (symbol == sendmore::symbol() || symbol == "didsendmore")
        return make_shared<sendmore>();
    if (symbol == sendfrom::symbol() || symbol == "didsendfrom")
        return make_shared<sendfrom>();

    // asset
    if (symbol == validatesymbol::symbol())
        return make_shared<validatesymbol>();
    if (symbol == createasset::symbol())
        return make_shared<createasset>();
    if (symbol == deletelocalasset::symbol() || symbol == "deleteasset" )
        return make_shared<deletelocalasset>();
    if (symbol == listassets::symbol())
        return make_shared<listassets>();
    if (symbol == getasset::symbol())
        return make_shared<getasset>();
    if (symbol == getaccountasset::symbol())
        return make_shared<getaccountasset>();
    // if (symbol == getassetview::symbol())
    //     return make_shared<getassetview>();
    if (symbol == getaddressasset::symbol())
        return make_shared<getaddressasset>();
    if (symbol == issue::symbol())
        return make_shared<issue>();
    if (symbol == secondaryissue::symbol() || (symbol == "additionalissue") )
        return make_shared<secondaryissue>();
    if (symbol == sendasset::symbol() || symbol == "didsendasset")
        return make_shared<sendasset>();
    if (symbol == sendassetfrom::symbol() || symbol == "didsendassetfrom")
        return make_shared<sendassetfrom>();
    if (symbol == sendmoreasset::symbol() || symbol == "sendassetmore")
        return make_shared<sendmoreasset>();
    if (symbol == burn::symbol())
        return make_shared<burn>();
    if (symbol == swaptoken::symbol())
        return make_shared<swaptoken>();

    // cert
    if (symbol == transfercert::symbol())
        return make_shared<transfercert>();
    if (symbol == issuecert::symbol())
        return make_shared<issuecert>();

    // mit
    if (symbol == registermit::symbol())
        return make_shared<registermit>();
    if (symbol == transfermit::symbol())
        return make_shared<transfermit>();
    if (symbol == listmits::symbol())
        return make_shared<listmits>();
    if (symbol == getmit::symbol())
        return make_shared<getmit>();

    // did
    if (symbol == registerdid::symbol())
        return make_shared<registerdid>();
    if (symbol == didchangeaddress::symbol())
        return make_shared<didchangeaddress>();
    if (symbol == listdids::symbol())
        return make_shared<listdids>();
    if (symbol == getdid::symbol())
        return make_shared<getdid>();

    return nullptr;
}

std::string formerly_extension(const string& former)
{
    return "";
}

} // namespace explorer
} // namespace libbitcoin

