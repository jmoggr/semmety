#pragma once

#include <hyprland/src/debug/log/Logger.hpp>

// declared in utils.cpp
std::string getSemmetyIndent();
std::string getInitialDebugString();
std::string getCurrentDebugString();
std::string getCallStackAsString();

template <typename... Args>
void semmety_log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	std::string indent = getSemmetyIndent();
	Log::logger->log(level, "[semmety] {}{}", indent, msg);
}

template <typename... Args>
[[noreturn]] void semmety_critical_error(std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));

	std::string out = "";
	out += "[semmety] " + msg;
	out += "\ncallstack:\n" + getCallStackAsString();
	out += "\ninitial state:\n" + getInitialDebugString();
	out += "\ncurrent state:\n" + getCurrentDebugString();

	Log::logger->log(Log::CRIT, "{}", out);
	throw std::runtime_error(out);
}
