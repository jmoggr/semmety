#pragma once

#include <hyprland/src/debug/Log.hpp>

// declared in utils.cpp
std::string getSemmetyIndent();

template <typename... Args>
void semmety_log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	std::string indent = getSemmetyIndent();
	Debug::log(level, "[semmety] {}{}", indent, msg);
}

template <typename... Args>
[[noreturn]] void semmety_critical_error(std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	Debug::log(CRIT, "[semmety] {}", msg);
	throw std::runtime_error(msg);
}
