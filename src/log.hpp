#pragma once

#include <hyprland/src/debug/Log.hpp>
static int indentLevel = 0;

template <typename... Args>
void semmety_log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));

	if (msg.starts_with("EXIT")) {
		if (indentLevel > 0) {
			--indentLevel;
		}
	}

	std::string indent(indentLevel * 4, ' ');

	Debug::log(level, "[semmety] {}{}", indent, msg);

	if (msg.starts_with("ENTER")) {
		++indentLevel;
	}
}

template <typename... Args>
[[noreturn]] void semmety_critical_error(std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	Debug::log(CRIT, "[semmety] {}", msg);
	throw std::runtime_error(msg);
}
