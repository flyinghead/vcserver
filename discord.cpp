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
#include "discord.h"
#include "log.h"
#include <dcserver/discord.hpp>
#include <chrono>

static const char *GameIds[] {
	nullptr,
	nullptr,
	"oogabooga",
	"igp",
	"floigan",
	"wsb2k2",
	"nba2k2",
	"nfl2k2",
	"ncaa2k2",
	"nfl2k1",
	"nba2k1"
};

std::string getGameId(GameType gameType)
{
	if (gameType < OOOGABOOGA || gameType >= std::size(GameIds))
		return {};
	else
		return GameIds[gameType];
}

void discordLobbyJoined(GameType gameType, const std::string& username, const std::vector<std::string>& playerList)
{
	if (gameType < OOOGABOOGA || gameType >= std::size(GameIds))
		return;
	using the_clock = std::chrono::steady_clock;
	static the_clock::time_point last_notif;
	the_clock::time_point now = the_clock::now();
	if (last_notif != the_clock::time_point() && now - last_notif < std::chrono::minutes(5))
		return;
	last_notif = now;
	Notif notif;
	notif.content = "Player **" + discordEscape(username) + "** joined the lobby";
	notif.embed.title = "Lobby Players";
	for (const auto& player : playerList)
		notif.embed.text += discordEscape(player) + "\n";
	try {
		discordNotif(getGameId(gameType), notif);
	} catch (const std::exception& e) {
		ERROR_LOG(gameType, "Discord error: %s", e.what());
	}
}

void discordGameCreated(GameType gameType, const std::string& username, const std::vector<std::string>& playerList)
{
	if (gameType < OOOGABOOGA || gameType >= std::size(GameIds))
		return;
	Notif notif;
	notif.content = "Player **" + discordEscape(username) + "** created a game";
	notif.embed.title = "Lobby Players";
	for (const auto& player : playerList)
		notif.embed.text += discordEscape(player) + "\n";

	try {
		discordNotif(getGameId(gameType), notif);
	} catch (const std::exception& e) {
		ERROR_LOG(gameType, "Discord error: %s", e.what());
	}
}
