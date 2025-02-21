
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

SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen = true) {
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
