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
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprlang.hpp>

#include "globals.hpp"
#include "log.hpp"
#include "src/desktop/DesktopTypes.hpp"

// defined in log.hpp
std::string getSemmetyIndent() { return std::string(g_SemmetyLayout->entryCount * 4, ' '); }
std::string getInitialDebugString() { return g_SemmetyLayout->debugStringOnEntry; }
std::string getCurrentDebugString() { return g_SemmetyLayout->getDebugString(); }

std::optional<size_t> getFocusHistoryIndex(PHLWINDOW wnd) {
	const auto& history = g_pCompositor->m_vWindowFocusHistory;

	for (size_t i = 0; i < history.size(); ++i) {
		if (history[i].lock() == wnd) return i;
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

// TODO: should it every be allowed to return null?
SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return nullptr;
	}

	if (g_pLayoutManager->getCurrentLayout() != layout) {
		return nullptr;
	}

	auto workspace = g_pCompositor->m_lastMonitor->activeSpecialWorkspace;
	if (!valid(workspace)) {
		workspace = g_pCompositor->m_lastMonitor->activeWorkspace;
	}

	if (!valid(workspace)) {
		return nullptr;
	}
	if (!allow_fullscreen && workspace->m_bHasFullscreenWindow) {
		return nullptr;
	}

	return &layout->getOrCreateWorkspaceWrapper(workspace);
}

SemmetyWorkspaceWrapper* workspace_for_window(PHLWINDOW window) {
	if (!window->m_workspace) return nullptr;

	for (auto& workspace: g_SemmetyLayout->workspaceWrappers) {
		if (window->m_workspace == workspace.workspace) {
			return &workspace;
		}
	}

	return &g_SemmetyLayout->getOrCreateWorkspaceWrapper(window->m_workspace);
}

uint64_t spawnRawProc(std::string args, PHLWORKSPACE pInitialWorkspace) {
	// Debug::log(LOG, "Executing {}", args);

	// const auto HLENV = getHyprlandLaunchEnv(pInitialWorkspace);

	int socket[2];
	if (pipe(socket) != 0) {
		Debug::log(LOG, "Unable to create pipe for fork");
	}

	Hyprutils::OS::CFileDescriptor pipeSock[2] = {
	    Hyprutils::OS::CFileDescriptor {socket[0]},
	    Hyprutils::OS::CFileDescriptor {socket[1]}
	};

	pid_t child, grandchild;
	child = fork();
	if (child < 0) {
		Debug::log(LOG, "Fail to create the first fork");
		return 0;
	}
	if (child == 0) {
		// run in child
		g_pCompositor->restoreNofile();

		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, nullptr);

		grandchild = fork();
		if (grandchild == 0) {
			// run in grandchild
			// for (auto const& e: HLENV) {
			// 	setenv(e.first.c_str(), e.second.c_str(), 1);
			// }
			setenv("WAYLAND_DISPLAY", g_pCompositor->m_szWLDisplaySocket.c_str(), 1);
			execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
			// exit grandchild
			_exit(0);
		}
		[[maybe_unused]] auto wret = write(pipeSock[1].get(), &grandchild, sizeof(grandchild));
		// exit child
		_exit(0);
	}
	// run in parent
	[[maybe_unused]] auto rret = read(pipeSock[0].get(), &grandchild, sizeof(grandchild));
	// clear child and leave grandchild to init
	waitpid(child, nullptr, 0);
	if (grandchild < 0) {
		Debug::log(LOG, "Fail to create the second fork");
		return 0;
	}

	Debug::log(LOG, "Process Created with pid {}", grandchild);

	return grandchild;
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
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		semmety_log(ERR, "no workspace");
		return;
	}

	const auto windowsJson = workspace_wrapper->getWorkspaceWindowsJson();
	const auto workspacesJson = g_SemmetyLayout->getWorkspacesJson();

	const json updateJson = {{"windows", windowsJson}, {"workspaces", workspacesJson}};

	auto jsonString = updateJson.dump();
	auto escapedJsonString = "\'" + escapeSingleQuotes(jsonString) + "\'";
	spawnRawProc(
	    "qs ipc call taskManager setWindows " + escapedJsonString,
	    workspace_wrapper->workspace.lock()
	);

	// semmety_log(ERR, "calling qs with {}", escapedJsonString);
}

void focusWindow(PHLWINDOWREF window) {
	auto focused_window = g_pCompositor->m_pLastWindow.lock();
	if (focused_window == window) {
		return;
	}
	if (window) {
		semmety_log(ERR, "Focusing window {}", window->fetchTitle());

		if (window->isHidden()) {
			window->setHidden(false);
		}
		g_pCompositor->focusWindow(window.lock());
	} else {
		g_pCompositor->focusWindow(nullptr);
	}
}

void shouldUpdateBar() { g_SemmetyLayout->_shouldUpdateBar = true; }

std::string windowToString(PHLWINDOWREF window) {
	return std::format("{:x}", (uintptr_t) window.get());
}

std::string getCallStackAsString() {
	const auto maxFrames = 64;
	// Create a vector to hold the stack addresses.
	std::vector<void*> addrList(maxFrames + 1);

	// Retrieve current stack addresses using the vector's data pointer.
	int addrLen = backtrace(addrList.data(), static_cast<int>(addrList.size()));

	if (addrLen == 0) {
		return "<empty, possibly corrupt stack>";
	}

	// Convert addresses to an array of symbolic strings.
	char** symbolList = backtrace_symbols(addrList.data(), addrLen);
	if (!symbolList) {
		return "<failed to obtain symbols>";
	}

	std::ostringstream oss;
	for (int i = 0; i < addrLen; ++i) {
		std::string symbol(symbolList[i]);

		// Optionally demangle the symbol name for readability.
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
