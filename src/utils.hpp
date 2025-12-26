#pragma once
#include <type_traits>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprlang.hpp>

#include "globals.hpp"

enum class Direction { Up, Right, Down, Left };

std::string toLower(const std::string& str);
std::optional<Direction> directionFromString(const std::string& str);
std::string directionToString(const Direction dir);
std::string getGeometryString(const CBox geometry);
std::optional<size_t> getFocusHistoryIndex(PHLWINDOW wnd);
SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen = true);
SemmetyWorkspaceWrapper* workspace_for_window(PHLWINDOW window);
void focusWindow(PHLWINDOWREF window);
std::string windowToString(PHLWINDOWREF window);
std::string getCallStackAsString(int maxFrames);

void updateBar();
void shouldUpdateBar();

template <>
struct std::formatter<eFullscreenMode, char>: std::formatter<std::string_view, char> {
	auto format(const eFullscreenMode& mode, std::format_context& ctx) const
	    -> std::format_context::iterator {
		std::string_view name = "UNKNOWN";
		switch (mode) {
		case FSMODE_NONE: name = "FSMODE_NONE"; break;
		case FSMODE_MAXIMIZED: name = "FSMODE_MAXIMIZED"; break;
		case FSMODE_FULLSCREEN: name = "FSMODE_FULLSCREEN"; break;
		case FSMODE_MAX: name = "FSMODE_MAX"; break;
		}
		return std::formatter<std::string_view, char>::format(name, ctx);
	}
};

// abandon hope all ye who enter here
class HyprlangUnspecifiedCustomType {};
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
