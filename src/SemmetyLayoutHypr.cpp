#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>

#include "SemmetyFrame.hpp"
#include "SemmetyLayout.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "src/desktop/DesktopTypes.hpp"
#include "src/layout/IHyprLayout.hpp"
#include "utils.hpp"

SP<HOOK_CALLBACK_FN> renderHookPtr;
SP<HOOK_CALLBACK_FN> tickHookPtr;
SP<HOOK_CALLBACK_FN> activeWindowHookPtr;
SP<HOOK_CALLBACK_FN> workspaceHookPtr;
SP<HOOK_CALLBACK_FN> urgentHookPtr;

void SemmetyLayout::onEnable() {
	for (auto& window: g_pCompositor->m_vWindows) {
		if (window->isHidden() || !window->m_bIsMapped || window->m_bFadingOut || window->m_bIsFloating)
			continue;

		this->onWindowCreatedTiling(window);
	}

	renderHookPtr =
	    HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", &SemmetyLayout::renderHook);

	tickHookPtr = HyprlandAPI::registerCallbackDynamic(PHANDLE, "tick", &SemmetyLayout::tickHook);

	activeWindowHookPtr = HyprlandAPI::registerCallbackDynamic(
	    PHANDLE,
	    "activeWindow",
	    &SemmetyLayout::activeWindowHook
	);

	workspaceHookPtr =
	    HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", &SemmetyLayout::workspaceHook);
	urgentHookPtr =
	    HyprlandAPI::registerCallbackDynamic(PHANDLE, "urgent", &SemmetyLayout::urgentHook);
}

void SemmetyLayout::onDisable() {
	renderHookPtr.reset();
	tickHookPtr.reset();
	activeWindowHookPtr.reset();
	workspaceHookPtr.reset();
	urgentHookPtr.reset();
}

void SemmetyLayout::onWindowCreatedTiling(PHLWINDOW window, eDirection direction) {
	semmety_log(LOG, "ENTER onWindowCreatedTiling");
	if (window->m_pWorkspace == nullptr) {
		semmety_log(
		    ERR,
		    "EXIT onWindowCreatedTiling -- called with a window that has an invalid workspace"
		);
		return;
	}

	semmety_log(
	    LOG,
	    "onWindowCreatedTiling called with window {:x} (floating: {}, monitor: {}, workspace: {})",
	    (uintptr_t) window.get(),
	    window->m_bIsFloating,
	    window->monitorID(),
	    window->m_pWorkspace->m_iID
	);

	if (window->m_bIsFloating) {
		semmety_log(LOG, "EXIT onWindowCreatedTiling -- window is floating");
		return;
	}

	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.addWindow(window);

	// TODO: is this correct?
	auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
	if (monitor != nullptr && monitor->activeWorkspace != nullptr) {
		if (monitor->activeWorkspace == window->m_pWorkspace) {
			updateBar();
		}
	}

	g_pAnimationManager->scheduleTick();
	semmety_log(LOG, "EXIT onWindowCreatedTiling");
}

void SemmetyLayout::onWindowRemovedTiling(PHLWINDOW window) {
	semmety_log(LOG, "ENTER onWindowRemovedTiling");
	if (window->m_pWorkspace == nullptr) {
		semmety_log(LOG, "EXIT onWindowRemovedTiling -- workspace is null");
		return;
	}

	semmety_log(
	    LOG,
	    "onWindowRemovedTiling window {:x} (floating: {}, monitor: {}, workspace: {}, title: {})",
	    (uintptr_t) window.get(),
	    window->m_bIsFloating,
	    window->monitorID(),
	    window->m_pWorkspace->m_iID,
	    window->fetchTitle()
	);
	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.removeWindow(window);

	updateBar();
	g_pAnimationManager->scheduleTick();
	semmety_log(LOG, "EXIT onWindowRemovedTiling");
}

void SemmetyLayout::onWindowFocusChange(PHLWINDOW window) {
	semmety_log(LOG, "ENTER onWindowFocusChange");
	auto title = window == nullptr ? "none" : window->fetchTitle();
	semmety_log(ERR, "focus changed for window {}", title);

	if (window == nullptr) {
		semmety_log(LOG, "EXIT onWindowFocusChange -- window is null");
		return;
	}

	if (window->m_pWorkspace == nullptr) {
		semmety_log(LOG, "EXIT onWindowFocusChange -- window workspace is null");
		return;
	}

	if (window->m_bIsFloating) {
		semmety_log(LOG, "EXIT onWindowFocusChange -- window is floating");
		return;
	}

	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.activateWindow(window);

	updateBar();
	semmety_log(LOG, "EXIT onWindowFocusChange");
}

void SemmetyLayout::recalculateMonitor(const MONITORID& monid) {
	semmety_log(LOG, "ENTER recalculateMonitor");
	const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

	if (!PMONITOR || !PMONITOR->activeWorkspace) {
		semmety_log(LOG, "EXIT recalculateMonitor -- null monitor or null workspace");
		return;
	}
	g_pHyprRenderer->damageMonitor(PMONITOR);

	if (PMONITOR->activeSpecialWorkspace) {
		recalculateWorkspace(PMONITOR->activeSpecialWorkspace);
	}

	recalculateWorkspace(PMONITOR->activeWorkspace);
	semmety_log(LOG, "EXIT recalculateMonitor");
}

void SemmetyLayout::recalculateWorkspace(const PHLWORKSPACE& workspace) {
	semmety_log(LOG, "ENTER recalculateWorkspace (workspace id: {})", workspace->m_iID);
	if (workspace == nullptr) {
		semmety_log(LOG, "EXIT recalculateWorkspace -- workspace is null");
		return;
	}

	const auto monitor = workspace->m_pMonitor;

	if (g_SemmetyLayout == nullptr) {
		semmety_critical_error("semmety layout is bad");
	}

	auto ww = g_SemmetyLayout->getOrCreateWorkspaceWrapper(workspace);

	ww.root->geometry = {
	    monitor->vecPosition + monitor->vecReservedTopLeft,
	    monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight
	};

	// TODO: figure out why this breaks, maybe re-entry?
	// DO NOT apply here, it breaks mouse focus, result is that first render has incorrectly sized
	// frames ww.apply(); g_pAnimationManager->scheduleTick();
	semmety_log(LOG, "EXIT recalculateWorkspace");
}

bool SemmetyLayout::isWindowTiled(PHLWINDOW pWindow) {
	semmety_log(LOG, "ENTER isWindowTiled");
	for (const auto& ws: workspaceWrappers) {
		if (std::find(ws.windows.begin(), ws.windows.end(), pWindow) != ws.windows.end()) {
			semmety_log(ERR, "isWindowTiled true {}", pWindow->fetchTitle());
			semmety_log(LOG, "EXIT isWindowTiled");
			return true;
		}
	}

	semmety_log(ERR, "isWindowTiled false {}", pWindow->fetchTitle());
	semmety_log(LOG, "EXIT isWindowTiled");
	return false;

	// return getNodeFromWindow(pWindow) != nullptr;
}

PHLWINDOW SemmetyLayout::getNextWindowCandidate(PHLWINDOW window) {
	semmety_log(ERR, "ENTER getNextWindowCandidate");
	// This is only called from the hyprland code that closes windows. If the window is tiled then the
	// logic for closing a tiled window would have already been handled by onWindowRemovedTiling.
	// TODO: return nothing when we have dedicated handling for floating windows
	if (!window->m_bIsFloating) {
		semmety_log(ERR, "EXIT getNextWindowCandidate");
		return {};
	}

	auto ws = workspace_for_action();

	if (!ws) {
		semmety_log(ERR, "EXIT getNextWindowCandidate");
		return {};
	}

	const auto index = ws->getLastFocusedWindowIndex();
	if (index >= ws->windows.size()) {
		semmety_log(ERR, "EXIT getNextWindowCandidate");
		return {};
	}

	if (ws->windows[index]) {
		semmety_log(ERR, "EXIT getNextWindowCandidate");
		return ws->windows[index].lock();
	}

	semmety_log(ERR, "EXIT getNextWindowCandidate");
	return {};
}

void SemmetyLayout::onBeginDragWindow() { semmety_log(LOG, "STUB onBeginDragWindow"); }

void SemmetyLayout::resizeActiveWindow(
    const Vector2D& pixResize,
    eRectCorner corner,
    PHLWINDOW pWindow
) {
	semmety_log(LOG, "STUB resizeActiveWindow");
}

void SemmetyLayout::fullscreenRequestForWindow(
    PHLWINDOW pWindow,
    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
    const eFullscreenMode EFFECTIVE_MODE
) {
	semmety_log(LOG, "STUB fullscreenRequestForWindow");
}

void SemmetyLayout::recalculateWindow(PHLWINDOW pWindow) {
	semmety_log(LOG, "STUB recalculateWindow");
}

SWindowRenderLayoutHints SemmetyLayout::requestRenderHints(PHLWINDOW pWindow) { return {}; }

void SemmetyLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
	semmety_log(LOG, "STUB moveWindowTo");
}

void SemmetyLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
	semmety_log(LOG, "STUB switchWindows");
}

void SemmetyLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
	semmety_log(LOG, "STUB alterSplitRatio");
}

std::any SemmetyLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
	semmety_log(LOG, "STUB layoutMessage");
	return 0;
}

void SemmetyLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
	semmety_log(LOG, "STUB replaceWindowDataWith");
}

Vector2D SemmetyLayout::predictSizeForNewWindowTiled() {
	semmety_log(LOG, "STUB predictSizeForNewWindowTiled");
	return {};
}

std::string SemmetyLayout::getLayoutName() { return "dwindle"; }

bool SemmetyLayout::isWindowReachable(PHLWINDOW win) { return !!win; }
