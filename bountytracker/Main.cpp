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
#include <sstream>
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
#include <fstream>
#include "Main.h"
#include <boost/algorithm/string.hpp>

#include "../hookext_plugin/hookext_exports.h"

static int set_iPluginDebug = 0;// 0 = no debug | 1 = all debug
static string MSG_LOG = "-mail.ini";
static const int MAX_MAIL_MSGS = 40;
static const int MAX_BOUNTY_SAVES = 200;
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
	if (fdwReason == DLL_PROCESS_ATTACH)
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
	bool active;
	string lastIssuer;
	string lastTime;
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
	string File_FLHook_bounties = "..\\exe\\flhook_plugins\\bountytracker.bounties.cfg";
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
		}ini.close();
	}
	if(ini.open(File_FLHook_bounties.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("bounty"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("hit"))
					{
						string setTargetName = ini.get_value_string(0);
						wstring theTargetName = ToLower(stows(setTargetName));
						BTI.Char = ToLower(ini.get_value_string(0));
						BTI.Cash = ini.get_value_string(1);
						BTI.xTimes = ini.get_value_string(2);
						BTI.issuer = ToLower(ini.get_value_string(3));
						BTI.active = ini.get_value_bool(4);
						BTI.lastIssuer = ToLower(ini.get_value_string(5));
						BTI.lastTime = ini.get_value_string(6);
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
//deletes a bounty from the cfg
bool deleteBountyCfg(BountyTargetInfo BTI)
{
	string sSafe;
	string sActive;
	string File_FLHook_tmp = "..\\exe\\flhook_plugins\\bountytracker.bounties.cfg";
	ifstream tmpCfg(File_FLHook_tmp);
	vector<string> file;
	string temp;//buffer for getline()
	if (!tmpCfg.is_open())
	{
		return false;
	}
	while (!tmpCfg.eof())
	{
		getline(tmpCfg, temp);
		file.push_back(temp);
	}
	// done reading file
	tmpCfg.close();

	if (BTI.active)
	{
		sActive = "true";
	}
	if (!BTI.active)
	{
		sActive = "false";
	}


	string item = "hit = " + BTI.Char + "," + BTI.Cash + "," + BTI.xTimes + "," + BTI.issuer + "," + sActive + "," + BTI.lastIssuer + "," + BTI.lastTime;//find this
	for (int i = 0; i < (int)file.size(); ++i)
	{
		///ConPrint(L"Debug: searching for " + stows(item));
		if (file[i].substr(0, item.length()) == item)
		{
			file.erase(file.begin() + i);
			///ConPrint(L"Debug: deleted line " + stows(item) + L" at i = " + stows(itos(i)) + L"\n");
			i = 0; // Reset search
		}
	}
	ofstream out(File_FLHook_tmp, ios::out | ios::trunc);
	if (!out.is_open())
	{
		return false;
	}
	for (vector<string>::const_iterator i = file.begin(); i != file.end(); ++i)
	{
		out << *i << endl;
	}
	out.close();

	return true;
}
//only used when server is able to handle the file write lag - we'll see how much lag this induces
bool appendBountyCfg(BountyTargetInfo BTI)
{
	string sSafe;
	string sActive;
	string File_FLHook_tmp = "..\\exe\\flhook_plugins\\bountytracker.bounties.cfg";
	ofstream tmpCfg;
	tmpCfg.open(File_FLHook_tmp, ios::out | ios::app);
	if (!tmpCfg.is_open())
	{
		return false;
	}
	if (BTI.active)
	{
		sActive = "true";
	}
	if (!BTI.active)
	{
		sActive = "false";
	}
	tmpCfg << endl << "hit = " << BTI.Char << "," << BTI.Cash << "," << BTI.xTimes << "," << BTI.issuer << "," << sActive << "," << BTI.lastIssuer << "," << BTI.lastTime;
	tmpCfg.close();
	return true;
}

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

bool UserCmd_BountyAdd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"BountyTracker is disabled.");
		return true;
	}
	

	// Get the parameters from the user command.
	wstring wscName = GetParam(wscParam, L' ', 0);
	wstring wscCash = GetParam(wscParam, L' ', 1);
	wstring wscxTimes = GetParam(wscParam, L' ', 2);
	wscCash = ReplaceStr(wscCash, L".", L"");
	wscCash = ReplaceStr(wscCash, L",", L"");
	wscCash = ReplaceStr(wscCash, L"$", L"");
	wscCash = ReplaceStr(wscCash, L"e6", L"000000");//because scientific notation is cool
	int iOnlineSecs;
	HkGetOnLineTime((wchar_t*)Players.GetActiveCharacterName(iClientID), iOnlineSecs);
	if (iOnlineSecs < 7200)// 7200 = 2hrs
	{
		PrintUserCmdText(iClientID, L"ERR Char is too new");
		return true;
	}
	
	//you are not allowed to create a bounty. ERR rank too low (note, find out what a good rank should be to have access to this. no fresh chars can
	//create bounties. this way we can protect against creating random fresh accs, tranferring cash, and setting copious amounts of bounties.
	if (wscName == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid name\n");
		return false;
	}
	if (HkGetAccountByCharname(wscName) == 0)
	{
		PrintUserCmdText(iClientID, L"ERR Player does not exist");
		return true;
	}
	if (mapBountyTargets[ToLower(wscName)].active)
	{
		PrintUserCmdText(iClientID, L"ERR Player already has an active bounty\n");
		return true;
	}
	if (mapBountyTargets[ToLower(wscName)].lastTime != "")
	{
		if ((stoi(mapBountyTargets[ToLower(wscName)].lastTime) + 3600) > (int)time(0))
			{
				PrintUserCmdText(iClientID, L"ERR Player is protected\n");
				PrintUserCmdText(iClientID, stows(itos((stoi(mapBountyTargets[ToLower(wscName)].lastTime) + 3600) - (int)time(0))) + L"'s remaining");
				return true;
			}
	}
	
	if (iClientID == HkGetClientIdFromCharname(stows(mapBountyTargets[ToLower(wscName)].lastIssuer)))//not too sure about this
	{
		PrintUserCmdText(iClientID, L"ERR You cannot double a bounty on this player\n");
		return true;
	}
	if (wscCash == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid cash amount\n");
		return false;
	}
	if (wscxTimes == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid contract limit\n");
		return false;
	}
	if (stoi(wscCash) < 1000000)
	{
		PrintUserCmdText(iClientID, L"ERR bounty cannot be less than 1,000,000 s.c");
		return true;
	}
	if (stoi(wscxTimes) < 0 || stoi(wscxTimes) > 5)
	{
		PrintUserCmdText(iClientID, L"ERR bounty contract limit cannot be less than 0 or more than 5");
		return true;
	}
	BountyTargetInfo BTIa = mapBountyTargets[ToLower(wscName)];
	//generate new bounty map values
	BTIa.Char = ToLower(wstos(wscName));
	BTIa.Cash = wstos(wscCash);
	BTIa.xTimes = wstos(wscxTimes);
	BTIa.issuer = ToLower(wstos((wchar_t*)Players.GetActiveCharacterName(iClientID)));
	BTIa.lastIssuer = BTIa.issuer;
	BTIa.active = true;
	BTIa.lastTime = "";

	//check user has enough money for the bounty
	int iCash;
	HkGetCash(stows(BTIa.issuer), iCash);
	if (iCash < (stoi(BTIa.Cash) * stoi(BTIa.xTimes)))
	{
		PrintUserCmdText(iClientID, L"ERR Not enough cash for bounty.");
		return true;
	}
	HkAddCash((wchar_t*)Players.GetActiveCharacterName(iClientID), 0 - (stoi(BTIa.Cash) * stoi(BTIa.xTimes)));
	PrintUserCmdText(iClientID, L"Uploading to Neural Net...");
	mapBountyTargets[ToLower(wscName)] = BTIa;

	wstring PFwsTargetInfo;
	PFwsTargetInfo = L"Target: ";
	PFwsTargetInfo += wscName;
	PFwsTargetInfo += L" Worth: ";
	PFwsTargetInfo += stows(BTIa.Cash);
	PFwsTargetInfo += L" Contracts Left: ";
	PFwsTargetInfo += stows(BTIa.xTimes);
	PFwsTargetInfo += L" Issuer: ";
	PFwsTargetInfo += stows(BTIa.issuer);
	PrintUserCmdText(iClientID, PFwsTargetInfo);

	if (appendBountyCfg(BTIa))
	{
		ConPrint(L"cfg saved\n");
	}
	else
	{
		ConPrint(L"Err saving to cfg\n");
	}
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_BountyView(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"BountyTracker is disabled.");
		return true;
	}

	// Get the parameters from the user command.
	wstring wscName = GetParam(wscParam, L' ', 0);
	wstring wscCash = GetParam(wscParam, L' ', 1);
	wstring wscxTimes = GetParam(wscParam, L' ', 2);

	if (wscName == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid Parameters\n");
		return false;
	}
	if (HkGetAccountByCharname(wscName) == 0)
	{
		PrintUserCmdText(iClientID, L"ERR Player does not exist");
		return true;
	}
	BountyTargetInfo BTIv = mapBountyTargets[ToLower(wscName)];
	wstring PFwsTargetInfo;
	PFwsTargetInfo = L"Target: ";
	PFwsTargetInfo += ToLower(wscName);
	PFwsTargetInfo += L" Worth: ";
	PFwsTargetInfo += stows(BTIv.Cash);
	PFwsTargetInfo += L" Contracts Left: ";
	PFwsTargetInfo += stows(BTIv.xTimes);
	PFwsTargetInfo += L" Issuer: ";
	PFwsTargetInfo += stows(BTIv.issuer);
	PrintUserCmdText(iClientID, PFwsTargetInfo);
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_BountyHelp(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	PrintUserCmdText(iClientID, L"Usage: /bounty add <target> <cash> <xContracts>\n");
	PrintUserCmdText(iClientID, L"Usage: /bounty addto <target> <cash>\n");
	PrintUserCmdText(iClientID, L"Usage: /bounty view <target>\n");
	return true;
}
bool UserCmd_BountyAddTo(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"BountyTracker is disabled.");
		return true;
	}

	// Get the parameters from the user command.
	wstring wscName = GetParam(wscParam, L' ', 0);
	wstring wscCash = GetParam(wscParam, L' ', 1);
	wstring wscxTimes = GetParam(wscParam, L' ', 2);
	wscCash = ReplaceStr(wscCash, L".", L"");
	wscCash = ReplaceStr(wscCash, L",", L"");
	wscCash = ReplaceStr(wscCash, L"$", L"");
	wscCash = ReplaceStr(wscCash, L"e6", L"000000");

	if (wscName == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid name\n");
		return false;
	}
	if (HkGetAccountByCharname(wscName) == 0)
	{
		PrintUserCmdText(iClientID, L"ERR Player does not exist");
		return true;
	}
	if (wscCash == L"")
	{
		PrintUserCmdText(iClientID, L"ERR invalid cash amount\n");
		return false;
	}
	if (stoi(wscCash) < 1000000)
	{
		PrintUserCmdText(iClientID, L"ERR bounty cannot be less than 1,000,000 s.c");
		return true;
	}
	//get bounty 
	BountyTargetInfo BTIat = mapBountyTargets[ToLower(wscName)];
	//check if it is active
	if (!BTIat.active)
	{
		PrintUserCmdText(iClientID, L"ERR bounty not currently active");
		return true;
	}
	//check user has enough money for the bounty
	int iCash;
	HkGetCash(stows(ToLower(wstos((wchar_t*)Players.GetActiveCharacterName(iClientID)))), iCash);
	if (iCash < (stoi(wscCash) * stoi(BTIat.xTimes)))
	{
		PrintUserCmdText(iClientID, L"ERR Not enough cash for bounty.");
		return true;
	}
	HkAddCash((wchar_t*)Players.GetActiveCharacterName(iClientID), 0 - (stoi(wscCash) * stoi(BTIat.xTimes)));
	if (deleteBountyCfg(BTIat))
	{
		//ConPrint(L"bounty removed from cfg\n");
	}
	else
	{
		ConPrint(L"BOUNTYTRACKER: Err removing from cfg. is server admin?\n");
	}
	BTIat.Cash = itos(stoi(BTIat.Cash) + stoi(wscCash));//update cash bounty
	PrintUserCmdText(iClientID, L"Uploading to Neural Net...");
	mapBountyTargets[ToLower(wscName)] = BTIat;
	if (appendBountyCfg(BTIat))
	{
		//ConPrint(L"cfg saved\n");
	}
	else
	{
		ConPrint(L"BOUNTYTRACKER: Err saving to cfg. is serevr admin?\n");
	}
	PrintUserCmdText(iClientID, L"OK");
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
			if (BTId.Char == wstos(wscDestroyedName) && stoi(BTId.xTimes) > 0 && BTId.active)
			{
				if (deleteBountyCfg(BTId))
				{
					///ConPrint(L"bounty removed from cfg\n");
				}
				else
				{
					ConPrint(L"BOUNTYTRACKER: Err removing from cfg. is server admin?\n");
				}
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
				if (iKillerID == iDestroyedID)
				{
					///ConPrint(L"killer and victim are the same");
					return;
				}
				if (ToLower(wscKillerName) == stows(BTId.issuer))
				{
					///ConPrint(L"killer was the issuer of the bounty");
					return;
				}
				// -1 to contracts left
				BTId.xTimes = itos(stoi(BTId.xTimes) - 1);
				if (stoi(BTId.xTimes) == 0)
				{
					//bounty has been fullfilled, clear all dataa
					BTId.active = false;
					BTId.Cash = "0";
					BTId.xTimes = "0";
					BTId.issuer = "n/a";
					BTId.lastTime = itos((int)time(0));
				}

				//upload into neural net
				mapBountyTargets[wscDestroyedName] = BTId;
				if (appendBountyCfg(BTId))
				{
					///ConPrint(L"cfg saved\n");
				}
				else
				{
					ConPrint(L"BOUNTYTRACKER: Err saving to cfg. is serevr admin?\n");
				}
				
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
				MailSend(wscKillerName, MSG_LOG, KillerMailMsg);
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

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/bounty add", UserCmd_BountyAdd, L"Usage: /bounty add <target> <cash> <xtimes>" },
	{ L"/bounty addto", UserCmd_BountyAddTo, L"Usage: /bounty addto <target> <cash>" },
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
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd, wscParam, UserCmds[i].usage))
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
	p_PI->sShortName = "bountytracker";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));

	return p_PI;
}
