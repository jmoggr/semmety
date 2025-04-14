#include <type_traits>

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
	entryWrapper("onWindowCreatedTiling", [&]() -> std::optional<std::string> {
		if (window->m_pWorkspace == nullptr) {
			return "called with a window that has an invalid workspace";
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
			return "window is floating";
		}

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
		workspace_wrapper.addWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::onWindowRemovedTiling(PHLWINDOW window) {
	entryWrapper("onWindowRemovedTiling", [&]() -> std::optional<std::string> {
		if (window->m_pWorkspace == nullptr) {
			return "workspace is null";
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

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::onWindowFocusChange(PHLWINDOW window) {
	entryWrapper("onWindowFocusChange", [&]() -> std::optional<std::string> {
		auto title = window == nullptr ? "none" : window->fetchTitle();
		semmety_log(ERR, "focus changed for window {}", title);

		if (window == nullptr) {
			return "window is null";
		}

		if (window->m_pWorkspace == nullptr) {
			return "window workspace is null";
		}

		if (window->m_bIsFloating) {
			return "window is floating";
		}

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
		workspace_wrapper.activateWindow(window);

		shouldUpdateBar();

		return std::nullopt;
	});
}

void SemmetyLayout::recalculateMonitor(const MONITORID& monid) {
	entryWrapper("recalculateMonitor", [&]() -> std::optional<std::string> {
		const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

		if (!PMONITOR || !PMONITOR->activeWorkspace) {
			return "null monitor or null workspace";
		}
		g_pHyprRenderer->damageMonitor(PMONITOR);

		if (PMONITOR->activeSpecialWorkspace) {
			recalculateWorkspace(PMONITOR->activeSpecialWorkspace);
		}

		recalculateWorkspace(PMONITOR->activeWorkspace);

		return std::nullopt;
	});
}

void SemmetyLayout::recalculateWorkspace(const PHLWORKSPACE& workspace) {
	entryWrapper("recalculateWorkspace", [&]() -> std::optional<std::string> {
		if (workspace == nullptr) {
			return "workspace is null";
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
		// DO NOT apply here, it breaks mouse focus, result is that first render has incorrectly
		// sized frames ww.apply(); g_pAnimationManager->scheduleTick();
		return std::nullopt;
	});
}

bool SemmetyLayout::isWindowTiled(PHLWINDOW pWindow) {
	return entryWrapper("isWindowTiled", [&]() {
		for (const auto& ws: workspaceWrappers) {
			if (std::find(ws.windows.begin(), ws.windows.end(), pWindow) != ws.windows.end()) {
				semmety_log(ERR, "isWindowTiled true {}", pWindow->fetchTitle());
				return true;
			}
		}

		semmety_log(ERR, "isWindowTiled false {}", pWindow->fetchTitle());
		return false;
	});
}

PHLWINDOW SemmetyLayout::getNextWindowCandidate(PHLWINDOW window) {
	return entryWrapper("getNextWindowCandidate", [&]() -> PHLWINDOW {
		// This is only called from the hyprland code that closes windows. If the window is
		// tiled then the logic for closing a tiled window would have already been handled by
		// onWindowRemovedTiling.
		// TODO: return nothing when we have dedicated handling for floating windows
		if (!window->m_bIsFloating) {
			return {};
		}

		auto ws = workspace_for_action();

		if (!ws) {
			return {};
		}

		const auto index = ws->getLastFocusedWindowIndex();
		if (index >= ws->windows.size()) {
			return {};
		}

		if (ws->windows[index]) {
			return ws->windows[index].lock();
		}

		return {};
	});
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
	return entryWrapper("predictSizeForNewWindowTiled", [&]() -> Vector2D {
		auto ws = workspace_for_action();

		if (!ws) {
			return {};
		}

		return ws->getFocusedFrame()->geometry.size();
	});
}

std::string SemmetyLayout::getLayoutName() { return "semmety"; }

bool SemmetyLayout::isWindowReachable(PHLWINDOW win) { return !!win; }
