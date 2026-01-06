#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <vector>

#include "G_main.h"
#include "rom.h"
#include "movie.h"
#include "save.h"
#include "G_ddraw.h"
#include "automation.h"
#include "gens.h"

using namespace std;

extern int Paused;

//To add additional commandline options
//1) add the identifier (-rom, -play, etc) into the argCmds array
//2) add a variable to store the argument in the list under "Strings that will get parsed"
//3) add an entry in the switch statement in order to assign the variable
//4) add code under the "execute commands" section to handle the given commandline

void ParseCmdLine(LPSTR lpCmdLine, HWND HWnd)
{
	string argumentList;					//Complete command line argument
	argumentList.assign(lpCmdLine);			//Assign command line to argumentList
	int argLength = argumentList.size();	//Size of command line argument

	//List of valid commandline args
	string argCmds[] = {"-cfg", "-rom", "-play", "-readwrite", "-loadstate", "-pause", "-lua",
		"-screenshot-interval", "-screenshot-dir", "-reference-dir", "-max-frames", "-max-diffs", "-frameskip", "-turbo", ""};	//Hint:  to add new commandlines, start by inserting them here.

	//Strings that will get parsed:
	string CfgToLoad = "";		//Cfg filename
	string RomToLoad = "";		//ROM filename
	string MovieToLoad = "";	//Movie filename
	string StateToLoad = "";	//Savestate filename
	vector<string> ScriptsToLoad;	//Lua script filenames
	string FileToLoad = "";		//Any file
	string PauseGame = "";		//adelikat: If user puts anything after -pause it will flag true, documentation will probably say put "1".  There is no case for "-paused 0" since, to my knowledge, it would serve no purpose
	string ReadWrite = "";		//adelikat: Read Only is the default so this will be the same situation as above, any value will set to read+write status

	// Automation parameters
	string ScreenshotIntervalStr = "";	// Capture every N frames
	string ScreenshotDirStr = "";		// Directory for screenshots
	string ReferenceDirStr = "";		// Reference screenshots directory
	string MaxFramesStr = "";			// Stop after N frames
	string MaxDiffsStr = "";			// Stop after N differences
	string FrameSkipStr = "";			// Frame skip value
	string TurboStr = "";				// Enable turbo mode

	//Temps for finding string list
	int commandBegin = 0;	//Beginning of Command
	int commandEnd = 0;		//End of Command
	string newCommand;		//Will hold newest command being parsed in the loop
	string trunc;			//Truncated argList (from beginning of command to end of argumentList

	//--------------------------------------------------------------------------------------------
	//Commandline parsing loop
	for (int x = 0; x < (sizeof argCmds / sizeof string); x++)
	{
		if (argumentList.find(argCmds[x]) != string::npos)
		{
			commandBegin = argumentList.find(argCmds[x]) + argCmds[x].size() + (argCmds[x].empty()?0:1);	//Find beginning of new command
			trunc = argumentList.substr(commandBegin);								//Truncate argumentList
			commandEnd = trunc.find(" ");											//Find next space, if exists, new command will end here
			if(argumentList[commandBegin] == '\"')									//Actually, if it's in quotes, extend to the end quote
			{
				commandEnd = trunc.find('\"', 1);
				if(commandEnd >= 0)
					commandBegin++, commandEnd--;
			}
			if (commandEnd < 0) commandEnd = argLength;								//If no space, new command will end at the end of list
			newCommand = argumentList.substr(commandBegin, commandEnd);				//assign freshly parsed command to newCommand
		}
		else
			newCommand = "";

		//Assign newCommand to appropriate variable
		switch (x)
		{
		case 0:	//-cfg
			CfgToLoad = newCommand;
			break;
		case 1:	//-rom
			RomToLoad = newCommand;
			break;
		case 2:	//-play
			MovieToLoad = newCommand;
			break;
		case 3:	//-readwrite
			ReadWrite = newCommand;
			break;
		case 4:	//-loadstate
			StateToLoad = newCommand;
			break;
		case 5:	//-pause
			PauseGame = newCommand;
			break;
		case 6:	//-lua
			ScriptsToLoad.push_back(newCommand);
			break;
		case 7:	//-screenshot-interval
			ScreenshotIntervalStr = newCommand;
			break;
		case 8:	//-screenshot-dir
			ScreenshotDirStr = newCommand;
			break;
		case 9:	//-reference-dir
			ReferenceDirStr = newCommand;
			break;
		case 10: //-max-frames
			MaxFramesStr = newCommand;
			break;
		case 11: //-max-diffs
			MaxDiffsStr = newCommand;
			break;
		case 12: //-frameskip
			FrameSkipStr = newCommand;
			break;
		case 13: //-turbo
			TurboStr = newCommand;
			break;
		case 14: //  (a filename on its own, this must come BEFORE any other options on the commandline)
			if(newCommand[0] != '-')
				FileToLoad = newCommand;
			break;
		}
	}
	//--------------------------------------------------------------------------------------------
	//Execute commands
	
	// anything (rom, movie, cfg, luascript, etc.)
	if (FileToLoad[0])
	{
		GensOpenFile(FileToLoad.c_str());
	}

	//Cfg
	if (CfgToLoad[0])
	{
		Load_Config((char*)CfgToLoad.c_str(), NULL);
		strcpy(Str_Tmp, "config loaded from ");
		strcat(Str_Tmp, CfgToLoad.c_str());
		Put_Info(Str_Tmp);
	}

	//ROM
	if (RomToLoad[0]) 
	{
		GensLoadRom(RomToLoad.c_str());
	}
	
	//Movie
	if (MovieToLoad[0]) GensPlayMovie(MovieToLoad.c_str(), 1);

	//Read+Write
	if (ReadWrite[0] && MainMovie.ReadOnly != 2) MainMovie.ReadOnly = 0;
	
	//Loadstate
	if (StateToLoad[0])
	{
		Load_State((char*)StateToLoad.c_str());
	}

	//Lua Scripts
	for(unsigned int i = 0; i < ScriptsToLoad.size(); i++)
	{
		if(ScriptsToLoad[i][0])
		{
			const char* error = GensOpenScript(ScriptsToLoad[i].c_str());
			if(error)
				fprintf(stderr, "failed to start script \"%s\" because: %s\n", ScriptsToLoad[i].c_str(), error);
		}
	}

	//Paused
	if (PauseGame[0]) Paused = 1;

	// Automation parameters
	if (ScreenshotIntervalStr[0])
	{
		ScreenshotInterval = atoi(ScreenshotIntervalStr.c_str());
		if (ScreenshotInterval < 0) ScreenshotInterval = 0;
	}

	if (ScreenshotDirStr[0])
	{
		strncpy(ScreenshotDir, ScreenshotDirStr.c_str(), sizeof(ScreenshotDir) - 1);
		ScreenshotDir[sizeof(ScreenshotDir) - 1] = '\0';
	}

	if (ReferenceDirStr[0])
	{
		strncpy(ReferenceDir, ReferenceDirStr.c_str(), sizeof(ReferenceDir) - 1);
		ReferenceDir[sizeof(ReferenceDir) - 1] = '\0';
	}

	if (MaxFramesStr[0])
	{
		MaxFrames = atoi(MaxFramesStr.c_str());
		if (MaxFrames < 0) MaxFrames = 0;
	}

	if (MaxDiffsStr[0])
	{
		MaxDiffs = atoi(MaxDiffsStr.c_str());
		if (MaxDiffs < 0) MaxDiffs = 0;
	}

	if (FrameSkipStr[0])
	{
		Frame_Skip = atoi(FrameSkipStr.c_str());
		if (Frame_Skip < -1) Frame_Skip = -1;
		if (Frame_Skip > 8) Frame_Skip = 8;
	}

	if (TurboStr[0])
	{
		TurboMode = TRUE;
	}


/* OLD CODE	
		char Str_Tmpy[1024];
		int src;

#ifdef CC_SUPPORT
//		src = CC_Connect("CCGEN://Stef:gens@emu.consoleclassix.com/sonicthehedgehog2.gen", (char *) Rom_Data, CC_End_Callback);
		src = CC_Connect(lpCmdLine, (char *) Rom_Data, CC_End_Callback);

		if (src == 0)
		{
			Load_Rom_CC(CCRom.RName, CCRom.RSize);
			Build_Main_Menu();
		}
		else if (src == 1)
		{
			MessageBox(HWnd, "Error during connection", NULL, MB_OK);
		}
		else if (src == 2)
		{
#endif
		src = 0;
		
		if (lpCmdLine[src] == '"')
		{
			src++;
			
			while ((lpCmdLine[src] != '"') && (lpCmdLine[src] != 0))
			{
				Str_Tmpy[src - 1] = lpCmdLine[src];
				src++;
			}

			Str_Tmpy[src - 1] = 0;
		}
		else
		{
			while (lpCmdLine[src] != 0)
			{
				Str_Tmpy[src] = lpCmdLine[src];
				src++;
			}

			Str_Tmpy[src] = 0;
		}

		Pre_Load_Rom(HWnd, Str_Tmpy);

#ifdef CC_SUPPORT
		}
#endif
*/	
}