#include "bot.h"

#include "handlers/diceroller.h"
#include "handlers/urlpreview.h"
#include "handlers/lastseen.h"

#include "glooxclient.h"

NewBot::NewBot(Settings &settings)
	: _settings(settings)
{
	_gloox = std::make_shared<GlooxClient>(this);
}

void NewBot::Run()
{
	_startTime = std::chrono::system_clock::now();
	RegisterHandler<DiceRoller>();
	RegisterHandler<UrlPreview>();
	RegisterHandler<LastSeen>();

	_gloox->Connect(_settings.GetUserJID(), _settings.GetPassword());
}

void NewBot::OnConnect()
{
	const auto &muc = _settings.GetMUC();
	_gloox->JoinRoom(muc);
}

void NewBot::OnMessage(const std::string &nick, const std::string &text)
{
	if (text == "!getversion")
		return SendMessage(GetVersion());

	if (text == "!uptime")
	{
		auto CurrentTime = std::chrono::system_clock::now();
		std::string uptime("Uptime: ");
		// FIXME There must be a way to make this line shorter
		uptime.append(std::to_string(std::chrono::duration_cast<std::chrono::duration<int, std::ratio<3600*24>>>
									 (CurrentTime - _startTime).count()));
		return SendMessage(uptime);
	}

	if (text.length() >= 5 && text.substr(0, 5) == "!help")
	{
		std::string module = "";
		if (text.length() > 6)
			module = text.substr(6);
		return SendMessage(GetHelp(module));
	}

	for (auto handler : _messageHandlers)
		if (handler->HandleMessage(nick, text))
			break;
}

void NewBot::OnPresence(const std::string &nick, const std::string &jid, bool connected)
{
	for (auto handler : _messageHandlers)
	{
		if (handler->HandlePresence(nick, jid, connected))
			break;
	}
}

void NewBot::SendMessage(const std::string &text) const
{
	_gloox->SendMessage(text);
}

const std::string NewBot::GetRawConfigValue(const std::string &name) const
{
	return _settings.GetRawString(name);
}

const std::string NewBot::GetVersion() const
{
	std::string version = "Core: 0.1 (" + std::string(__DATE__) + ") | Modules:";
	for (auto handler : _messageHandlers)
	{
		version.append(" " + handler->GetVersion());
	}
	return version;
}

const std::string NewBot::GetHelp(const std::string &module) const
{
	auto handler = _handlersByName.find(module);

	if (handler == _handlersByName.end())
	{
		std::string help = "Use !help %module_name%, where module_name is one of:";
		for (auto handlerPtr : _messageHandlers)
		{
			help.append(" " + handlerPtr->GetName());
		}
		return help;
	} else {
		return handler->second->GetHelp();
	}
}
