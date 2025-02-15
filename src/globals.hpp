#pragma once

#include <type_traits>

#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprlang.hpp>

#include "SemmetyLayout.hpp"
#include "log.hpp"

inline HANDLE PHANDLE = nullptr;
inline std::unique_ptr<SemmetyLayout> g_SemmetyLayout;

class HyprlangUnspecifiedCustomType {};

// abandon hope all ye who enter here
template <typename T, typename V = HyprlangUnspecifiedCustomType>
class ConfigValue {
public:
	ConfigValue(const std::string& option) {
		this->static_data_ptr = HyprlandAPI::getConfigValue(PHANDLE, option)->getDataStaticPtr();
	}

	template <typename U = T>
	typename std::enable_if<std::is_same<U, Hyprlang::CUSTOMTYPE>::value, const V&>::type
	operator*() const {
		return *(V*) ((Hyprlang::CUSTOMTYPE*) *this->static_data_ptr)->getData();
	}

	template <typename U = T>
	typename std::enable_if<std::is_same<U, Hyprlang::CUSTOMTYPE>::value, const V*>::type
	operator->() const {
		return &**this;
	}

	// Bullshit microptimization case for strings
	template <typename U = T>
	typename std::enable_if<std::is_same<U, Hyprlang::STRING>::value, const char*>::type
	operator*() const {
		return *(const char**) this->static_data_ptr;
	}

	template <typename U = T>
	typename std::enable_if<
	    !std::is_same<U, Hyprlang::CUSTOMTYPE>::value && !std::is_same<U, Hyprlang::STRING>::value,
	    const T&>::type
	operator*() const {
		return *(T*) *this->static_data_ptr;
	}

private:
	void* const* static_data_ptr;
};
