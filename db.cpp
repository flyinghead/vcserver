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
#include "vcserver.h"
#include "log.h"
#include <dcserver/database.hpp>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>

static std::string dbPath;

void setDatabasePath(const std::string& databasePath) {
	::dbPath = databasePath;
	try {
		Database db(dbPath);
	} catch (const std::exception& e) {
	    ERROR_LOG(UNKNOWN, "Database error: %s", e.what());
	    exit(1);
	}
}

std::vector<uint8_t> getUserRecord(const std::string& name, GameType gameType)
{
	try {
		Database db(dbPath);
		Statement stmt(db, "SELECT USER_DATA from USER_RECORD WHERE USER_NAME = ? AND GAME_TYPE = ?");
		stmt.bind(1, name);
		stmt.bind(2, gameType);
		if (stmt.step())
			return stmt.getBlobColumn(0);
	} catch (const std::runtime_error& e) {
		ERROR_LOG(gameType, "getUserRecord: %s", e.what());
	}
	return {};
}

void saveUserRecord(const std::string& name, GameType gameType, const uint8_t *data, int size)
{
	Database db;
	try {
		db.open(dbPath);
		Statement stmt(db, "UPDATE USER_RECORD SET USER_DATA = ? WHERE USER_NAME = ? AND GAME_TYPE = ?");
		stmt.bind(1, data, size);
		stmt.bind(2, name);
		stmt.bind(3, gameType);
		stmt.step();
		if (stmt.changedRows() > 0)
			return;
	} catch (const std::runtime_error& e) {
		ERROR_LOG(gameType, "saveUserRecord UPDATE: %s", e.what());
	}
	try {
		Statement stmt(db, "INSERT INTO USER_RECORD (USER_NAME, GAME_TYPE, USER_DATA) VALUES (?, ?, ?)");
		stmt.bind(1, name);
		stmt.bind(2, gameType);
		stmt.bind(3, data, size);
		stmt.step();
	} catch (const std::runtime_error& e) {
		ERROR_LOG(gameType, "saveUserRecord INSERT: %s", e.what());
	}
}

std::vector<HighScore> getHighScores(GameType gameType)
{
	std::vector<HighScore> result;
	try {
		Database db(dbPath);
		Statement stmt(db, "SELECT USER_NAME, USER_DATA from USER_RECORD WHERE GAME_TYPE = ?");
		stmt.bind(1, gameType);
		while (stmt.step()) {
			HighScore score;
			score.name = stmt.getStringColumn(0);
			std::vector<uint8_t> blob = stmt.getBlobColumn(1);
			score.wins = blob[0] + (blob[1] << 8);
			score.losses = blob[2] + (blob[3] << 8);
			score.data3 = blob[4] + (blob[5] << 8);
			if (blob.size() > 12)
				score.data4 = blob[6] + (blob[7] << 8);
			else
				score.data4 = -1;
			result.push_back(score);
		}
		std::sort(result.begin(), result.end(), [](const HighScore& a, const HighScore& b) {
			return (a.wins - a.losses) > (b.wins - b.losses);
		});
		if (result.size() > 50)
			result = std::vector(result.begin(), result.begin() + 50);
	} catch (const std::runtime_error& e) {
		ERROR_LOG(gameType, "getHighScores: %s", e.what());
	}
	return result;
}
