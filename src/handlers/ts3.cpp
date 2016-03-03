#include "ts3.h"

#include "util/stringops.h"

#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>

#include <iostream>

TS3::TS3(LemonBot *bot)
	: LemonHandler("ts3", bot)
{
	StartServerQueryClient();
}

bool TS3::HandleMessage(const std::string &from, const std::string &body)
{
	if (body == "!ts")
	{
		if (_clients.size() == 0)
			SendMessage("So ronery");
		else
		{
			std::string online;
			for (auto &user : _clients)
				online.append(" " + user.second);

			SendMessage("Current TeamSpeak users online:" + online);
		}
		return true;
	}
	return false;
}

const std::string TS3::GetVersion() const
{
	return "0.1";
}

const std::string TS3::GetHelp() const
{
	return "!ts - get online teamspeak users";
}

void eventcb(bufferevent *bev, short event, void *arg)
{
	if (event & BEV_EVENT_CONNECTED) {
		std::cout << "[TS3] Connected" << std::endl;
	} else if (event & BEV_EVENT_TIMEOUT) {
		std::cout << "[TS3] Sending keepalive" << std::endl;
		bufferevent_enable(bev, EV_READ|EV_WRITE);
		evbuffer_add_printf(bufferevent_get_output(bev), "whoami\n");
	} else if (event & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
		auto parent = static_cast<TS3*>(arg);
		if (event & BEV_EVENT_ERROR) {
			int err = bufferevent_socket_get_dns_error(bev);
			if (err)
				std::cout << "[TS3] DNS error: " << evutil_gai_strerror(err) << std::endl;
		}
		std::cout << "[TS3] Connection closed" << std::endl;
		// FIXME needs a reliable restart mechanism, see thread
		parent->SendMessage("TS3 ServerQuery connection closed, fix me please");
		bufferevent_free(bev);
		event_base_loopexit(parent->base, nullptr);
	}
}

void readcb(bufferevent *bev, void *arg)
{
	auto parent = static_cast<TS3*>(arg);
	std::string s;
	char buf[1024];
	int n;
	evbuffer *input = bufferevent_get_input(bev);
	while ((n = evbuffer_remove(input, buf, sizeof(buf))) > 0)
		s.append(buf, n);

	std::cout << "[TS3] >>> " << s << std::endl;

	switch (parent->_sqState)
	{
	case TS3::State::NotConnected:
		// FIXME: very poor criteria for connection
		static const std::string welcome = "Welcome to the TeamSpeak 3 ServerQuery interface";
		if (s.find(welcome) != s.npos)
		{
			parent->_sqState = TS3::State::ServerQueryConnected;
			std::string loginString = "login " + parent->GetRawConfigValue("TS3QueryLogin")
					+ " " + parent->GetRawConfigValue("TS3QueryPassword") + "\n";
			evbuffer_add_printf(bufferevent_get_output(bev), loginString.c_str());
		}
		break;

	case TS3::State::ServerQueryConnected:
		static const std::string ok = "error id=0 msg=ok";
		if (s.find(ok) == 0)
		{
			parent->_sqState = TS3::State::Authrozied;
			evbuffer_add_printf(bufferevent_get_output(bev), "use port=9987\n");
		}
		break;

	case TS3::State::Authrozied:
		if (s.find(ok) == 0)
		{
			parent->_sqState = TS3::State::VirtualServerConnected;
			evbuffer_add_printf(bufferevent_get_output(bev), "servernotifyregister event=server\n");
		}
		break;

	case TS3::State::VirtualServerConnected:
		if (s.find(ok) == 0)
			parent->_sqState = TS3::State::Subscribed;
		break;

	case TS3::State::Subscribed:
		if (s.find("notifycliententerview") == 0)
		{
			// FIXME we need only 1-2 tokens, no need to tokenize whole thing?
			auto tokens = tokenize(s, ' ');
			try {
				parent->Connected(tokens.at(4).substr(5), tokens.at(6).substr(16));
			} catch (std::exception e) {
				std::cout << "Something broke: " << e.what() << std::endl;
			}
			break;
		}

		if (s.find("notifyclientleftview") == 0)
		{
			auto tokens = tokenize(s, ' ');
			try {
				parent->Disconnected(tokens.at(5).substr(5, tokens.at(5).size() - 7)); // FIXME: account for CR LF
			} catch (std::exception e) {
				std::cout << "Something broke: " << e.what() << std::endl;
			}
			break;
		}
		break;
	}

}

void telnetClientThread(TS3 * parent, std::string server)
{
	int port = 10011;
	parent->base = event_base_new();
	parent->dns_base = evdns_base_new(parent->base, 1);

	parent->bev = bufferevent_socket_new(parent->base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(parent->bev, readcb, nullptr, eventcb, parent);
	bufferevent_enable(parent->bev, EV_READ|EV_WRITE);
	timeval t;
	t.tv_sec = 60;
	t.tv_usec = 0;
	bufferevent_set_timeouts(parent->bev, &t, nullptr);
	bufferevent_socket_connect_hostname(
		parent->bev, parent->dns_base, AF_UNSPEC, server.c_str(), port);

	// FIXME needs a reliable restart mechanism
	event_base_dispatch(parent->base);
}

void TS3::StartServerQueryClient()
{
	_telnetClient = std::thread (&telnetClientThread, this, GetRawConfigValue("ts3server"));
}

void TS3::Connected(const std::string &clid, const std::string &nick)
{
	_clients[clid] = nick;
	SendMessage("Teamspeak user connected: " + nick);
}

void TS3::Disconnected(const std::string &clid)
{
	auto nick = _clients[clid];
	SendMessage("Teamspeak user disconnected: " + nick);
	_clients.erase(clid);
}
