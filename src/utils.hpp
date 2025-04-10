#pragma once
#include <type_traits>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
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
void focusWindow(PHLWINDOWREF window);

size_t getWrappedOffsetIndex3(size_t index, int offset, size_t size);

void updateBar();

template <typename T, typename U>
Hyprutils::Memory::CSharedPointer<T>
hyprland_dynamic_pointer_cast(const Hyprutils::Memory::CSharedPointer<U>& ptr) {
	// Perform dynamic_cast on the raw pointer.
	if (T* casted = dynamic_cast<T*>(ptr.get())) {
		// If successful, create a new shared pointer that uses the same control block.
		return Hyprutils::Memory::CSharedPointer<T>(ptr.impl_);
	}
	// If the cast fails, return an empty pointer.
	return Hyprutils::Memory::CSharedPointer<T>(nullptr);
}

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
