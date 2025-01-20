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
#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>

static sqlite3 *db;
static std::string dbPath;

void setDatabasePath(const std::string& databasePath) {
	::dbPath = databasePath;
}

static bool openDatabase()
{
	if (db != nullptr)
		return true;
	if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
		fprintf(stderr, "ERROR: Can't open database %s: %s\n", dbPath.c_str(), sqlite3_errmsg(db));
		return false;
	}
	sqlite3_busy_timeout(db, 1000);
	return true;
}

void closeDatabase()
{
	if (db != nullptr) {
		sqlite3_close(db);
		db = nullptr;
	}
}

static void throwSqlError(sqlite3 *db)
{
	const char *msg = sqlite3_errmsg(db);
	if (msg != nullptr)
		throw std::runtime_error(msg);
	else
		throw std::runtime_error("SQL Error");
}

class Statement
{
public:
	Statement(sqlite3 *db, const char *sql) : db(db) {
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
			throwSqlError(db);
	}

	~Statement() {
		if (stmt != nullptr)
			sqlite3_finalize(stmt);
	}

	void bind(int idx, int v) {
		if (sqlite3_bind_int(stmt, idx, v) != SQLITE_OK)
			throwSqlError(db);
	}
	void bind(int idx, const std::string& s) {
		if (sqlite3_bind_text(stmt, idx, s.c_str(), s.length(), SQLITE_TRANSIENT) != SQLITE_OK)
			throwSqlError(db);
	}
	void bind(int idx, const uint8_t *data, size_t len) {
		if (sqlite3_bind_blob(stmt, idx, data, len, SQLITE_STATIC) != SQLITE_OK)
			throwSqlError(db);
	}

	bool step()
	{
		int rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
			return true;
		if (rc != SQLITE_DONE && rc != SQLITE_OK)
			throwSqlError(db);
		return false;
	}

	int getIntColumn(int idx) {
		return sqlite3_column_int(stmt, idx);
	}
	std::string getStringColumn(int idx) {
		return std::string((const char *)sqlite3_column_text(stmt, idx));
	}
	std::vector<uint8_t> getBlobColumn(int idx)
	{
		std::vector<uint8_t> blob;
		blob.resize(sqlite3_column_bytes(stmt, idx));
		if (!blob.empty())
			memcpy(blob.data(), sqlite3_column_blob(stmt, idx), blob.size());
		return blob;
	}

	int changedRows() {
		return sqlite3_changes(db);
	}

private:
	sqlite3 *db;
	sqlite3_stmt *stmt = nullptr;
};

std::vector<uint8_t> getUserRecord(const std::string& name, GameType gameType)
{
	if (!openDatabase())
		return {};
	try {
		Statement stmt(db, "SELECT USER_DATA from USER_RECORD WHERE USER_NAME = ? AND GAME_TYPE = ?");
		stmt.bind(1, name);
		stmt.bind(2, gameType);
		if (stmt.step())
			return stmt.getBlobColumn(0);
	} catch (const std::runtime_error& e) {
		fprintf(stderr, "ERROR: getUserRecord: %s", e.what());
	}
	return {};
}

void saveUserRecord(const std::string& name, GameType gameType, const uint8_t *data, int size)
{
	if (!openDatabase())
		return;
	try {
		Statement stmt(db, "UPDATE USER_RECORD SET USER_DATA = ? WHERE USER_NAME = ? AND GAME_TYPE = ?");
		stmt.bind(1, data, size);
		stmt.bind(2, name);
		stmt.bind(3, gameType);
		stmt.step();
		if (stmt.changedRows() > 0)
			return;
	} catch (const std::runtime_error& e) {
		fprintf(stderr, "ERROR: saveUserRecord UPDATE: %s", e.what());
	}
	try {
		Statement stmt(db, "INSERT INTO USER_RECORD (USER_NAME, GAME_TYPE, USER_DATA) VALUES (?, ?, ?)");
		stmt.bind(1, name);
		stmt.bind(2, gameType);
		stmt.bind(3, data, size);
		stmt.step();
	} catch (const std::runtime_error& e) {
		fprintf(stderr, "ERROR: saveUserRecord INSERT: %s", e.what());
	}
}

std::vector<HighScore> getHighScores(GameType gameType)
{
	std::vector<HighScore> result;
	if (!openDatabase())
		return result;
	try {
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
		fprintf(stderr, "ERROR: getHighScores: %s", e.what());
	}
	return result;
}
