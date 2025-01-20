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
#include "shared_this.h"
extern "C" {
#include "blowfish.h"
}
#include "vcserver.h"
#include "discord.h"
#include <signal.h>
#include <asio.hpp>
#include <stdio.h>
#include <string>
#include <array>
#include <vector>
#include <functional>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

#if ASIO_VERSION < 102900
namespace asio::placeholders
{
static inline constexpr auto& error = std::placeholders::_1;
static inline constexpr auto& bytes_transferred = std::placeholders::_2;
}
#endif

// server IP: 146.185.135.179
//            92  b9  87  b3
// OOGA BOOGA
// auth server
// request:  1f 00 92 13 02 00 04 2b 00 00 00 06 00 96 59 59 f9 f2 9e 8b e7 06 00 e1 0f a0 9c 6a cf bf 68
//           len   fixed wsb2k  l (var64)     len   user name encrypted     len   password encrypted
//                       -> 5     varies...key num?
//													encrypted data is always multiple of 8 bytes (len is real len after decrypt)
// response: 1e 00 95 13 00 00 0c 00 50 6c 61 79 65 72 5f 30 31 34 38 32 08 00 00 00 00 00 00 00 00 08
//	         len               len    P  l  a  y  e  r  _  0  1  4  8  2 len                        ??
//
// record server (port 12201 for ooga)
// request: (RecordRetrieve)
// 25 00				len (37)
// 5a 1b 04 14 00 00 00 0c 00 b0 23 b8 09 cf 00 af 48 ad 7c 1f 39 5f c8 49 6d 06 00 f8 42 9f d3 8f 65 8e e0
// response:
// 18 00				len (24)
// 5b 1b 00 00 10 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00
//             len?              00 for new user
// EOT
// (after game?)2
// req:		(RecordUpdate)
// 37 00
// 5e 1b 04 14 00 00 00 0c 00 b0 23 b8 09 cf 00 af 48 e6 0b de b6 e4 d2 60 98 06 00 42 62 5b ae 4d a8 df 75 10 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00
// resp:
// 18 00
// 5b 1b 01 00 10 00  00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00
// EOT
//
// region server (port 15203)
// req:
// 04 00
// a0 0f
// resp:
// 17 00				len (23)
// a1 0f 81 0a 00 53 68 75 6f 47 61 72 64 65 6e 02 00 00 00 00 00
//          len   S  h  u  o  G  a  r  d  e  n
// EOT
// req:
// 10 00				len (16)
// a2 0f 0a 00 53 68 75 6f 47 61 72 64 65 6e
//       len   S  h  u  o  G  a  r  d  e  n
// resp:
// 1d 00				len (29)
// a3 0f 00 00 81 08 00 53 68 75 6d 61 6e 69 61 92 b9 87 b3 3b 64 02  00 00 00 00 00
//                len   S  h  u  m  a  n  i  a  IP address. port.
//                                                          15204
// EOT
//
// game server? port 15204
// req:
// 2d 00				len (45)
// d2 00 ee 0c 35 4f  ba 72 73 df 00 0c 00 50 6c 61 79 65 72 5f 30 31 34 38 32 10 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00
//                                   len   P  l  a  y  e  r  _  0  1  4  8  2  len
// resp:
// 08 00				len
// d3 00 00 00 03 00
//             3 players?
// 26 00				len (38)
// 37 01 81 02 00 07 00 50 49 54 4f 52 55 53 bb 26 ff a7                10 00 01 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00
//                       P  I  T  O  R  U  S ...IP addr....
// 2b 00				len (43)
// 37 01 81 03 00 0c 00 50 6c 61 79 65 72 5f 30 31 34 38 32 b0 94 41 49 10 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00
//                      P  l  a  y  e  r  _  0  1  4  8  2  ...IP addr....
// 27 00
// 37 01 81 04 00 08 00 53 63 72 69 76 61 6e 69 c9 00 72 38              10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//                      S  c  r  i  v  a  n  i  ...IP addr....
// alternative: (first login?)
// 27 00
// d2 00 58 58 58 58 58 58 58 58 00 06 00 6c 6c 6c 6c 6c 6c 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//       X  X  X  X  X  X  X  X           l  l  l  l  l  l
// resp: (might be unrelated to the current exchange?) Challenge?
// 0b 00
// 3d 00 02 00 00 00 00 00 00
// req:
// 07 00
// 3e 00 00 00 00
// resp: (normal) 08 00 d3 00 00 00 01 00...

// req:
// 0c 00
// 8a 02 01 00 00 00 00 00 00 00
// resp:
// 27 00
// 8b 02 01 00 00 00 81 01 00 08 00 53 63 72 69 76 61 6e 69 04 00 0c 00 00 00 01 02 05 00 00 00 00 41 00 00 00 00
//                                  S  c  r  i  v  a  n  i
// USER JOINS
// 27 00
// 8b 02 01 00 00 00 81 01 00 08 00 53 63 72 69 76 61 6e 69 04 00 0c 00 00 00 01 02 05 00 00 00 00 43 00 00 00 00
//                                  S  c  r  i  v  a  n  i
// ping?
// 0a 00
// 90 02 00 00 00 00 01 00
// req: Leave lobby
// 0a 00
// d4 00 02 00 00 00 00 00

static std::string RegionName = "DCNet World";
static std::string LobbyName = "DCNet";
static asio::io_context io_context;
static std::unordered_map<std::string, std::string> Config;

class Connection;

class User : public SharedThis<User>
{
	User(const std::string& name, Connection *connection, uint16_t userId, uint16_t recSize, uint8_t *record)
		: name(name), connection(connection), userId(userId)
	{
		this->record.resize(recSize);
		memcpy(this->record.data(), record, recSize);
	}

public:
	std::string name;			// 6-16 chars
	Connection *connection;
	uint16_t userId = 0;
	std::vector<uint8_t> record;

	friend super;
};

class Game
{
public:
	Game() = default;
	Game(User::Ptr host, uint16_t paramSize, uint8_t *params, uint16_t gameId)
		: host(host), gameId(gameId)
	{
		setParams(paramSize, params);
	}

	void setParams(uint16_t paramSize, uint8_t *params) {
		this->params.resize(paramSize);
		memcpy(this->params.data(), params, this->params.size());
	}

	User::Ptr host;
	std::vector<uint8_t> params;
	uint16_t unknown;	// sent after game params
	uint16_t gameId = 0;
};

class Lobby
{
	uint16_t nextUserId()
	{
		uint16_t id = users.size() + 1;
		for (const User::Ptr& user : users)
			id = std::max<uint16_t>(id, user->userId + 1);
		return id;
	}

	uint16_t nextGameId()
	{
		uint16_t id = games.size() + 1;
		for (const Game& game : games)
			id = std::max<uint16_t>(id, game.gameId + 1);
		return id;
	}

	Game *findGame(Connection *connection, const std::string& hostname)
	{
		for (Game& game : games) {
			if (game.host->connection == connection && game.host->name == hostname)
				return &game;
		}
		return nullptr;
	}

public:
	Lobby(GameType gameType) : gameType(gameType) {}

	User::Ptr addUser(const std::string& name, Connection *connection, uint16_t recSize, uint8_t *record);

	void deleteUser(Connection *connection)
	{
		uint16_t userId = 0;
		for (auto it = users.begin(); it != users.end(); ++it)
		{
			if ((*it)->connection == connection)
			{
				// Find if the user is hosting a game
				uint16_t gameId = 0;
				for (auto gameIt = games.begin(); gameIt != games.end(); ++gameIt) {
					if (gameIt->host == *it) {
						gameId = gameIt->gameId;
						break;
					}
				}
				userId = (*it)->userId;
				printf("Lobby: user %s left (ID=%d)\n", (*it)->name.c_str(), userId);
				(*it)->connection = nullptr;
				// Delete the user before deleting the game so he doesn't get the update
				users.erase(it);
				if (gameId != 0)
					deleteGame(nullptr, gameId);
				break;
			}
		}
		if (userId != 0)
			sendUserLeave(userId);
	}

	Game *updateGame(Connection *connection, const std::string& hostname, uint16_t paramSize, uint8_t *params, uint16_t unknown)
	{
		Game *game = findGame(connection, hostname);
		if (game != nullptr) {
			game->setParams(paramSize, params);
			game->unknown = unknown;
			sendGameUpdate(*game, game->host);
			return game;
		}
		for (auto& user : users)
		{
			if (user->connection == connection) {	// IGP uses "CREATE GAME" as host name
				games.emplace_back(user, paramSize, params, nextGameId());
				games.back().unknown = unknown;
				sendGameUpdate(games.back(), user);

				std::vector<std::string> userNames;
				for (const auto& user : users)
					userNames.push_back(user->name);
				std::sort(userNames.begin(), userNames.end());
				discordGameCreated(gameType, user->name, userNames);

				return &games.back();
			}
		}
		fprintf(stderr, "ERROR: can't create/update game: host not found\n");
		return nullptr;
	}

	bool deleteGame(const User::Ptr& user, int gameId)
	{
		for (auto it = games.begin(); it != games.end(); ++it)
		{
			if ((user == nullptr || it->host == user) && it->gameId == gameId)
			{
				printf("Game %d by host %s deleted\n", gameId, user ? user->name.c_str() : "?");
				games.erase(it);
				sendGameDelete(gameId);
				return true;
			}
		}
		return false;
	}

	void sendChat(const User::Ptr& user, const std::string& chatMsg);
	void sendGameUpdate(const Game& game, User::Ptr exceptTo = {});
	void sendGameDelete(uint16_t gameId);
	void sendUserLeave(uint16_t userId);

	GameType gameType;
	std::vector<User::Ptr> users;
	std::vector<Game> games;
};

class Connection : public SharedThis<Connection>
{
public:
	asio::ip::tcp::socket& getSocket() {
		return socket;
	}

	void receive() {
		recvBuffer.clear();	// FIXME do we have to handle more than 1 msg per buffer?
		asio::async_read_until(socket, asio::dynamic_vector_buffer(recvBuffer), packetMatcher,
				std::bind(&Connection::onReceive, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
	}

	void sendUser(int idx)
	{
		if (is2K1()) {
			respond(301);
			respByte(1);
		}
		else {
			respond(311);
			respByte(0x81);
		}
		const User::Ptr& user = lobby->users[idx];
		printf("Sending user[%d] %s\n", user->userId, user->name.c_str());
		respShort(user->userId);
		respStr(user->name);
		try {
			asio::ip::address_v4 address = user->connection->address();
			respData(address.to_bytes());
		} catch (const std::system_error& e) {
			// connection might have been closed
			respLong(0);
		}
		if (!is2K1()) {
			respShort(user->record.size());
			respData(user->record);
		}
		send();
	}

	void sendChat(int userId, const std::string& chatMsg)
	{
		respond(501);
		respShort(userId);
		respStr(chatMsg);
		send();
	}

	void sendGameUpdate(const Game& game)
	{
		respond(651);
		respLong(1);
		respByte(0x81);
		respShort(game.gameId);
		respStr(game.host->name);
		respShort(game.host->userId);
		respShort(game.params.size());
		respData(game.params);
		respShort(game.unknown);
		send();
	}

	void sendGameDelete(uint16_t gameId)
	{
		respond(656);
		respSkip(4);
		respShort(gameId);
		send();
	}

	void sendUserLeave(uint16_t userId)
	{
		if (is2K1())
			respond(203);
		else
			respond(213);
		respShort(userId);
		send();
	}

private:
	Connection(asio::io_context& io_context, std::shared_ptr<Lobby> lobby)
		: io_context(io_context), socket(io_context),
		  lobby(lobby), gameType(lobby ? lobby->gameType : UNKNOWN) {
	}

	bool is2K1() const {
		return gameType == NFL2K1 || gameType == NBA2K1;
	}

	void send()
	{
		*(uint16_t *)&sendBuffer[packetStart] = sendIdx - packetStart;
		packetStart = 0;
		sendInternal();
	}
	void sendInternal()
	{
		if (sending)
			return;
		sending = true;
		//printf("OUT: ");
		//dumpSendMsg();
		uint16_t packetSize = *(uint16_t *)&sendBuffer[0];
		asio::async_write(socket, asio::buffer(sendBuffer, packetSize),
			std::bind(&Connection::onSent, shared_from_this(),
					asio::placeholders::error,
					asio::placeholders::bytes_transferred));
	}
	void onSent(const std::error_code& ec, size_t len)
	{
		if (ec)
		{
			fprintf(stderr, "ERROR: onSent: %s\n", ec.message().c_str());
			lobbyLeave();
			return;
		}
		sending = false;
		assert(len <= sendIdx);
		sendIdx -= len;
		if (sendIdx != 0) {
			memmove(&sendBuffer[0], &sendBuffer[len], sendIdx);
			sendInternal();
		}
	}

	using iterator = asio::buffers_iterator<asio::const_buffers_1>;

	std::pair<iterator, bool>
	static packetMatcher(iterator begin, iterator end)
	{
		if (end - begin < 3)
			return std::make_pair(begin, false);
		iterator i = begin;
		uint16_t len = (uint8_t)*i++;
		len |= uint8_t(*i++) << 8;
		if (end - begin < len)
			return std::make_pair(begin, false);
		return std::make_pair(begin + len, true);
	}

	void onReceive(const std::error_code& ec, size_t len)
	{
		if (ec || len == 0)
		{
			if (ec && ec != asio::error::eof)
				fprintf(stderr, "ERROR: onReceive: %s\n", ec.message().c_str());
			else
				printf("Connection closed\n");
			lobbyLeave();
			return;
		}
		dumpMsg();
		uint16_t msgLen = recvShort(0);
		if (len != msgLen)
			fprintf(stderr, "ERROR: Received %zd bytes but packet len is %d\n", len, msgLen);
		uint16_t msgType = recvShort(2);
		switch (msgType)
		{
		// Auth server
		case 5010:		// UserLogin
			userLogin();
			break;
		case 5000:			// nfl 2k1
		case 5004:			// nba 2k1
			userLogin2K1();
			break;
		case 5012:		// UserCreate
			userCreate();
			break;
		case 5002:		// nfl 2k1
			userCreate2k1();
			break;

		// Record server
		case 7002:		// RecordRetrieve
			recordRetrieve();
			break;
		case 7006:		// RecordUpdate
			recordUpdate();
			break;
		case 7014:		// get top 50 players
			getTopUserList();
			break;

		// Region server
		case 4000:		// get regions
			getRegions();
			break;
		case 4002:		// get lobby servers
			getLobbies();
			break;
		case 50:		// find user
			findUser();
			break;
		case 400:		// find user 2k1
			findUser();
			break;

		// Lobby server
		case 200:		// enter lobby (nfl 2K1)
		case 206:		// enter lobby (nba 2K1)
			lobbyRegister2K1();
			break;
		case 210:		// enter lobby, get user list
			lobbyRegister();
			break;
		case 202:		// leave lobby (2k1)
		case 212:		// leave lobby
			lobbyLeave();
			break;
		case 650:		// get game list
			getGameList();
			break;
		case 654:		// create game
			createUpdateGame();
			break;
		case 656:		// delete/leave game
			leaveGame();
			break;
		case 500:		// chat message
			chatSent();
			break;
		case 62:
			lobbyUserList();
			break;
		case 602:		// get user list (2k1)
			lobbyUserList2K1();
			break;
		case 10:		// ping? refresh request? respond with 311 or 651 if new user or new game
			break;
		default:
			fprintf(stderr, "TODO: Unknown packet type %04x (len %d)\n", msgType, msgLen);
			break;
		}
		receive();
	}

	int decrypt(BLOWFISH_CTX *ctx, uint8_t *data, int len)
	{
		int lenw = (len + 7) / 8 * 2;
		uint32_t *p = (uint32_t *)data;
		for (int i = 0; i < lenw; i += 2, p += 2)
			Blowfish_Decrypt(ctx, p, p + 1);
		return lenw * 4;
	}

	bool validLoginString(const std::string& s)
	{
		for (char c : s) {
			if (c < ' ' || c > '~')
				return false;
		}
		return true;
	}

	int recvLoginPassword(uint8_t *data, std::string& username, std::string& password)
	{
		printf("keychal %x ", data[1]);
		int n = data[0] + 1;		// skip key challenge

		uint8_t *puser = &data[n];
		uint16_t userlen = *(uint16_t *)puser;
		n += 2;
		int cuserlen = (userlen + 7) / 8 * 8;
		n += cuserlen;
		uint8_t tmpuser[18];

		uint8_t *ppassword = &data[n];
		uint16_t passwordlen = *(uint16_t *)ppassword;
		n += 2;
		int cpasswordlen = (passwordlen + 7) / 8 * 8;
		n += cpasswordlen;
		uint8_t tmppassword[18];

		if (userlen > 16 || passwordlen > 16)
		{
			fprintf(stderr, "ERROR: username or password too long (%d, %d)\n", userlen, passwordlen);
			username.clear();
			password.clear();
			return n;
		}

		// ooga key schedule:
		// var4	key
		// 29	2f
		// 5e	16
		// 33	21
		// 09	60
		// 10	01
		// 34	2f
		// 07	21
		// 49	60
		// 22	40
		// 4f	16
		// 2a	21
		// 23	01
		// 60	01
		// 3e	01
		// 27   21
		// 12	16
		//
		BLOWFISH_CTX ctx{};
		// 1f 00 92 13 02 00 04 2b 00 00 00 06 00 a9 60 29 07 a9 f5 4c a1 06 00 a7 c9 d2 03 53 f8 eb 2b

		// Brute-force crack the encryption key since it's only 8 bits.
		// A smarter way would be to derive the key from the included key challenge, which is probably what
		// the original server does.
		uint32_t key = 0;
		for (; key < 0x100; key++)
		{
			ctx = {};
			Blowfish_Init(&ctx, (uint8_t *)&key, sizeof(key));
			memcpy(tmpuser, puser, cuserlen + 2);
			decrypt(&ctx, tmpuser + 2, userlen);
			username = recvStr(tmpuser);

			memcpy(tmppassword, ppassword, cpasswordlen + 2);
			decrypt(&ctx, tmppassword + 2, passwordlen);
			password = recvStr(tmppassword);

			if (validLoginString(username) && validLoginString(password))
				break;
		}
		if (key == 0x100) {
			fprintf(stderr, "ERROR: can't find the encryption key\n");
			username.clear();
			password.clear();
		}
		else {
			printf("key %x username/password: %s/%s\n", key, username.c_str(), password.c_str());
		}
		return n;
	}

	void loginFailure(const std::string& msg)
	{
		respond(5013);
		// Invalid username/password:
		// 36 00 95 13 01 00 1b 00  49 6e 63 6f 72 72 65 63   6....... Incorrec
	    // 74 20 75 73 65 72 6e 61  6d 65 2f 70 61 73 73 77   t userna me/passw
	    // 6f 72 64 08 01 00 00 00  00 00 00 00 08 00 36 10   ord..... ......6.
	    // fd 38 65 26 f5 7d
		respShort(1);	// failure
		respStr(msg);
		respByte(8);
		respByte(1);
		respSkip(7);
		respShort(8);
		respByte(0x36);
		respByte(0x10);
		respByte(0xfd);
		respByte(0x38);
		respByte(0x65);
		respByte(0x26);
		respByte(0xf5);
		respByte(0x7d);
		send();
	}

	void userLogin()
	{
		int gameId = recvShort(4);
		printf("Login: game %d ", gameId);
		std::string username, password;
		recvLoginPassword(&recvBuffer[6], username, password);

		if (!username.empty() && !password.empty())
		{
			respond(5013);
			respShort(0);		// success?
			respStr(username);
			respShort(8);
			respSkip(7);
			respByte(8);
			send();
		}
		else {
			loginFailure("Authentication error");
		}
	}

	void userLogin2K1()
	{
		// msg 5004: (nba 2k1)
		// 1f 00 8c 13 04 06 00 00 00 06 00 69 8b ac aa 1c 31 6d fa 06 00 b8 84 ba cc d6 a5 3b 9d 01 00
		//             l  key chal    len   encrypted string....... len   encrypted string.......
		// msg 5000: (nfl 2k1)
		// 2d 00 88 13 04 38 00 00 00 0a 00 16 c7 e6 56 02 f7 1b 70 d1 e3 f8 89 01 e9 d9 95 09 00 b4 0f 5b 01 3a df 05 e1 0e 19 7e 83 70 d1 95 5b
		// resp: 5003
		// 10 00 8b 13 01 00 08 00  00 00 00 00 00 00 00 08
		printf("Login(500x) 2K1 ");
		std::string username, password;
		recvLoginPassword(&recvBuffer[4], username, password);

		if (!username.empty() && !password.empty())
		{
			respond(5003);
			respShort(1);
			respShort(8);
			respSkip(7);
			respByte(8);
		}
		else
		{
			// not working either
			respond(0xffff);
			respLong(0);

			// FIXME doesn't work. Need actual net dump.
			// nfl 2K1:
			// readShort	if 0 -> success
			//   readString		user name?
			//   readString		password?
			//              else
			//   readString		error message?
			//respond(5003);
			//respShort(1);	// error
			//respStr("Authentication error POUET");
		}
		send();
	}

	void userCreate()
	{
		// 21 00 94 13 02 00 00 00 04 29 00 00 00 06 00 29 d6 60 bf 28 1e 28 aa 06 00 ed c3 ef 36 3c dc 15 97
		//             gameId      l  key chal    len   user-name.............. len   password...............
		// response:
		// 21 00 95 13 00 00 06 00  6c 6c 6c 6c 6c 6c 08 01   !....... llllll..
		// 00 00 00 00 00 00 00 08  00 36 10 fd 38 65 26 f5   ........ .6..8e&.
		// 7d
		int gameId = recvShort(4);
		std::string username, password;
		recvLoginPassword(&recvBuffer[8], username, password);
		printf("Create user: game %d login %s password %s\n", gameId, username.c_str(), password.c_str());

		if (!username.empty() && !password.empty())
		{
			respond(5013);
			respShort(0);
			respStr(username);
			respByte(8);
			respByte(1);
			respSkip(7);
			respShort(8);
			respByte(0x36);
			respByte(0x10);
			respByte(0xfd);
			respByte(0x38);
			respByte(0x65);
			respByte(0x26);
			respByte(0xf5);
			respByte(0x7d);
			send();
		}
		else {
			loginFailure("User creation failed");
		}
	}

	void userCreate2k1()
	{
		// 28 00 8a 13 05 00 70 61 72 69 73 02 00 66 72 04 (.....paris..fr.
		//                   city                 state
		// 27 00 00 00 06 00 47 a4 fb 5d c2 d7 80 fd 06 00 '.....G..]......
		//                   user name
		// c6 89 32 d2 7b 7c f1 09                         ..2.{|..
		// password
		// no city and state:
		// 21 00 8a 13 00 00 00 00 04 19 00 00 00 06 00 cb !...............
		//             city sz     key chal       username
		//                  state sz
		// ee c2 eb b3 34 4c 70 06 00 ea aa 2b 57 ff ba 68 ....4Lp....+W..h
		//                            password
		// ac                                              .

		size_t n = 4;
		std::string city = recvStr(n);
		n += 2 + city.length();
		std::string state = recvStr(n);
		n += 2 + state.length();
		std::string username, password;
		recvLoginPassword(&recvBuffer[n], username, password);
		printf("Create user 2k1: login %s password %s city %s state %s\n", username.c_str(), password.c_str(),
				city.c_str(), state.c_str());
		respond(5003);
		if (!username.empty() && !password.empty())
		{
			respShort(1);
			respShort(8);
			respSkip(7);
			respByte(8);
		}
		else
		{
			// FIXME doesn't work. Need actual net dump.
			respShort(2);	// ???
			respStr("Authentication error");
			respByte(8);
			respByte(1);
			respSkip(7);
			respShort(8);
		}
		send();
	}

	void recordRetrieve()
	{
		// 25 00
		// 5a 1b 04 14 00 00 00 0c 00 b0 23 b8 09 cf 00 af 48 ad 7c 1f 39 5f c8 49 6d 06 00 f8 42 9f d3 8f 65 8e e0
		printf("User record: ");
		std::string username, password;
		recvLoginPassword(&recvBuffer[4], username, password);
		// TODO check username password are valid
		std::vector<uint8_t> record = getUserRecord(username, gameType);
		respond(7003);
		respShort(0);
		if (record.empty())
		{
			uint16_t recordSize = 16;
			if (gameType == IGP)
				recordSize = 256;
			else if (gameType == NBA2K2)
				recordSize = 12;
			respShort(recordSize);
			respSkip(recordSize);
		}
		else {
			respShort(record.size());
			respData(record);
		}
		send();
	}

	void recordUpdate()
	{
		printf("Record update: ");
		std::string username, password;
		int n = 4;
		n += recvLoginPassword(&recvBuffer[n], username, password);
		// TODO check username password are valid
		uint16_t recordSize = recvShort(n);
		n += 2;
		saveUserRecord(username, gameType, &recvBuffer[n], recordSize);
		// when game starts
		// 2f 00 5e 1b 04 3f 00 00 00 07 00 25 a4 49 15 96 3a 3a 7d 06 00 ee 34 49 5c 6b 1a d4 30
		//             l  key chal    len   encrypted               len   encrypted           ...
		//    10 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00
		//    len   params?                             in-game
		// 37 00 5e 1b 04 24 00 00 00 0a 00 d3 67 e8 01 5d 9f 1d e7 b7 5e 19 06 b6 6d a4 06 00 b8 24 63 f4 b6 ee a2 88
		//             l  key chal    len   encrypted                                ... len   encrypted           ...
		//    10 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00
		//    len   params                              in-game
		// resp:
		// 18 00
		// 5b 1b 01 00 10 00  00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00
		respond(7003);
		if (gameType == OOOGABOOGA)
			// oogabooga returns 1 here
			respShort(1);
		else
			// other games want 0
			respShort(0);
		// ooga,wsb2k2 sends 4 longs
		// nba2k2 sends 3 longs
		// IGP sends 4 or 5 (wins,losses,drops,rating,awards?) * 6 (games) longs? -> 120 bytes
		respShort(recordSize);
		respBytes(&recvBuffer[n], recordSize);
		send();
	}

	void getTopUserList()
	{
		// nba2K2
		// 06 00 66 1b 01 00
		// resp:
		// 7a 04	len (1146!)
		// 67 1b 02 00 b2
		//             178?
		// 07  00 44 43 6d 61 6e 32 31   . .DCman21
		// player name
		// 33 00 00 00 1f 00 00 00  0a 00 00 00   3....... ....
		//    12 bytes data
		// 08 00 6e 63   ..nc
		// player name
		// 6d 61 6e 30 37 31 32 00  00 00 34 00 00 00 0d 00   man0712. ..4.....
		//                   12 bytes data...
		// 00 00
		// 0e 00 67 61 6d 69  6e 67 74 72 75 63 6b 65   ....gami ngtrucke
		// 72 31 27 00 00 00 0e 00  00 00 04 00 00 00
		//          12 bytes data
		// end:
		// 00 00
		std::vector<HighScore> hiScores = getHighScores(gameType);
		respond(7015);
		respShort(2);						// must be 2 (nba2k2)
		respByte(0x80 | hiScores.size());	// 0x80: success
											// | 0-50: number of records
		for (HighScore hs : hiScores)
		{
			respStr(hs.name);
			respLong(hs.wins);		// wins
			respLong(hs.losses);	// losses
			respLong(hs.data3);		// nfl2k2,nba2k2,ncaa2k2:drops
			if (hs.data4 != -1)
				respLong(hs.data4);		//wsb2k2:drops, not for nba2K2
		}
		send();
	}

	void getRegions()
	{
		// nfl2k1 resp:
		// 17 00 a1 0f 01 0a 00 53  68 75 6f 47 61 72 64 65   .......S huoGarde
	    // 6e 00 00 00 00 00 00
		printf("Get regions: len %d\n", recvShort(0));
		respond(4001);
		if (is2K1())
			respByte(1);	// number of regions: { string name, long user count }
		else
			respByte(0x81);
		respStr(RegionName);
		respShort(lobby->users.size());
		respSkip(4);
		send();
	}

	void getLobbies()
	{
		// nfl2k1 resp:
		// 1b 00 a3 0f 01 08 00 53  68 75 6d 61 6e 69 61 92   .......S humania.
	    // b9 87 b3 36 b4 00 00 00  00 00 00
		std::string region = recvStr(&recvBuffer[4]);
		printf("Get lobby servers: region %s\n", region.c_str());

		respond(4003);
		if (is2K1()) {
			respByte(1);
		}
		else {
			respShort(0);
			respByte(0x81);
		}
		respStr(LobbyName);
		// ip address, port
		respData(socket.local_endpoint().address().to_v4().to_bytes());
		respShort(htons(socket.local_endpoint().port() + 1));
		respShort(lobby->users.size());
		respSkip(4);
		send();
	}

	void findUser()
	{
		// 0c 00 32 00 06 00 61 62 63 64 65 66 ..2...abcdef
		std::string userName = recvStr(&recvBuffer[4]);
		for (const User::Ptr& user : lobby->users)
		{
			if (user->name == userName)
			{
				printf("Found user: %s\n", userName.c_str());
				if (is2K1()) {
					respond(401);
					respLong(1);	// success
				}
				else
				{
					respond(51);
					respShort(0);	// success
					respLong(1);	// NFL2K2 needs 1
				}
				respStr(RegionName);
				respStr(LobbyName);
				respData(socket.local_endpoint().address().to_v4().to_bytes());
				respShort(htons(socket.local_endpoint().port() + 1));
				send();
				return;
			}
		}
		printf("User not found: %s\n", userName.c_str());
		if (is2K1()) {
			respond(401);
			respLong(0);	// error
		}
		else
		{
			respond(51);
			respShort(1);	// error
			respStr("User not found in any lobby");
		}
		send();
	}

	void notifyLobbyRegister(const std::string& user)
	{
		std::vector<std::string> users;
		for (const auto& user : lobby->users)
			users.push_back(user->name);
		std::sort(users.begin(), users.end());
		discordLobbyJoined(gameType, user, users);
	}

	void lobbyRegister()
	{
		std::string userName = recvStr(&recvBuffer[13]);
		uint16_t recordSize = recvShort(15 + userName.length());
		uint8_t *record = &recvBuffer[17 + userName.length()];
		user = lobby->addUser(userName, this, recordSize, record);
		// record sizes vary: floigan?:0, nfl2k2:20, others:16
		printf("Enter lobby: user %s record: %d bytes\n", userName.c_str(), recordSize);

		if (gameType == OOOGABOOGA) {
			lobbyUserList();
		}
		else
		{
			respond(61);
			respShort(user->userId);
			respSkip(5);
			send();
		}
		notifyLobbyRegister(userName);
	}
	void lobbyRegister2K1()
	{
		// nfl 2k1
		// 19 00 c8 00 00 00 00 00  00 00 00 00 04 17 00 00 00 06 00 66 6c 79 69 6e 67
		//                                                     user name
		// nba 2k1
		// 20 00 ce 00 00 00 00 00  00 00 00 00 04 1a 00 00 00 06 00 66 6c 79 69 6e  67 03 00 00 00 00 00 00
		//                                                     user name
		std::string userName = recvStr(&recvBuffer[17]);
		user = lobby->addUser(userName, this, 0, nullptr);
		printf("Enter lobby(2k1): user %s\n", userName.c_str());
		// resp:
		// 0b 00 59 02 02 00 00 00  00 00 00
		respond(601);
		respShort(user->userId);
		respSkip(5);
		send();
		notifyLobbyRegister(userName);
	}

	void lobbyUserList()
	{
		respond(211);
		respShort(0);
		respShort(user->userId);
		send();
		for (size_t i = 0; i < lobby->users.size(); i++)
			sendUser(i);
	}
	void lobbyUserList2K1()
	{
		// 08 00 5a 02 00 00 00 00
		// resp:
		// 08 00 c9 00 00 00 02 00
		respond(201);
		respShort(0);
		respShort(user->userId);
		send();
		// 13 00 2d 01 01 02 00 06  00 66 6c 79 69 6e 67 b0   ..-..... .flying.
	    // 94 41 49
		for (size_t i = 0; i < lobby->users.size(); i++)
			sendUser(i);
	}

	void lobbyLeave()
	{
		if (lobby) {
			lobby->deleteUser(this);
			lobby.reset();
		}
	}

	void getGameList()
	{
		/*
		// 01 00 00 00 81 01 00 08 00 53 63 72 69 76 61 6e 69 04 00 0c 00 00 00 01 02 05 00 00 00 00 41 00 00 00 00
		//                            S  c  r  i  v  a  n  i
		// type:makahuna, island:fatty tutorial
		// host:Scrivani, round_reset:off
		respLong(1);
		respByte(0x81);
		respShort(1);
		respStr("Scrivani");
		respShort(4);
		respLong(0xc);
		respByte(1);	// island: 1=fatty tutorial, 9=spider beach, 19=twitchy tutorial, 31=hoohoo tutorial, 49=hottie tutorial, 60=boar polo arena 1, 3a=bruise box
		respByte(2);	// min round time (min)
		respByte(5);	// points to win
		respByte(0);	// number of birds
		respByte(0);	// number of boars
		respByte(0);	// number of tikis
		respByte(0);	// round reset: 0=off, 1=on
		respByte(0x41);	// max players/cur players
		respByte(0);	// enabled stuff: bit0:mine spell, bit1:fireball spell, ...
		respByte(0);
		respByte(0);
		respByte(0);	// 3 with full vmu?
		send();
		*/
		for (const Game& game : lobby->games)
			sendGameUpdate(game);
	}

	void createUpdateGame()
	{
		// 2c 00 8e 02 01 00 00 00 0a 00 50 6c 61 79 65 72 ,.........Player
		//                         host name
		// 5f 31 32 33 02 00 00 00 00 00 00 00 0c 00 00 00 _123............
		//
		// 01 02 05 00 00 00 00 41 00 00 00 00 .......A....
		//   rd time,
		std::string host = recvStr(&recvBuffer[8]);
		int n = 18 + host.length();
		int paramSize = recvShort(n);
		n += 2;
		Game *game = lobby->updateGame(this, host, paramSize, &recvBuffer[n], recvShort(n + paramSize));
		if (game == nullptr)
			return;

		printf("Game created/updated by %s: type/island %x\n", host.c_str(), recvBuffer[n + 2]);

		// 0a 00 8f 02 00 00 00 00  01 00
		respond(655);
		respSkip(4);
		respShort(game->gameId);
		send();
	}

	void leaveGame()
	{
		// 0a 00 90 02 01 00 00 00 00 00
		// or
		// 0a 00 90 02 01 00 00 00 01 00
		int gameId = recvShort(8);
		if (!lobby->deleteGame(user, gameId))
			fprintf(stderr, "ERROR: Game by host %s not found\n", user->name.c_str());
	}

	void chatSent()
	{
		// 0a 00 f4 01 01 00 01 01 00 73
		// 2k1:
		// 0b 00 f4 01 01 00 03 00 6c 6f 6c
		int n = is2K1() ? 6 : 7;
		std::string chat = recvStr(&recvBuffer[n]);
		printf("Chat from %s: %s\n", user->name.c_str(), chat.c_str());
		lobby->sendChat(user, chat);
	}

	void respond(uint16_t type)
	{
		assert(packetStart == 0);
		packetStart = sendIdx;
		*(uint16_t *)&sendBuffer[sendIdx + 2] = type;
		sendIdx += 4;
	}
	void respSkip(int n) {
		memset(&sendBuffer[sendIdx], 0, n);
		sendIdx += n;
	}
	void respByte(uint8_t v) {
		sendBuffer[sendIdx++] = v;
	}
	void respShort(uint16_t v) {
		*(uint16_t *)&sendBuffer[sendIdx] = v;
		sendIdx += 2;
	}
	void respLong(uint32_t v) {
		*(uint32_t *)&sendBuffer[sendIdx] = v;
		sendIdx += 4;
	}
	void respBytes(uint8_t *data, int size)
	{
		memcpy(&sendBuffer[sendIdx], data, size);
		sendIdx += size;
	}
	template<typename T>
	void respData(T v) {
		respBytes(v.data(), v.size());
	}
	void respStr(const std::string& str)
	{
		*(uint16_t *)&sendBuffer[sendIdx] = str.length();
		sendIdx += 2;
		memcpy(&sendBuffer[sendIdx], str.c_str(), str.length());
		sendIdx += str.length();
	}

	std::string recvStr(const uint8_t *data) {
		int len = *(uint16_t *)data;
		return std::string(data + 2, data + 2 + len);
	}
	std::string recvStr(size_t offset) {
		return recvStr(&recvBuffer[offset]);
	}

	uint16_t recvShort(size_t offset) {
		return *(uint16_t *)&recvBuffer[offset];
	}

	asio::ip::address_v4 address() {
		return socket.remote_endpoint().address().to_v4();
	}

	void dumpMsg()
	{
		size_t msgLen = *(uint16_t *)&recvBuffer[0];
		for (size_t i = 0; i < msgLen;)
		{
			char ascii[17] {};
			for (int j = 0; j < 16 && i + j < msgLen; j++) {
				uint8_t b = recvBuffer[i + j];
				printf("%02x ", b);
				if (b >= ' ' && b < 0x7f)
					ascii[j] = (char)b;
				else
					ascii[j] = '.';
			}
			printf("%s\n", ascii);
			i += 16;
		}
	}
	void dumpSendMsg()
	{
		size_t msgLen = *(uint16_t *)&sendBuffer[0];
		for (size_t i = 0; i < msgLen;)
		{
			char ascii[17] {};
			for (int j = 0; j < 16 && i + j < msgLen; j++) {
				uint8_t b = sendBuffer[i + j];
				printf("%02x ", b);
				if (b >= ' ' && b < 0x7f)
					ascii[j] = (char)b;
				else
					ascii[j] = '.';
			}
			printf("%s\n", ascii);
			i += 16;
		}
	}

	asio::io_context& io_context;
	asio::ip::tcp::socket socket;
	std::vector<uint8_t> recvBuffer;
	std::array<uint8_t, 1024> sendBuffer;
	size_t sendIdx = 0;
	bool sending = false;
	size_t packetStart = 0;

	std::shared_ptr<Lobby> lobby;
	User::Ptr user;
	GameType gameType;

	friend super;
};

User::Ptr Lobby::addUser(const std::string& name, Connection *connection, uint16_t recSize, uint8_t *record)
{
	users.push_back(User::create(name, connection, nextUserId(), recSize, record));
	for (size_t i = 0; i < users.size() - 1; i++)
		users[i]->connection->sendUser(users.size() - 1);
	return users.back();
}

void Lobby::sendChat(const User::Ptr& srcUser, const std::string& chatMsg) {
	for (const User::Ptr& user : users)
		user->connection->sendChat(srcUser->userId, chatMsg);
}

void Lobby::sendGameUpdate(const Game& game, User::Ptr exceptTo)
{
	for (const User::Ptr& user : users)
		if (user != exceptTo)
			user->connection->sendGameUpdate(game);
}

void Lobby::sendGameDelete(uint16_t gameId)
{
	for (const User::Ptr& user : users)
		user->connection->sendGameDelete(gameId);
}

void Lobby::sendUserLeave(uint16_t userId) {
	for (const User::Ptr& user : users)
		user->connection->sendUserLeave(userId);

}

class Server : public SharedThis<Server>
{
public:
	void start()
	{
		Connection::Ptr newConnection = Connection::create(io_context, lobby);

		acceptor.async_accept(newConnection->getSocket(),
				std::bind(&Server::handleAccept, shared_from_this(), newConnection, asio::placeholders::error));
	}

private:
	Server(asio::io_context& io_context, uint16_t port, std::shared_ptr<Lobby> lobby = nullptr)
		: io_context(io_context),
		  acceptor(asio::ip::tcp::acceptor(io_context,
				asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))),
		 lobby(lobby)
	{
		asio::socket_base::reuse_address option(true);
		acceptor.set_option(option);
	}

	void handleAccept(Connection::Ptr newConnection, const std::error_code& error)
	{
		if (!error)
			newConnection->receive();
		start();
	}

	asio::io_context& io_context;
	asio::ip::tcp::acceptor acceptor;
	std::shared_ptr<Lobby> lobby;

	friend super;
};

static void breakhandler(int signum) {
	io_context.stop();
}

static void loadConfig(const std::string& path)
{
	std::filebuf fb;
	if (!fb.open(path, std::ios::in)) {
		fprintf(stderr, "ERROR: config file %s not found\n", path.c_str());
		return;
	}

	std::istream istream(&fb);
	std::string line;
	while (std::getline(istream, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		auto pos = line.find_first_of("=:");
		if (pos != std::string::npos)
			Config[line.substr(0, pos)] = line.substr(pos + 1);
		else
			fprintf(stderr, "ERROR: config file syntax error: %s\n", line.c_str());
	}
	if (Config.count("REGION") > 0)
		RegionName = Config["REGION"];
	if (Config.count("LOBBY") > 0)
		LobbyName = Config["LOBBY"];
	if (Config.count("DATABASE") > 0)
		setDatabasePath(Config["DATABASE"]);
	else
		setDatabasePath("vcserver.db");
	if (Config.count("DISCORD_WEBHOOK") > 0)
		setDiscordWebhook(Config["DISCORD_WEBHOOK"]);
}

int main(int argc, char *argv[])
{
	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = breakhandler;
	sigact.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);
	printf("VC game server starting\n");

	loadConfig(argc >= 2 ? argv[1] : "vcserver.cfg");

	// ooga, floigan, IGP, 2k2 games
	Server::Ptr authAcceptor = Server::create(io_context, 11000);
	authAcceptor->start();

	std::vector<std::shared_ptr<Lobby>> lobbies;
	// record server port is 12001 + gameType * 100
	// region server port is 15003 + gameType * 100
	// and we set the lobby port at 15004 + gameType * 100
	std::vector<Server::Ptr> servers;
	for (int i = OOOGABOOGA; i <= NCAA2K2; i++)
	{
		auto lobby = std::make_shared<Lobby>((GameType)i);
		lobbies.push_back(lobby);
		servers.push_back(Server::create(io_context, 15003 + i * 100, lobby));
		servers.back()->start();
		servers.push_back(Server::create(io_context, 15004 + i * 100, lobby));
		servers.back()->start();
		if (i != FLOIGAN) {
			servers.push_back(Server::create(io_context, 12001 + i * 100, lobby));
			servers.back()->start();
		}
	}

	// nfl 2k1, nba 2k1
	Server::Ptr authAcceptor2k1 = Server::create(io_context, 14001);
	authAcceptor2k1->start();
	// nfl 2K1
	auto lobby = std::make_shared<Lobby>(NFL2K1);
	lobbies.push_back(lobby);
	servers.push_back(Server::create(io_context, 14003, lobby));
	servers.back()->start();
	servers.push_back(Server::create(io_context, 14004, lobby));
	servers.back()->start();
	// nba 2k1
	lobby = std::make_shared<Lobby>(NBA2K1);
	lobbies.push_back(lobby);
	servers.push_back(Server::create(io_context, 14503, lobby));
	servers.back()->start();
	servers.push_back(Server::create(io_context, 14504, lobby));
	servers.back()->start();

	io_context.run();

	closeDatabase();
	printf("VC game server stopping\n");

	return 0;
}

