#include "bot.h"

#include "handlers/diceroller.h"
#include "handlers/urlpreview.h"
#include "handlers/lastseen.h"
#include "handlers/pager.h"
#include "handlers/githubwebhooks.h"
#include "handlers/goodenough.h"

#include "glooxclient.h"

Bot::Bot(XMPPClient *client, Settings &settings)
	: _xmpp(client)
	, _settings(settings)
{
	_xmpp->SetXMPPHandler(this);
}

void Bot::Run()
{
	_startTime = std::chrono::system_clock::now();
	_lastMessage = std::chrono::system_clock::now();

	RegisterHandler<DiceRoller>();
	RegisterHandler<UrlPreview>();
	RegisterHandler<LastSeen>();
	RegisterHandler<Pager>();
	RegisterHandler<GithubWebhooks>();
	RegisterHandler<GoodEnough>();

	_xmpp->Connect(_settings.GetUserJID(), _settings.GetPassword());
}

void Bot::OnConnect()
{
	const auto &muc = _settings.GetMUC();
	_xmpp->JoinRoom(muc);
}

void Bot::OnMessage(const std::string &nick, const std::string &text)
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

	if (text == "!die")
	{
		auto admin = GetRawConfigValue("admin");
		if (!admin.empty() && GetJidByNick(nick) == admin)
		{
			_messageHandlers.clear();
			_xmpp->Disconnect();
		}
		return;
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

void Bot::OnPresence(const std::string &nick, const std::string &jid, bool online, const std::string &newNick)
{
	bool newConnection = false;
	if (online)
	{
		newConnection = _nick2jid.insert(std::pair<std::string, std::string>(nick, jid)).second;
		_jid2nick[jid] = nick;
	}
	else
	{
		_nick2jid.erase(nick);
		if (!newNick.empty())
		{
			_nick2jid[newNick] = jid;
			_jid2nick[jid] = newNick;
		} else {
			_jid2nick.erase(jid);
		}
	}

	for (auto handler : _messageHandlers)
	{
		if (handler->HandlePresence(nick, jid, newConnection))
			break;
	}
}

std::string Bot::GetNickByJid(const std::string &jid) const
{
	auto nick = _jid2nick.find(jid);
	return nick != _jid2nick.end() ? nick->second : "";
}

std::string Bot::GetJidByNick(const std::string &nick) const
{
	auto jid = _nick2jid.find(nick);
	return jid != _nick2jid.end() ? jid->second : "";
}

void Bot::SendMessage(const std::string &text)
{
	std::lock_guard<std::mutex> lock(_sendMessageMutex);
	auto currentTime = std::chrono::system_clock::now();
	if (_lastMessage + std::chrono::seconds(_sendMessageThrottle) > currentTime)
	{
		_sendMessageThrottle++;
		std::this_thread::sleep_for(std::chrono::seconds(_sendMessageThrottle));
	} else {
		_sendMessageThrottle = 1;
	}

	_xmpp->SendMessage(text);
	_lastMessage = std::chrono::system_clock::now();
}

std::string Bot::GetRawConfigValue(const std::string &name) const
{
	return _settings.GetRawString(name);
}

const std::string Bot::GetVersion() const
{
	std::string version = "Core: 0.1 (" + std::string(__DATE__) + ") | Modules:";
	for (auto handler : _messageHandlers)
	{
		version.append(" [" + handler->GetName() + ": " + handler->GetVersion() + "]");
	}
	return version;
}

const std::string Bot::GetHelp(const std::string &module) const
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
