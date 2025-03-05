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

SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen = false) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return nullptr;
	}

	if (g_pLayoutManager->getCurrentLayout() != layout) {
		return nullptr;
	}

	auto workspace = g_pCompositor->m_pLastMonitor->activeSpecialWorkspace;
	if (!valid(workspace)) {
		workspace = g_pCompositor->m_pLastMonitor->activeWorkspace;
	}

	if (!valid(workspace)) {
		return nullptr;
	}
	if (!allow_fullscreen && workspace->m_bHasFullscreenWindow) {
		return nullptr;
	}

	semmety_log(ERR, "getting workspace for action {}", workspace->m_iID);

	return &layout->getOrCreateWorkspaceWrapper(workspace);
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
		write(pipeSock[1].get(), &grandchild, sizeof(grandchild));
		// exit child
		_exit(0);
	}
	// run in parent
	read(pipeSock[0].get(), &grandchild, sizeof(grandchild));
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

size_t getWrappedOffsetIndex3(size_t index, int offset, size_t size) {
	// if (size == 0) {
	// 	semmety_critical_error("getWrappedOffsetIndex called with size 0");
	// }

	return ((index + offset) % size + size) % size;
}
