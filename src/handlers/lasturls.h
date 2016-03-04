#pragma once

#include "lemonhandler.h"

#include <list>
#include <thread>

class event_base;
class evhttp_request;
class event;

class LastURLs : public LemonHandler
{
public:
	LastURLs(LemonBot *bot);
	~LastURLs();
	ProcessingResult HandleMessage(const std::string &from, const std::string &body);
	const std::string GetVersion() const;
	const std::string GetHelp() const;

private:
	std::list<std::string> _urlHistory;
	static constexpr int maxLength = 100;

	// FIXME put common code in util somewhere
	bool InitLibeventServer();

	std::thread _httpServer;
	event_base *_eventBase = nullptr;
	event *_breakLoop = nullptr;

	friend void terminateServerUrls(int, short int, void *arg);
	friend void httpServerThreadUrls(LastURLs * parent, std::uint16_t port);
	friend void httpHandlerUrls(evhttp_request *request, void *arg);
};