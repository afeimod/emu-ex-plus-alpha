#pragma once

/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/config.hh>
#include <emuframework/Option.hh>
#include <imagine/base/Timer.hh>
#include <imagine/fs/FSDefs.hh>
#include <imagine/util/enum.hh>
#include <string>
#include <string_view>
#include <functional>

namespace IG
{
class MapIO;
class FileIO;
}

namespace EmuEx
{

class EmuApp;
class EmuSystem;

WISE_ENUM_CLASS((AutosaveLaunchMode, uint8_t),
	Load,
	LoadNoState,
	Ask,
	NoSave);

enum class LoadAutosaveMode{Normal, NoState};

constexpr std::string_view defaultAutosaveFilename = "auto-00";
constexpr std::string_view noAutosaveName = "\a";

class AutosaveManager
{
public:
	AutosaveLaunchMode autosaveLaunchMode{};
	IG::Minutes autosaveTimerMins{};

	AutosaveManager(EmuApp &);
	bool save();
	bool load(LoadAutosaveMode m = LoadAutosaveMode::Normal);
	bool setSlot(std::string_view name);
	void resetSlot(std::string_view name = "") { autoSaveSlot = name; }
	bool renameSlot(std::string_view name, std::string_view newName);
	bool deleteSlot(std::string_view name);
	std::string_view slotName() const { return autoSaveSlot; }
	std::string slotFullName() const;
	std::string stateTimeAsString() const;
	IG::Time stateTime() const;
	IG::Time backupMemoryTime() const;
	FS::PathString statePath() const { return statePath(autoSaveSlot); }
	FS::PathString statePath(std::string_view name) const;
	void pauseTimer();
	void cancelTimer();
	void resetTimer();
	void startTimer();
	IG::Time nextTimerFireTime() const;
	IG::Time timerFrequency() const;
	bool readConfig(MapIO &, unsigned key, size_t size);
	void writeConfig(FileIO &) const;
	ApplicationContext appContext() const;
	EmuSystem &system();
	const EmuSystem &system() const;

private:
	EmuApp &app;
	std::string autoSaveSlot;
	IG::Timer autoSaveTimer;
	IG::Time autoSaveTimerStartTime{};
	IG::Time autoSaveTimerElapsedTime{};
};

}