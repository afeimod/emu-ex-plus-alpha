#pragma once

#include <emuframework/Option.hh>

extern "C"
{
	#include <blueMSX/Board/Board.h>
}

extern bool canInstallCBIOS;
extern char optionMachineNameStr[128];
extern Byte1Option optionSkipFdcAccess;
extern PathOption optionFirmwarePath;
extern FS::PathString machineCustomPath;
extern FS::PathString machineBasePath;
extern PathOption optionMachineName;
extern char hdName[4][512];
extern char cartName[2][512];
extern char diskName[2][512];
extern uint activeBoardType;
extern BoardInfo boardInfo;
extern bool fdcActive;

static const char *installFirmwareFilesMessage =
	#if defined CONFIG_BASE_ANDROID
	"Install the C-BIOS BlueMSX machine files to your storage device?";
	#elif defined CONFIG_ENV_WEBOS
	"Install the C-BIOS BlueMSX machine files to internal storage? If using WebOS 1.4.5, make sure you have a version without the write permission bug.";
	#elif defined CONFIG_BASE_IOS
	"Install the C-BIOS BlueMSX machine files to /User/Media/MSX.emu?";
	#else
	"Install the C-BIOS BlueMSX machine files to Machines directory?";
	#endif

void installFirmwareFiles();
HdType boardGetHdType(int hdIndex);
FS::PathString makeMachineBasePath(FS::PathString customPath);
bool hasMSXTapeExtension(const char *name);
bool hasMSXDiskExtension(const char *name);
bool hasMSXROMExtension(const char *name);
bool insertROM(const char *path, uint slot = 0);
bool insertDisk(const char *path, uint slot = 0);
