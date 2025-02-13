#pragma once

#include <hyprland/src/debug/Log.hpp>

template <typename... Args>
void semmety_log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	Debug::log(level, "[semmety] {}", msg);
}

template <typename... Args>
std::unique_ptr<std::runtime_error> semmety_critical_error(std::format_string<Args...> fmt, Args&&... args) {
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	Debug::log(CRIT, "[semmety] {}", msg);
  return std::make_unique<std::runtime_error>(msg);
}

