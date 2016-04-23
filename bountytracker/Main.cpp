// BountyTracker for FLHookPlugin v3 vc14
// April 2016 by RamRawR
//
// This is a bounty tracking plugin that allows hunters to be auto paid,
// and makes it easier for the issuer to not worry about cash transfers.
//
// very messy, and probably badly written; expect bugs and such.
// config files are not updated, any settings in config stick
// when server restarts / plugin is loaded. Virtual bounties
// work fine however.
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <limits.h>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"
#include <boost/algorithm/string.hpp>

#include "../hookext_plugin/hookext_exports.h"

static int set_iPluginDebug = 0;// 0 = no debug | 1 = all debug
static string MSG_LOG = "-mail.ini";
static const int MAX_MAIL_MSGS = 40;
bool set_bLocalTime = false;
bool bPluginEnabled = true;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

struct BountyTargetInfo {
	string Char;
	string Cash;
	string xTimes;
	string issuer;
};
BountyTargetInfo BTI;
static map<wstring, BountyTargetInfo> mapBountyTargets;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = "..\\exe\\flhook_plugins\\bountytracker.cfg";
	int iLoaded = 0;

	INI_Reader ini;
	if (ini.open(File_FLHook.c_str(), false))
	{
		while (ini.read_header())
			{
				if (ini.is_header("config"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("enabled"))
						{
							bPluginEnabled = ini.get_value_bool(0);
						}
					}				
				}
				if (ini.is_header("bounty"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("hit"))
						{
							string setTargetName = ini.get_value_string(0);
							wstring theTargetName = stows(setTargetName);
							BTI.Char = ToLower(ini.get_value_string(0));
							BTI.Cash = ini.get_value_string(1);
							BTI.xTimes = ini.get_value_string(2);
							BTI.issuer = ini.get_value_string(3);
							mapBountyTargets[theTargetName] = BTI;
							++iLoaded;
						}
					}
				}
			}
		ini.close();
	}

	ConPrint(L"BOUNTYTRACKER: Loaded %u Bounties\n", iLoaded);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*updates map and config files of changes to BTI NOTE: not in use
void UpdateBountyTarget(wstring wscTargetName, BountyTargetInfo BTI) {
	//first update current map
	
	//next shove it into the cfg
	return;
}
*/
/* copy pasta from playercntl as to provide independance*/
string GetUserFilePath(const wstring &wscCharname, const string &scExtension)
{
	// init variables
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath);
	string scAcctPath = string(szDataPath) + "\\Accts\\MultiPlayer\\";

	wstring wscDir;
	wstring wscFile;
	if (HkGetAccountDirName(wscCharname, wscDir) != HKE_OK)
		return "";
	if (HkGetCharFileName(wscCharname, wscFile) != HKE_OK)
		return "";

	return scAcctPath + wstos(wscDir) + "\\" + wstos(wscFile) + scExtension;
}
/* copy pasta from playercntl as to provide independance*/
bool MailSend(const wstring &wscCharname, const string &scExtension, const wstring &wscMsg)
{
	// Get the target player's message file.
	string scFilePath = GetUserFilePath(wscCharname, scExtension);
	if (scFilePath.length() == 0)
		return false;

	// Move all mail up one slot starting at the end. We automatically
	// discard the oldest messages.
	for (int iMsgSlot = MAX_MAIL_MSGS - 1; iMsgSlot>0; iMsgSlot--)
	{
		wstring wscTmpMsg = IniGetWS(scFilePath, "Msgs", itos(iMsgSlot), L"");
		IniWriteW(scFilePath, "Msgs", itos(iMsgSlot + 1), wscTmpMsg);

		bool bTmpRead = IniGetB(scFilePath, "MsgsRead", itos(iMsgSlot), false);
		IniWrite(scFilePath, "MsgsRead", itos(iMsgSlot + 1), (bTmpRead ? "yes" : "no"));
	}

	// Write message into the slot
	IniWriteW(scFilePath, "Msgs", "1", GetTimeString(set_bLocalTime) + L" " + wscMsg);
	IniWrite(scFilePath, "MsgsRead", "1", "no");
	return true;
}

bool UserCmd_BountyAdd(uint iClientID, const wstring &wscCmd, const wstring &wscName, const wstring &wscCash, const wstring &wscxTimes, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"BountyTracker is disabled.");
		return true;
	}
	if (wscName == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid name\n");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	if (wscCash == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid cash amount\n");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	if (wscxTimes == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid contract limit\n");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	if (HkGetClientIdFromCharname(wscName) == -1)
	{
		PrintUserCmdText(iClientID, L"ERR Player not logged in");
		return true;
	}
	if (stoi(wscCash) < 0 )
	{
		PrintUserCmdText(iClientID, L"ERR bounty cannot be less than 0");
		return true;
	}
	if (stoi(wscxTimes) < 0 )
	{
		PrintUserCmdText(iClientID, L"ERR bounty contract limit cannot be less than 0");
		return true;
	}
	//generate new bounty map values
	BTI.Char = ToLower(wstos(wscName));
	BTI.Cash = wstos(wscCash);
	BTI.xTimes = wstos(wscxTimes);
	BTI.issuer = wstos((wchar_t*)Players.GetActiveCharacterName(iClientID));
	//check user has enough money for the bounty
	int iCash;
	HkGetCash(stows(BTI.issuer), iCash);
	if (iCash < (stoi(BTI.Cash) * stoi(BTI.xTimes)))
	{
		PrintUserCmdText(iClientID, L"ERR Not enough cash for bounty.");
		return true;
	}
	HkAddCash((wchar_t*)Players.GetActiveCharacterName(iClientID), 0-(stoi(BTI.Cash) * stoi(BTI.xTimes)));
	PrintUserCmdText(iClientID, L"Uploading to Neural Net...");

	wstring PFwsTargetInfo;
	PFwsTargetInfo = L"Target: ";
	PFwsTargetInfo += wscName;
	PFwsTargetInfo += L" Worth: ";
	PFwsTargetInfo += stows(BTI.Cash);
	PFwsTargetInfo += L" Contracts Left: ";
	PFwsTargetInfo += stows(BTI.xTimes);
	PFwsTargetInfo += L" Issuer: ";
	PFwsTargetInfo += stows(BTI.issuer);
	PrintUserCmdText(iClientID, PFwsTargetInfo);

	mapBountyTargets[ToLower(wscName)] = BTI;
	
	///UpdateBountyTarget(wscName, BTI);//sets the values into the map and update cfg	
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_BountyView(uint iClientID, const wstring &wscCmd, const wstring &wscName, const wstring &wscParam2, const wstring &wscParam3, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"BountyTracker is disabled.");
		return true;
	}
	if (wscName == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid Parameters\n");
		PrintUserCmdText(iClientID, usage);
		return false;
	}
	wstring wscTargetName = ToLower(wscName);
	BountyTargetInfo BTITargetInfo = mapBountyTargets[wscTargetName];
	wstring PFwsTargetInfo;
	PFwsTargetInfo = L"Target: ";
	PFwsTargetInfo += wscTargetName;
	PFwsTargetInfo += L" Worth: ";
	PFwsTargetInfo += stows(BTITargetInfo.Cash);
	PFwsTargetInfo += L" Contracts Left: ";
	PFwsTargetInfo += stows(BTITargetInfo.xTimes);
	PFwsTargetInfo += L" Issuer: ";
	PFwsTargetInfo += stows(BTITargetInfo.issuer);
	PrintUserCmdText(iClientID, PFwsTargetInfo);
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_BountyHelp(uint iClientID, const wstring &wscCmd, const wstring &wscName, const wstring &wscCash, const wstring &wscxTimes, const wchar_t *usage)
{
	PrintUserCmdText(iClientID, L"Usage: /bounty add <target> <cash> <xContracts>\n");
	PrintUserCmdText(iClientID, L"Usage: /bounty view <target>\n");
	return true;
}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;
	if (iKill)
	{
		CShip *cship = (CShip*)ecx[4];
		//check the death was a player
		if (cship->is_player())
		{
			uint iDestroyedID = cship->GetOwnerPlayer();
			wstring wscDestroyedName = ToLower((wchar_t*)Players.GetActiveCharacterName(iDestroyedID));
			//check if they have a bounty
			BountyTargetInfo BTId = mapBountyTargets[wscDestroyedName];
			if (BTId.Char == wstos(wscDestroyedName) && stoi(BTId.xTimes) > 0)
			{
				// calls the killer the last one to damage the victim
				DamageList dmg;
				try { dmg = *_dmg; }
				catch (...) { return; }
				dmg = ClientInfo[iDestroyedID].dmgLast;

				//The killer's id
				uint iKillerID = HkGetClientIDByShip(dmg.get_inflictor_id());
				//The killer's name
				wstring wscKillerName = (wchar_t*)Players.GetActiveCharacterName(iKillerID);

				//check if player killed by ai
				if (stoi(wstos(HkGetAccountIDByClientID(iKillerID))) == -1)
				{
					///ConPrint(L"nevermind, ai got him");
					return;
				}

				// -1 to contracts left
				BTId.xTimes = itos(stoi(BTId.xTimes) - 1);

				//upload into neural net
				mapBountyTargets[wscDestroyedName] = BTId;
				///UpdateBountyTarget(wscDestroyedName, BTId);

				//Print Friendly Wide String TargetInfo
				wstring PFwsTargetInfo;
				PFwsTargetInfo = L"Target: ";
				PFwsTargetInfo += wscDestroyedName;
				PFwsTargetInfo += L" Worth: ";
				PFwsTargetInfo += stows(BTId.Cash);
				PFwsTargetInfo += L" Contracts Left: ";
				PFwsTargetInfo += stows(BTId.xTimes);
				PFwsTargetInfo += L" Issuer: ";
				PFwsTargetInfo += stows(BTId.issuer);

				//add bounty cash
				HkAddCash(wscKillerName, stoi(BTId.Cash));
				PrintUserCmdText(iKillerID, L"Successfully collected bounty on");
				PrintUserCmdText(iKillerID, PFwsTargetInfo);
				PrintUserCmdText(iKillerID, L"Alerting bounty issuer...");
				wstring IssuerMailMsg = L"Bounty Alert: " + wscKillerName + L" Has collected your bounty on " + wscDestroyedName + L". " + stows(BTId.xTimes) + L" Contracts remaining.";
				MailSend(stows(BTId.issuer), MSG_LOG, IssuerMailMsg);
				PrintUserCmdText(iKillerID, L"Saving record...");
				wstring KillerMailMsg = L"Bounty Alert: " + wscKillerName + L" Has collected a bounty on " + wscDestroyedName + L". " + stows(BTId.xTimes) + L" Contracts remaining.";
				MailSend(stows(BTId.issuer), MSG_LOG, KillerMailMsg);
				PrintUserCmdText(iKillerID, L"OK");
				return;
			}
			else
			{
				///ConPrint(L"bounty tracker found no name match, or no more contracts for name\n");//either the name didn't match, or they have exhaused the number of contracts left on them.
			}
		}
		else
		{
			///ConPrint(L"Bounty tracker found dead ai\n");//never mind, it's just an ai death
		}
	}
	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/bounty add", UserCmd_BountyAdd, L"Usage: /bounty add <target> <cash> <xtimes>" },
	{ L"/bounty view", UserCmd_BountyView, L"Usage: /bounty view <target>" },
	{ L"/bounty help", UserCmd_BountyHelp, L"Usage: /bounty help" },
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	// If the chat string does not match the USER_CMD then we do not handle the
	// command, so let other plugins or FLHook kick in. We require an exact match
	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{

		if (wscCmdLineLower.find(UserCmds[i].wszCmd) == 0)
		{
			// Extract the parameters string from the chat string. It should
			// be immediately after the command and a space.
			wstring wscParam = L"";
			std::vector<std::wstring> tok;
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
				split(tok, wscParam, boost::is_any_of(L" "));
			}
			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd, tok[0], tok[1], tok[2], UserCmds[i].usage))
			{
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
			else {
				PrintUserCmdText(iClientID, UserCmds[i].usage);
				return true;
			}
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "BountyTracker by RamRawR";
	p_PI->sShortName = "bounty";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));

	return p_PI;
}
