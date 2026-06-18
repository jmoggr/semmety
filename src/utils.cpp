#include "utils.hpp"
#include <cstdlib>
#include <sstream>
#include <string>

#include <cxxabi.h>
#include <execinfo.h>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/history/WindowHistoryTracker.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprlang.hpp>

#include "globals.hpp"
#include "log.hpp"
#include "src/SemmetyEventManager.hpp"

std::string getSemmetyIndent() {
	return g_SemmetyLayout ? std::string(g_SemmetyLayout->entryCount * 4, ' ') : "";
}
std::string getInitialDebugString() {
	return g_SemmetyLayout ? g_SemmetyLayout->debugStringOnEntry : "[not initialized]";
}
std::string getCurrentDebugString() {
	return g_SemmetyLayout ? g_SemmetyLayout->getDebugString() : "[not initialized]";
}

std::optional<size_t> getFocusHistoryIndex(PHLWINDOW wnd) {
	// CCompositor::m_windowFocusHistory was removed in 0.55; focus history now lives in the
	// dedicated window tracker. fullHistory() is ordered old -> new (back() is the most recently
	// focused), so convert the position into a "0 == most recently focused" rank to preserve the
	// original semantics relied on by getLastFocusedWindowIndex().
	const auto& history = Desktop::History::windowTracker()->fullHistory();
	for (size_t i = 0; i < history.size(); ++i) {
		if (history[i].lock() == wnd) { return history.size() - 1 - i; }
	}

	return std::nullopt;
}

std::string getGeometryString(const CBox geometry) {
	return std::format(
	    "{}, {}, {}, {}",
	    geometry.pos().x,
	    geometry.pos().y,
	    geometry.size().x,
	    geometry.size().y
	);
}

std::string directionToString(const Direction dir) {
	switch (dir) {
	case Direction::Up: return "Up";
	case Direction::Down: return "Down";
	case Direction::Left: return "Left";
	case Direction::Right: return "Right";
	default: return "Unknown";
	}
}

std::string toLower(const std::string& str) {
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), [](unsigned char c) {
		return std::tolower(c);
	});
	return result;
}

std::optional<Direction> directionFromString(const std::string& str) {
	const auto lowerStr = toLower(str);

	if (lowerStr == "l" || lowerStr == "left") return Direction::Left;
	else if (lowerStr == "r" || lowerStr == "right") return Direction::Right;
	else if (lowerStr == "u" || lowerStr == "up") return Direction::Up;
	else if (lowerStr == "d" || lowerStr == "down") return Direction::Down;
	else return {};
}

SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen) {
	auto layout = g_SemmetyLayout;
	if (layout == nullptr) { return nullptr; }

	auto monitor = Desktop::focusState()->monitor();
	if (!monitor) { return nullptr; }

	auto workspace = monitor->m_activeSpecialWorkspace;
	if (!valid(workspace)) { workspace = monitor->m_activeWorkspace; }

	if (!valid(workspace)) { return nullptr; }
	if (!allow_fullscreen && workspace->m_hasFullscreenWindow) { return nullptr; }

	return &layout->getOrCreateWorkspaceWrapper(workspace);
}

SemmetyWorkspaceWrapper* workspace_for_window(PHLWINDOW window) {
	if (!window || !window->m_workspace) { return nullptr; }

	for (auto& workspace: g_SemmetyLayout->workspaceWrappers) {
		if (window->m_workspace == workspace.workspace) { return &workspace; }
	}

	return &g_SemmetyLayout->getOrCreateWorkspaceWrapper(window->m_workspace);
}

std::string escapeSingleQuotes(const std::string& input) {
	std::ostringstream ss;
	for (char c: input) {
		switch (c) {
		case '\'': ss << "'\\\''"; break;
		default: ss << c;
		}
	}
	return ss.str();
}

void updateBar() {
	// Avoid creating workspace wrappers (and thus frames) before the event loop is running.
	if (!g_semmetyReady) { return; }

	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		semmety_log(Log::ERR, "no workspace");
		return;
	}

	const auto windowsJson = workspace_wrapper->getWorkspaceWindowsJson();
	const auto workspacesJson = g_SemmetyLayout->getWorkspacesJson();

	const json updateJson = {{"windows", windowsJson}, {"workspaces", workspacesJson}};

	auto jsonString = updateJson.dump();
	g_SemmetyEventManager->postBarUpdate(jsonString);
}

void focusWindow(PHLWINDOWREF window) {
	auto focused_window = Desktop::focusState()->window();
	if (focused_window == window) { return; }
	if (window) {
		semmety_log(Log::ERR, "Focusing window {}", window->fetchTitle());

		if (window->isHidden()) { window->setHidden(false); }
		Desktop::focusState()->fullWindowFocus(window.lock(), Desktop::FOCUS_REASON_OTHER);
	} else {
		Desktop::focusState()->resetWindowFocus();
	}
}

void shouldUpdateBar() { g_SemmetyLayout->_shouldUpdateBar = true; }

std::string windowToString(PHLWINDOWREF window) {
	return std::format("{:x}", (uintptr_t) window.get());
}

std::string getCallStackAsString() {
	const auto maxFrames = 64;
	std::vector<void*> addrList(maxFrames + 1);

	int addrLen = backtrace(addrList.data(), static_cast<int>(addrList.size()));

	if (addrLen == 0) { return "<empty, possibly corrupt stack>"; }

	char** symbolList = backtrace_symbols(addrList.data(), addrLen);
	if (!symbolList) { return "<failed to obtain symbols>"; }

	std::ostringstream oss;
	for (int i = 0; i < addrLen; ++i) {
		std::string symbol(symbolList[i]);

		std::size_t begin = symbol.find('(');
		std::size_t end = symbol.find('+', begin);
		if (begin != std::string::npos && end != std::string::npos) {
			std::string mangled = symbol.substr(begin + 1, end - begin - 1);
			int status = 0;
			char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
			if (status == 0 && demangled) {
				symbol.replace(begin + 1, end - begin - 1, demangled);
				free(demangled);
			}
		}

		oss << symbol << "\n";
	}

	free(symbolList);
	return oss.str();
}
