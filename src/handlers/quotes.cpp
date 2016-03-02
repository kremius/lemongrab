#include "quotes.h"

#include <leveldb/db.h>

#include <regex>
#include <iostream>
#include <chrono>

#include "util/stringops.h"

Quotes::Quotes(LemonBot *bot)
	: LemonHandler("quotes", bot)
	, _generator(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))
{
	leveldb::Options options;
	options.create_if_missing = true;

	leveldb::Status status = leveldb::DB::Open(options, "db/quotes", &_quotesDB);
	if (!status.ok())
		std::cerr << status.ToString() << std::endl;
}

bool Quotes::HandleMessage(const std::string &from, const std::string &body)
{
	if (body.length() < 3)
		return false;

	auto command = body.substr(0, 3);

	if (command == "!gq")
	{
		std::string id;
		if (body.length() > 4)
			id = body.substr(4);
		SendMessage(from + ": " + GetQuote(id));
		return true;
	}

	if (body.length() < 5)
		return false;

	auto parameter = body.substr(4);

	if (command == "!aq")
	{
		auto id = AddQuote(parameter);
		id.empty() ? SendMessage(from + ": can't add quote") : SendMessage(from + ": quote added with id " + id);
		return true;
	}

	if (command == "!dq")
	{
		DeleteQuote(parameter) ? SendMessage(from + ": quote deleted") : SendMessage(from + ": quote doesn't exist or access denied");
		return true;
	}

	if (command == "!fq")
	{
		auto searchResults = FindQuote(parameter);
		SendMessage(from + ": " + searchResults);
		return true;
	}

	return false;
}

const std::string Quotes::GetVersion() const
{
	return "0.1";
}

const std::string Quotes::GetHelp() const
{
	return "!aq %text% - add quote, !dq %id% - delete quote, !fq %regex% - find quote, !gq %id% - get quote, if id is empty then quote is random";
}

std::string Quotes::GetQuote(const std::string &id)
{
	if (!_quotesDB)
		return "database connection error";

	std::string lastid;
	std::string selectedId;
	_quotesDB->Get(leveldb::ReadOptions(), "lastid", &lastid);

	if (id.empty())
	{
		int nLastID = 1;
		try {
			nLastID = std::stoi(lastid);
		} catch (std::exception e) {
			return "database is empty";
		}
		std::uniform_int_distribution<int> dis(1, nLastID);
		auto randomID = dis(_generator);
		selectedId = std::to_string(randomID);
	} else {
		selectedId = id;
	}

	std::string quote;
	auto addResult = _quotesDB->Get(leveldb::ReadOptions(), selectedId, &quote);
	if (!addResult.ok())
		return "Quote not found or something exploded";

	return "(" + selectedId + "/" + lastid + ") \"" + quote + "\"";
}

std::string Quotes::AddQuote(const std::string &text)
{
	if (!_quotesDB)
	{
		std::cout << "database connection error" << std::endl;
		return "";
	}

	std::string sLastID("0");
	int lastID = 0;

	_quotesDB->Get(leveldb::ReadOptions(), "lastid", &sLastID);

	try {
		lastID = std::stoi(sLastID);
	} catch (std::exception e) {
		std::cout << "Something broke: " << e.what() << std::endl;
		return "";
	}

	lastID++;
	std::string id(std::to_string(lastID));
	auto addRecordStatus = _quotesDB->Put(leveldb::WriteOptions(), id, text);
	if (!addRecordStatus.ok())
	{
		std::cout << "leveldb::Put for quote failed" << std::endl;
		return "";
	}

	addRecordStatus = _quotesDB->Put(leveldb::WriteOptions(), "lastid", id);
	if (!addRecordStatus.ok())
	{
		std::cout << "leveldb::Put for lastid failed" << std::endl;
		return "";
	}

	return id;
}

bool Quotes::DeleteQuote(const std::string &id)
{
	if (!_quotesDB)
	{
		std::cout << "Database connection error" << std::endl;
		return false;
	}

	auto checkQuote = _quotesDB->Get(leveldb::ReadOptions(), id, nullptr);
	if (!checkQuote.ok())
		return false;

	auto removeStatus = _quotesDB->Delete(leveldb::WriteOptions(), id);
	return removeStatus.ok();
}

std::string Quotes::FindQuote(const std::string &request)
{
	if (!_quotesDB)
		return "database connection error";

	// FIXME put similar code from lastseen and this into utils?
	std::string searchResults("Matching quote IDs:");
	std::regex inputRegex;
	std::smatch regexMatch;
	try {
		inputRegex = std::regex(toLower(request));
	} catch (std::regex_error& e) {
		return "Something broke: " + std::string(e.what());
	}

	int matches = 0;

	std::string onlyQuote;
	std::string onlyQuoteID;
	std::string lastID;

	std::shared_ptr<leveldb::Iterator> it(_quotesDB->NewIterator(leveldb::ReadOptions()));
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		if (it->key().ToString() == "lastid")
		{
			lastID = it->value().ToString();
			continue;
		}

		bool doesMatch = false;
		try {
			auto quote = toLower(it->value().ToString());
			doesMatch = std::regex_search(quote, regexMatch, inputRegex);
		} catch (std::regex_error &e) {
			std::cout << "Regex exception thrown" << e.what() << std::endl;
		}

		if (doesMatch)
		{
			if (onlyQuote.empty())
			{
				onlyQuote = it->value().ToString();
				onlyQuoteID = it->key().ToString();
			}

			if (matches >= 10)
			{
				searchResults.append(" ... (too many matches)");
				return searchResults;
			}

			matches++;
			searchResults.append(" " + it->key().ToString());
		}
	}

	if (matches == 1)
	{
		return "(" + onlyQuoteID + "/" + lastID + ") \"" + onlyQuote + "\"";
	}

	return searchResults;
}