#include "discord.h"

#include <thread>

#include <cstdlib>
#include <iostream>
#include <hexicord/gateway_client.hpp>
#include <hexicord/rest_client.hpp>

#include <glog/logging.h>
#include "util/stringops.h"

#include <cpr/cpr.h>

Discord::Discord(LemonBot *bot)
	: LemonHandler("discord", bot)
{

}

LemonHandler::ProcessingResult Discord::HandleMessage(const ChatMessage &msg)
{
	if (msg._module_name != GetName()) {
		if (!_webhookURL.empty()) {
			cpr::Post(cpr::Url{_webhookURL},
					  cpr::Payload{
						  {"username", msg._nick},
						  {"content", msg._body}
					  });

		} else if (_channelID != 0) {
			rclient->sendTextMessage(_channelID, msg._body);
		}
	}

	if (msg._body == "!discord") {
		std::string result = "Discord users:";

		for (const auto &user : _users) {
			if (!user.second._status.empty()
					&& user.second._status != "offline") {
				result += "\n" + user.second._nick;
				if (user.second._status != "idle") {
					result += " (" + user.second._status + ")";
				}
			}
		}

		SendMessage(result);
		rclient->sendTextMessage(_channelID, result);
		return LemonHandler::ProcessingResult::StopProcessing;
	}

	if ((msg._body == "!jabber" || msg._body == "!xmpp")
			&& msg._module_name == "discord") {
		//SendMessage(_botPtr->GetOnlineUsers());
		rclient->sendTextMessage(_channelID, "```\n" + _botPtr->GetOnlineUsers()
								 + "```\n\nthis message is invisible to xmpp users to avoid highlighting");
		return ProcessingResult::StopProcessing;
	}

	return LemonHandler::ProcessingResult::KeepGoing;
}

Discord::~Discord()
{
	if (_isEnabled) {
		gclient->disconnect();
		ioService.stop();

		_clientThread.join();
	}
}

bool Discord::Init()
{
	auto botToken = GetRawConfigValue("discord.token");
	_selfWebhook = GetRawConfigValue("discord.webhookid");
	auto ownerIdStr = GetRawConfigValue("discord.owner");
	Hexicord::Snowflake ownerId = ownerIdStr.empty() ? Hexicord::Snowflake() : Hexicord::Snowflake(ownerIdStr);

	//auto guildIdStr = GetRawConfigValue("discord.guild");
	//Hexicord::Snowflake guildId = guildIdStr.empty() ? Hexicord::Snowflake() : Hexicord::Snowflake(guildIdStr);

	auto channelIdStr = GetRawConfigValue("discord.channel");
	_webhookURL = GetRawConfigValue("discord.webhook");

	if (botToken.empty()) {
		LOG(WARNING) << "Bot token is empty, discord module disabled";
		return false;
	}

	if (auto channelID = from_string<std::uint64_t>(channelIdStr)) {
		if (*channelID == 0) {
			LOG(WARNING) << "Invalid channel ID, discord disabled";
			return false;
		}

		_channelID = *channelID;
	} else {
		LOG(WARNING) << "ChannelID is not set, discord disabled";
		return false;
	}

	gclient.reset(new Hexicord::GatewayClient(ioService, botToken));
	rclient.reset(new Hexicord::RestClient(ioService, botToken));

	_clientThread = std::thread([=]() noexcept
	{
									nlohmann::json me;
									gclient->eventDispatcher.addHandler(Hexicord::Event::Ready, [&me](const nlohmann::json& json) {
										me = json["user"];
									});

									gclient->eventDispatcher.addHandler(Hexicord::Event::GuildMemberUpdate, [&](const nlohmann::json& json) {
										auto id = json["user"]["id"].get<std::string>();
										auto nickname = json["user"]["username"].get<std::string>();

										if (json["nick"].is_string()) {
											nickname = json["nick"].get<std::string>();
										}

										_users[id]._nick = nickname;
									});

									gclient->eventDispatcher.addHandler(Hexicord::Event::MessageCreate, [&](const nlohmann::json& json) {
										// Sender can be webhook. For such we need to use "webhook_id" instead of "id".
										Hexicord::Snowflake senderId(json["author"].count("id") ? json["author"]["id"].get<std::string>()
										: json["author"]["webhook_id"].get<std::string>());

										auto id = json["author"]["id"].get<std::string>();

										// Avoid responing to messages of bot.
										if (senderId == Hexicord::Snowflake(me["id"].get<std::string>())
												|| senderId == Hexicord::Snowflake(_selfWebhook)) return;

										std::string text = json["content"];

										this->SendMessage(_users[id]._nick + "> " + text);

										ChatMessage msg;
										msg._nick = _users[id]._nick;
										msg._body = text;
										msg._jid = _users[id]._username;
										msg._isAdmin = senderId == ownerId;
										msg._isPrivate = false;
										this->TunnelMessage(msg);
									});

									gclient->eventDispatcher.addHandler(Hexicord::Event::PresenceUpdate, [&](const nlohmann::json& json) {
										auto id = json["user"]["id"].get<std::string>();

										if (json["status"].is_string()) {
											_users[id]._status = json["status"].get<std::string>();
										}
									});

									gclient->eventDispatcher.addHandler(Hexicord::Event::GuildCreate, [&](const nlohmann::json& json) {
										auto members = json["members"];

										std::cout << members.dump();

										for (auto member : members) {
											auto id = member["user"]["id"].get<std::string>();

											if (member["nick"].is_string()) {
												_users[id]._nick = member["nick"].get<std::string>();
											} else {
												_users[id]._nick = member["user"]["username"].get<std::string>();
											};

											_users[id]._username = member["user"]["username"].get<std::string>() + "#" + member["user"]["discriminator"].get<std::string>();
										}
									});

									gclient->connect(rclient->getGatewayUrlBot().first,
									Hexicord::GatewayClient::NoSharding, Hexicord::GatewayClient::NoSharding,
									{
										{ "since", nullptr   },
										{ "status", "online" },
										{ "game", {{ "name", "LEMONGRAB"}, { "type", 0 }}},
										{ "afk", false }
									});

									ioService.run();
	});

	_isEnabled = true;
	return true;
}

void Discord::HandlePresence(const std::string &from, const std::string &jid, bool connected)
{

}

const std::string Discord::GetHelp() const
{
	return "?";
}

void Discord::SendToDiscord(std::string text)
{
	if (_isEnabled) {
		rclient->sendTextMessage(Hexicord::Snowflake(_channelID), text);
	}
}
