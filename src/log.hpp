#pragma once

#include <hyprland/src/debug/Log.hpp>

// declared in utils.cpp
std::string getSemmetyIndent();
std::string getInitialDebugString();
std::string getCurrentDebugString();
std::string getCallStackAsString();

template <typename... Args>
void semmety_log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	// TODO: handle if you can't get the indent
	std::string indent = getSemmetyIndent();
	Debug::log(level, "[semmety] {}{}", indent, msg);
}

template <typename... Args>
[[noreturn]] void semmety_critical_error(std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));

	std::string out = "";
	out += "[semmety] " + msg;
	out += "\ncallstack:\n" + getCallStackAsString();
	out += "\ninitial state:\n" + getInitialDebugString();
	out += "\ncurrent state:\n" + getCurrentDebugString();

	Debug::log(CRIT, "{}", out);
	throw std::runtime_error(out);
}
