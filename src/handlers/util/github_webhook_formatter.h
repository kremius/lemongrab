#pragma once

#include <string>
#include <vector>
#include <map>

class GithubWebhookFormatter
{
public:
	enum class FormatResult
	{
		JSONParseError,
		IgnoredHook,
		OK,
	};

	static FormatResult FormatWebhookMessage(const std::string &hookType, const std::vector<char> &input, std::string &output);
};
