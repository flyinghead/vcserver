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
#include <curl/curl.h>
#include "json.hpp"
#include <atomic>
#include <thread>
#include <chrono>

static std::string DiscordWebhook;
static std::atomic_int threadCount;

using namespace nlohmann;

struct {
	const char *name;
	const char *url;
} Games[] = {
	{ },
	{ },
	{ "Ooga Booga",					"https://dcnet.flyca.st/gamepic/oogabooga.jpg" },
	{ "Internet Game Pack",			"https://dcnet.flyca.st/gamepic/igp.jpg" },
	{ "Floigan Bros",				"https://dcnet.flyca.st/gamepic/floigan.jpg" },
	{ "World Series Baseball 2K2",	"https://dcnet.flyca.st/gamepic/wsb2k2.jpg" },
	{ "NBA 2K2", 					"https://dcnet.flyca.st/gamepic/nba2k2.jpg" },
	{ "NFL 2K2", 					"https://dcnet.flyca.st/gamepic/nfl2k2.jpg" },
	{ "NCAA 2K2", 					"https://dcnet.flyca.st/gamepic/ncaa2k2.jpg" },
	{ "NFL 2K1", 					"https://dcnet.flyca.st/gamepic/nfl2k1.jpg" },
	{ "NBA 2K1", 					"https://dcnet.flyca.st/gamepic/nba2k1.jpg" },
};

class Notif
{
public:
	Notif(GameType gameType) : gameType(gameType) {}

	std::string to_json() const
	{
		json embeds;
		embeds.push_back({
			{ "author",
				{
					{ "name", Games[gameType].name },
					{ "icon_url", Games[gameType].url }
				},
			},
			{ "title", embed.title },
			{ "description", embed.text },
			{ "color", 9118205 },
		});

		json j = {
			{ "content", content },
			{ "embeds", embeds },
		};
		return j.dump(4);
	}

	GameType gameType;
	std::string content;
	struct {
		std::string title;
		std::string text;
	} embed;
};

static void postWebhook(Notif notif)
{
	CURL *curl = curl_easy_init();
	if (curl == nullptr) {
		fprintf(stderr, "Can't create curl handle\n");
		threadCount.fetch_sub(1);
		return;
	}
	CURLcode res;
	curl_easy_setopt(curl, CURLOPT_URL, DiscordWebhook.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "DCNet-DiscordWebhook");
	curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	std::string json = notif.to_json();
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl error: %d", res);
	}
	else
	{
		long code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code < 200 || code >= 300)
			fprintf(stderr, "Discord error: %ld", code);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	threadCount.fetch_sub(1);
}

static void discordNotif(const Notif& notif)
{
	if (DiscordWebhook.empty() || notif.gameType < OOOGABOOGA || notif.gameType >= std::size(Games))
		return;
	if (threadCount.fetch_add(1) > 5) {
		threadCount.fetch_sub(1);
		fprintf(stderr, "Discord max thread count reached");
		return;
	}
	std::thread thread(postWebhook, notif);
	thread.detach();
}

void setDiscordWebhook(const std::string& url)
{
	DiscordWebhook = url;
}

void discordLobbyJoined(GameType gameType, const std::string& username, const std::vector<std::string>& playerList)
{
	using the_clock = std::chrono::steady_clock;
	static the_clock::time_point last_notif;
	the_clock::time_point now = the_clock::now();
	if (last_notif != the_clock::time_point() && now - last_notif < std::chrono::minutes(5))
		return;
	last_notif = now;
	Notif notif(gameType);
	notif.content = "Player **" + username + "** joined the lobby";
	notif.embed.title = "Lobby Players";
	for (const auto& player : playerList)
		notif.embed.text += player + "\n";
	discordNotif(notif);
}

void discordGameCreated(GameType gameType, const std::string& username, const std::vector<std::string>& playerList)
{
	Notif notif(gameType);
	notif.content = "Player **" + username + "** created a game";
	notif.embed.title = "Lobby Players";
	for (const auto& player : playerList)
		notif.embed.text += player + "\n";

	discordNotif(notif);
}
