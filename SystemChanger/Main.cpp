// System Changer (system mover) Plugin vc14 hook ver 3
// April 2016 by RamRawR
//
// This is a test plugin that enables the client to teleport across systems, 
// keeping their position and orientation.
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

bool bPluginEnabled;
vector<string> bPluginAllowedSystems;
string bPluginAllowedSystemsFriendly;

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix ornt;
};
static map<uint, DEFERREDJUMPS> mapDeferredJumps;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = "..\\exe\\flhook_plugins\\changesys.cfg";
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
			if (ini.is_header("AllowedSystems"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("sys"))
					{
						bPluginAllowedSystems.push_back(ini.get_value_string(0));
					}
					if (ini.is_value("desc"))
					{
						bPluginAllowedSystemsFriendly = ini.get_value_string();
					}
				}
			}
		}
		ini.close();
	}

	ConPrint(L"ChangeSys: Loaded\n", iLoaded);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt)
{
	mapDeferredJumps[iClientID].system = system;
	mapDeferredJumps[iClientID].pos = pos;
	mapDeferredJumps[iClientID].ornt = ornt;

	// Force a launch to put the ship in the right location in the current system so that
	// when the change system command arrives (hopefully) a fraction of a second later
	// the ship will appear at the right location.
	HkRelocateClient(iClientID, pos, ornt);
	// Send the jump command to the client. The client will send a system switch out complete
	// event which we intercept to set the new starting positions.
	PrintUserCmdText(iClientID, L" ChangeSys %u", system);
}

bool AdminCmd_ChangeSystem(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	wstring wscArg = ToLower(GetParam(wscParam, ' ', 0));
	//check master switch bruh
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"ChangeSys is disabled.\n");
		return true;
	}
	if (wscArg.length()==0)
	{
		PrintUserCmdText(iClientID, L"ERR Incorrect Parameters <sys|list>");
		return true;
	}
	//check what the parameters are
	if (wcscmp(wscArg.c_str(), L"list") == 0)
	{
		//show list of systems
		PrintUserCmdText(iClientID, L"list of systems: ");
		/*for (vector<string>::iterator it = bPluginAllowedSystems.begin(); it != bPluginAllowedSystems.end(); ++it) 
		{
			PrintUserCmdText(iClientID, stows(*it));
		}*/
		PrintUserCmdText(iClientID, stows(bPluginAllowedSystemsFriendly));
		return true;
	}
	//assume player is trying to enter a system id
	uint ship;
	pub::Player::GetShip(iClientID, ship);
	if (!ship)
	{
		PrintUserCmdText(iClientID, L"ERR Not in space\n");
		return true;
	}
	
	//check if valid sys id from config vector
	if(find(bPluginAllowedSystems.begin(), bPluginAllowedSystems.end(), wstos(wscArg)) != bPluginAllowedSystems.end())
	{ 
		//its a match buddy!
		//do tha magic
		//get system id for server
		uint iTargetSystem = CreateID(wstos(wscArg).c_str());
		Vector pos;
		Matrix ornt;
		//get the pos and ornt of ship
		pub::SpaceObj::GetLocation(ship, pos, ornt);
		//move player
		SwitchSystem(iClientID, iTargetSystem, pos, ornt);
		PrintUserCmdText(iClientID, L"Moving to system: ");
		PrintUserCmdText(iClientID, wscArg.c_str());
		PrintUserCmdText(iClientID, L"OK\n");
		//done magic
	}
	else
	{
		//well fuck
		PrintUserCmdText(iClientID, L"ERR Invalid system id: ");
		PrintUserCmdText(iClientID, wscArg.c_str());
	}

	return true;
}

//called by trigger
bool SystemSwitchOutComplete_before(unsigned int iShip, unsigned int iClientID)
{
	static PBYTE SwitchOut = 0;
	if (!SwitchOut)
	{
		SwitchOut = (PBYTE)hModServer + 0xf600;

		DWORD dummy;
		VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
	}

	// Patch the system switch out routine to put the ship in a
	// system of our choosing.
	if (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end())
	{
		SwitchOut[0x0d7] = 0xeb;				// ignore exit object
		SwitchOut[0x0d8] = 0x40;
		SwitchOut[0x119] = 0xbb;				// set the destination system
		*(PDWORD)(SwitchOut + 0x11a) = mapDeferredJumps[iClientID].system;
		SwitchOut[0x266] = 0x45;				// don't generate warning
		*(float*)(SwitchOut + 0x2b0) = mapDeferredJumps[iClientID].pos.z;		// set entry location
		*(float*)(SwitchOut + 0x2b8) = mapDeferredJumps[iClientID].pos.y;
		*(float*)(SwitchOut + 0x2c0) = mapDeferredJumps[iClientID].pos.x;
		*(float*)(SwitchOut + 0x2c8) = mapDeferredJumps[iClientID].ornt.data[2][2];
		*(float*)(SwitchOut + 0x2d0) = mapDeferredJumps[iClientID].ornt.data[1][1];
		*(float*)(SwitchOut + 0x2d8) = mapDeferredJumps[iClientID].ornt.data[0][0];
		*(float*)(SwitchOut + 0x2e0) = mapDeferredJumps[iClientID].ornt.data[2][1];
		*(float*)(SwitchOut + 0x2e8) = mapDeferredJumps[iClientID].ornt.data[2][0];
		*(float*)(SwitchOut + 0x2f0) = mapDeferredJumps[iClientID].ornt.data[1][2];
		*(float*)(SwitchOut + 0x2f8) = mapDeferredJumps[iClientID].ornt.data[1][0];
		*(float*)(SwitchOut + 0x300) = mapDeferredJumps[iClientID].ornt.data[0][2];
		*(float*)(SwitchOut + 0x308) = mapDeferredJumps[iClientID].ornt.data[0][1];
		*(PDWORD)(SwitchOut + 0x388) = 0x03ebc031;		// ignore entry object
		mapDeferredJumps.erase(iClientID);
		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
		Server.SystemSwitchOutComplete(iShip, iClientID);
		SwitchOut[0x0d7] = 0x0f;
		SwitchOut[0x0d8] = 0x84;
		SwitchOut[0x119] = 0x87;
		*(PDWORD)(SwitchOut + 0x11a) = 0x1b8;
		*(PDWORD)(SwitchOut + 0x25d) = 0x1cf7f;
		SwitchOut[0x266] = 0x1a;
		*(float*)(SwitchOut + 0x2b0) =
			*(float*)(SwitchOut + 0x2b8) =
			*(float*)(SwitchOut + 0x2c0) = 0;
		*(float*)(SwitchOut + 0x2c8) =
			*(float*)(SwitchOut + 0x2d0) =
			*(float*)(SwitchOut + 0x2d8) = 1;
		*(float*)(SwitchOut + 0x2e0) =
			*(float*)(SwitchOut + 0x2e8) =
			*(float*)(SwitchOut + 0x2f0) =
			*(float*)(SwitchOut + 0x2f8) =
			*(float*)(SwitchOut + 0x300) =
			*(float*)(SwitchOut + 0x308) = 0;
		*(PDWORD)(SwitchOut + 0x388) = 0xcf8b178b;

		return true;
	}
	return false;
}
//called by hook
void __stdcall SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	// Make player invincible to fix JHs/JGs near mine fields sometimes
	// exploding player while jumping (in jump tunnel)
	pub::SpaceObj::SetInvincible(iShip, true, true, 0);
	if (SystemSwitchOutComplete_before(iShip, iClientID))
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	mapDeferredJumps.erase(iClientID);
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

void UserCmd_Help(uint iClientID, const wstring &wscParam)
{
	returncode = DEFAULT_RETURNCODE;
	PrintUserCmdText(iClientID, L"Change SyS");
}

USERCMD UserCmds[] =
{
	{ L"/changesys", AdminCmd_ChangeSystem, L"Usage: /changesys <sys>" },
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




/*#define IS_CMD(a) !wscCmd.compare(L##a)

void ExecuteCommandString_Callback(CCmds* cmds, uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("changesys"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		cmds->Print(L"about to call function");
		AdminCmd_ChangeSystem(cmds, iClientID, cmds->ArgStr(1), L"usage: changesys <sys>");
		return;
	}
	return;
}
*/


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "ChangeSystem Plugin by RamRawR";
	p_PI->sShortName = "changesys";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Help, PLUGIN_UserCmd_Help, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));

	return p_PI;
}
