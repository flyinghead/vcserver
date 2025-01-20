/*
	Game server for Visual Concepts Dreamcast games.
    Copyright (C) 2025  Flyinghead

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include <stdint.h>
#include <vector>
#include <string>

enum GameType
{
	UNKNOWN = 1,
	OOOGABOOGA = 2,
	IGP = 3,
	FLOIGAN = 4,
	WSB2K2 = 5,
	NBA2K2 = 6,
	NFL2K2 = 7,
	NCAA2K2 = 8,

	NFL2K1 = 9,
	NBA2K1 = 10,
};

struct HighScore {
	std::string name;
	int wins;
	int losses;
	int data3;
	int data4;
};

void setDatabasePath(const std::string& databasePath);
void closeDatabase();
std::vector<uint8_t> getUserRecord(const std::string& name, GameType gameType);
void saveUserRecord(const std::string& name, GameType gameType, const uint8_t *data, int size);
std::vector<HighScore> getHighScores(GameType gameType);
