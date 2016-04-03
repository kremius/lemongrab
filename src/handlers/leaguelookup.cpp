#include "leaguelookup.h"

#include <glog/logging.h>
#include <curl/curl.h>

#include <json/json.h>

#include "util/stringops.h"
#include "util/curlhelper.h"

LeagueLookup::LeagueLookup(LemonBot *bot)
	: LemonHandler("leaugelookup", bot)
{
	if (bot)
	{
		_apiKey = bot->GetRawConfigValue("RiotApiKey");
		_region = bot->GetRawConfigValue("RiotRegion");
		_platformID = bot->GetRawConfigValue("RiotPlatform");
	}

	if (_apiKey.empty())
		LOG(WARNING) << "Riot API Key is not set";

	if (_region.empty() || _platformID.empty())
	{
		LOG(WARNING) << "Region / platform are not set, defaulting to EUNE";
		_region = "eune";
		_platformID = "EUN1";
	}
}

LemonHandler::ProcessingResult LeagueLookup::HandleMessage(const std::string &from, const std::string &body)
{
	if (_apiKey.empty())
		return ProcessingResult::KeepGoing;

	std::string args;
	if (getCommandArguments(body, "!ll", args))
	{
		SendMessage(lookupCurrentGame(args));
		return ProcessingResult::StopProcessing;
	}

	return ProcessingResult::KeepGoing;
}

const std::string LeagueLookup::GetVersion() const
{
	return "0.1";
}

const std::string LeagueLookup::GetHelp() const
{
	return "!ll %summonername% - check if summoner is currently in game";
}

LeagueLookup::RiotAPIResponse LeagueLookup::RiotAPIRequest(const std::string &request, Json::Value &output)
{
	auto apiResponse = CurlRequest(request);

	if (apiResponse.second == 404)
		return RiotAPIResponse::NotFound;

	if (apiResponse.second != 200)
	{
		LOG(ERROR) << "RiotAPI unexpected response: " << apiResponse.second;
		return RiotAPIResponse::UnexpectedResponseCode;
	}

	const auto &strResponse = apiResponse.first;

	Json::Reader reader;
	if (!reader.parse(strResponse.data(), strResponse.data() + strResponse.size(), output))
	{
		LOG(ERROR) << "Failed to parse JSON: " << strResponse;
		return RiotAPIResponse::InvalidJSON;
	}

	return RiotAPIResponse::OK;
}

std::string LeagueLookup::lookupCurrentGame(const std::string &name)
{
	auto id = getSummonerIDFromName(name);
	if (id == -1)
		return "Summoner not found";

	if (id == -2)
		return "Something went horribly wrong";

	std::string apiRequest = "https://" + _region + ".api.pvp.net/observer-mode/rest/consumer/getSpectatorGameInfo/"
			+ _platformID + "/" + std::to_string(id) + "?api_key=" + _apiKey;

	Json::Value response;

	switch (RiotAPIRequest(apiRequest, response))
	{
	case RiotAPIResponse::NotFound:
		return "Summoner is not currently in game";
	case RiotAPIResponse::UnexpectedResponseCode:
		return "RiotAPI returned unexpected return code";
	case RiotAPIResponse::InvalidJSON:
		return "RiotAPI returned invalid JSON";
	case RiotAPIResponse::OK:
	{
		auto players = getSummonerNamesFromJSON(response);

		std::string message = "Currently playing: { ";
		for (auto player : players)
			message += player + " ";
		message += "}";

		return message;
	}
	}

	return "This should never happen";
}

int LeagueLookup::getSummonerIDFromName(const std::string &name)
{
	std::string apiRequest = "https://" + _region + ".api.pvp.net/api/lol/" + _region + "/v1.4/summoner/by-name/" + name + "?api_key=" + _apiKey;

	Json::Value response;

	switch (RiotAPIRequest(apiRequest, response))
	{
	case RiotAPIResponse::NotFound:
		return -1;
	case RiotAPIResponse::UnexpectedResponseCode:
	case RiotAPIResponse::InvalidJSON:
		return -2;
	case RiotAPIResponse::OK:
		return getSummonerIdFromJSON(name, response);
	}

	return -2;
}

int LeagueLookup::getSummonerIdFromJSON(const std::string &name, const Json::Value &root)
{
	return root[toLower(name)]["id"].asInt();
}

std::list<std::string> LeagueLookup::getSummonerNamesFromJSON(const Json::Value &root)
{
	std::list<std::string> result;

	auto participants = root["participants"];
	if (!participants.isArray())
		return result;

	auto partNum = participants.size();

	for (Json::ArrayIndex index = 0; index < partNum; index++)
	{
		auto summonerName = participants[index]["summonerName"].asString();
		result.push_back(summonerName);
	}

	return result;
}

#ifdef _BUILD_TESTS // LCOV_EXCL_START

#include <gtest/gtest.h>

#include <fstream>
#include <vector>
#include <iostream>

TEST(LeagueLookupTest, PlayerList)
{
	LeagueLookup t(nullptr);

	std::ifstream apiresponse("test/getSpectatorGameInfo.json");
	std::vector<char> responseStr((std::istreambuf_iterator<char>(apiresponse)),
								  std::istreambuf_iterator<char>());

	Json::Value tmp;
	Json::Reader reader;

	std::list<std::string> expectedNames = {
		"Rito Xalean",
		"ephiphonee",
		"cilla16",
		"LaaaNce",
		"Rito Asepta",
		"TR10 EsOToxic ",
		"LYLsky",
		"ArtOfOneShoots",
		"PH SkidRow",
		"saulens437",
	};

	ASSERT_TRUE(reader.parse(responseStr.data(), responseStr.data() + responseStr.size(), tmp));

	auto res = t.getSummonerNamesFromJSON(tmp);

	auto got = res.begin();
	auto want = expectedNames.begin();

	while (got != res.end())
	{
		EXPECT_EQ(*want, *got);
		++got;
		++want;
	}
}

TEST(LeagueLookupTest, SummonerByNameJson)
{
	LeagueLookup t(nullptr);

	std::ifstream apiresponse("test/summonerByName.json");
	std::vector<char> responseStr((std::istreambuf_iterator<char>(apiresponse)),
								  std::istreambuf_iterator<char>());

	Json::Value tmp;
	Json::Reader reader;

	ASSERT_TRUE(reader.parse(responseStr.data(), responseStr.data() + responseStr.size(), tmp));

	EXPECT_EQ(20374680, t.getSummonerIdFromJSON("JazzUSCM", tmp));
}

#endif // LCOV_EXCL_STOP
