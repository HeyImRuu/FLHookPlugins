// ShipPassword
// August 2016 by RamRawR
//
// This is a template with the bare minimum to have a functional plugin.
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
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"

#include "../hookext_plugin/hookext_exports.h"

static int set_iPluginDebug = 0;

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

bool bPluginEnabled = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = "..\\exe\\flhook_plugins\\shippassword.cfg";
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
			}
		ini.close();
	}

	ConPrint(L"ShipPass: Loaded\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Data
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ProtectedChar {
	string charname;
	string mstrpass;
	string usrpass;
	bool locked;
	string timer;

};

ProtectedChar PChar;
static map<wstring, ProtectedChar> mapProtectedChars;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool UserCmd_SetMasterPass(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"ShipPassword is disabled.");
		return true;
	}
	// Get the parameters from the user command.
	wstring wscChar = GetParam(wscParam, L' ', 0);
	wstring wscPass = GetParam(wscParam, L' ', 1);
	if (wscChar == L"" || wscPass == L"") {
		return false;
	}

	wstring charname = (wchar_t*)Players.GetActiveCharacterName(iClientID);

	if (wscChar == charname) {
		if (mapProtectedChars[wscChar].mstrpass.empty()) {
			PChar.charname = wstos(wscChar);
			PChar.mstrpass = wstos(wscPass);
			PChar.locked = true;
			mapProtectedChars[wscChar] = PChar;

			wstring msg = L"Master Password for " + wscChar + L" set & locked.";
			PrintUserCmdText(iClientID, msg);
		}
		else {
			PrintUserCmdText(iClientID, L"Char already has master password.");
		}
	}
	else {
		PrintUserCmdText(iClientID, L"Must log char to set master password.");
	}
	
	
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_SetUserPass(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"ShipPassword is disabled.");
		return true;
	}
	// Get the parameters from the user command.
	wstring wscChar = GetParam(wscParam, L' ', 0);
	wstring wscPass = GetParam(wscParam, L' ', 1);
	wstring wscMstrPass = GetParam(wscParam, L' ', 2);

	if (wscChar == L"" || wscPass == L"") {
		return false;
	}
	if (mapProtectedChars[wscChar].mstrpass == wstos(wscMstrPass) && !wscMstrPass.empty()) {
		PChar.charname = wstos(wscChar);
		PChar.usrpass = wstos(wscPass);
		PChar.locked = true;
		mapProtectedChars[wscChar] = PChar;
		
		wstring msg = L"User Password for " + wscChar + L" set & locked.";
		PrintUserCmdText(iClientID, msg);
	}
	
	else {
		PrintUserCmdText(iClientID, L"Either your master password is wrong or has not been set yet.");
	}
	
	
	PrintUserCmdText(iClientID, L"OK");
	return true;
}
bool UserCmd_UnlockChar(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"ShipPassword is disabled.");
		return true;
	}
	// Get the parameters from the user command.
	wstring wscChar = GetParam(wscParam, L' ', 0);
	wstring wscPass = GetParam(wscParam, L' ', 1);

	if (wscChar == L"" || wscPass == L"") {
		return false;
	}

	if (mapProtectedChars[wscChar].usrpass == wstos(wscPass)) {
		mapProtectedChars[wscChar].timer = time(0);
		mapProtectedChars[wscChar].locked = false;
		PrintUserCmdText(iClientID, L"Char unlocked");
	}

	PrintUserCmdText(iClientID, L"OK");
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////



void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID) {
	wstring charname = (wchar_t*)Players.GetActiveCharacterName(iClientID);
	//ConPrint(L"hi");
	//ConPrint(charname);
	if (mapProtectedChars[charname].charname == wstos(charname)) {
		if (mapProtectedChars[charname].locked) {
			PrintUserCmdText(iClientID, L"This ship is password protected.");
			HkKick(charname);
			//ConPrint(L"hieffff");
			return;
		}
		else {
			mapProtectedChars[charname].locked = true;
			PrintUserCmdText(iClientID, L"Locking Char.");
			//ConPrint(L"hieaaaad");
			return;
		}
	}
	
	//ConPrint(L"hiend");
	return;
}

void UserCmd_Help(uint iClientID, const wstring &wscParam) {
	returncode = DEFAULT_RETURNCODE;
	PrintUserCmdText(iClientID, L"/setmasterpass");
	PrintUserCmdText(iClientID, L"/setuserpass");
	PrintUserCmdText(iClientID, L"/unlockchar");
}

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
	{ L"/setmasterpass", UserCmd_SetMasterPass, L"Usage: /setmasterpass <char> <mstrpass>" },
	{ L"/setuserpass", UserCmd_SetUserPass, L"Usage: /setpass <char> <usrpass> <mstrpass>" },
	{ L"/unlockchar", UserCmd_UnlockChar, L"Usage: /unlockchar <char> <usrpass>" },
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
	p_PI->sName = "ShipPassword";
	p_PI->sShortName = "ShipPass";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Help, PLUGIN_UserCmd_Help, 0));

	return p_PI;
}
