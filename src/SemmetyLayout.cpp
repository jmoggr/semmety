#include "SemmetyLayout.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>

#include "SemmetyFrame.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "src/desktop/DesktopTypes.hpp"
#include "src/layout/IHyprLayout.hpp"
#include "utils.hpp"

void SemmetyLayout::onWindowCreatedTiling(PHLWINDOW window, eDirection direction) {
	if (window->m_pWorkspace == nullptr) {
		semmety_log(ERR, "onWindowCreatedTiling called with a window that has an invalid workspace");
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

	if (window->m_bIsFloating) return;

	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.addWindow(window);

	auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
	if (monitor != nullptr && monitor->activeWorkspace != nullptr) {
		if (monitor->activeWorkspace == window->m_pWorkspace) {
			workspace_wrapper.apply();
			updateBar();
		}
	}

	g_pAnimationManager->scheduleTick();
}

void SemmetyLayout::onWindowRemovedTiling(PHLWINDOW window) {
	semmety_log(TRACE, "BEGIN onWindowRemovedTiling");
	if (window->m_pWorkspace == nullptr) {
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
	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.removeWindow(window);
	workspace_wrapper.apply();
	updateBar();
	g_pAnimationManager->scheduleTick();
	semmety_log(TRACE, "END onWindowRemovedTiling");
}

void SemmetyLayout::onWindowFocusChange(PHLWINDOW window) {
	semmety_log(TRACE, "BEGIN onWindowFocusChange");
	auto title = window == nullptr ? "none" : window->fetchTitle();
	semmety_log(ERR, "focus changed for window {}", title);
	if (window == nullptr) {
		semmety_log(TRACE, "END onWindowFocusChange");
		return;
	}

	if (window->m_pWorkspace == nullptr) {
		semmety_log(TRACE, "END onWindowFocusChange");
		return;
	}

	if (window->m_bIsFloating) {
		return;
	}

	const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(&*window));
	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);

	const auto frame = workspace_wrapper.getFrameForWindow(window);
	if (frame == nullptr) {
		semmety_log(ERR, "putting focused window in focused frame");
		workspace_wrapper.putWindowInFocusedFrame(window);
	} else {
		workspace_wrapper.setFocusedFrame(frame);
	}
	updateBar();
	semmety_log(TRACE, "END onWindowFocusChange");
}

SemmetyWorkspaceWrapper& SemmetyLayout::getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace) {
	if (workspace == nullptr) {
		semmety_critical_error("Tring to get or create a workspace wrapper with an invalid workspace");
	}

	for (auto& wrapper: this->workspaceWrappers) {
		if (wrapper.workspace.get() == &*workspace) {
			return wrapper;
		}
	}

	semmety_log(ERR, "Creating new workspace wrapper for workspace {}", workspace->m_iID);
	auto ww = SemmetyWorkspaceWrapper(workspace, *this);

	const auto childA = makeShared<SemmetyFrame>(SemmetyFrame());
	const auto childB = makeShared<SemmetyFrame>(SemmetyFrame());
	ww.root->makeParent(childA, childB);
	for (auto& child: ww.root->as_parent().children) {
		child->parent = ww.root;
	}

	ww.setFocusedFrame(ww.root);
	ww.apply();

	this->workspaceWrappers.emplace_back(ww);
	return this->workspaceWrappers.back();
}

void SemmetyLayout::recalculateMonitor(const MONITORID& monid) {
	semmety_log(TRACE, "BEGIN recalculateMonitor");
	const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

	if (!PMONITOR || !PMONITOR->activeWorkspace) {
		semmety_log(TRACE, "END recalculateMonitor");
		return;
	}
	g_pHyprRenderer->damageMonitor(PMONITOR);

	// if (PMONITOR->activeSpecialWorkspace) {
	// 	recalculateWorkspace(PMONITOR->activeSpecialWorkspace);
	// }

	recalculateWorkspace(PMONITOR->activeWorkspace);
	semmety_log(TRACE, "END recalculateMonitor");
}

void SemmetyLayout::recalculateWorkspace(const PHLWORKSPACE& workspace) {
	if (workspace == nullptr) {
		return;
	}

	semmety_log(ERR, "recalculate workspace {}", workspace->m_iID);

	const auto monitor = workspace->m_pMonitor;

	if (g_SemmetyLayout == nullptr) {
		semmety_critical_error("semmety layout is bad");
	}

	if (g_SemmetyLayout->workspaceWrappers.empty()) {
		semmety_log(ERR, "no workspace wrappers");
	}

	auto ww = g_SemmetyLayout->getOrCreateWorkspaceWrapper(workspace);

	ww.root->geometry = {
	    monitor->vecPosition + monitor->vecReservedTopLeft,
	    monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight
	};

	// TODO: figure out why this breaks, maybe re-entry?
	// DO NOT apply here, it breaks mouse focus, result is that first render has incorrectly sized
	// frames ww.apply(); g_pAnimationManager->scheduleTick();
}

bool SemmetyLayout::isWindowTiled(PHLWINDOW pWindow) {
	semmety_log(TRACE, "BEGIN isWindowTiled");
	for (const auto& ws: workspaceWrappers) {
		if (std::find(ws.windows.begin(), ws.windows.end(), pWindow) != ws.windows.end()) {
			semmety_log(ERR, "isWindowTiled true {}", pWindow->fetchTitle());
			semmety_log(TRACE, "END isWindowTiled");
			return true;
		}
	}

	semmety_log(ERR, "isWindowTiled false {}", pWindow->fetchTitle());
	semmety_log(TRACE, "END isWindowTiled");
	return false;

	// return getNodeFromWindow(pWindow) != nullptr;
}

PHLWINDOW SemmetyLayout::getNextWindowCandidate(PHLWINDOW window) {
	SemmetyWorkspaceWrapper* ws = nullptr;
	if (window && window->m_pWorkspace) {
		ws = &getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	}

	if (!ws) {
		ws = workspace_for_action();
	}

	if (!ws) {
		return {};
	}

	// if the window we are replacing is floating, first try using an visible window
	if (!window || !window->m_bIsFloating) {
		const auto nextMinimizedWindow = ws->getNextMinimizedWindow();
		if (nextMinimizedWindow) {
			return nextMinimizedWindow.lock();
		}
	}

	const auto index = ws->getLastFocusedWindowIndex();
	if (index >= ws->windows.size()) {
		return {};
	}

	if (ws->windows[index]) {
		return ws->windows[index].lock();
	}

	return {};
}

// void SemmetyLayout::onBeginDragWindow() { semmety_log(TRACE, "STUB onBeginDragWindow"); }

void SemmetyLayout::resizeActiveWindow(
    const Vector2D& pixResize,
    eRectCorner corner,
    PHLWINDOW pWindow
) {
	semmety_log(TRACE, "STUB resizeActiveWindow");
}

void SemmetyLayout::fullscreenRequestForWindow(
    PHLWINDOW pWindow,
    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
    const eFullscreenMode EFFECTIVE_MODE
) {
	semmety_log(TRACE, "STUB fullscreenRequestForWindow");
}

void SemmetyLayout::recalculateWindow(PHLWINDOW pWindow) {
	semmety_log(TRACE, "STUB recalculateWindow");
}

SWindowRenderLayoutHints SemmetyLayout::requestRenderHints(PHLWINDOW pWindow) { return {}; }

void SemmetyLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
	semmety_log(TRACE, "STUB moveWindowTo");
}

void SemmetyLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
	semmety_log(TRACE, "STUB switchWindows");
}

void SemmetyLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
	semmety_log(TRACE, "STUB alterSplitRatio");
}

std::any SemmetyLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
	semmety_log(TRACE, "STUB layoutMessage");
}

void SemmetyLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
	semmety_log(TRACE, "STUB replaceWindowDataWith");
}

Vector2D SemmetyLayout::predictSizeForNewWindowTiled() {
	semmety_log(TRACE, "STUB predictSizeForNewWindowTiled");
	return {};
}

std::string SemmetyLayout::getLayoutName() { return "dwindle"; }

bool SemmetyLayout::isWindowReachable(PHLWINDOW win) { return !!win; }

SP<HOOK_CALLBACK_FN> renderHookPtr;
SP<HOOK_CALLBACK_FN> tickHookPtr;
SP<HOOK_CALLBACK_FN> activeWindowHookPtr;

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
}

void SemmetyLayout::onDisable() {
	renderHookPtr.reset();
	tickHookPtr.reset();
	activeWindowHookPtr.reset();
}

void SemmetyLayout::moveWindowToWorkspace(std::string wsname) {
	auto focused_window = g_pCompositor->m_pLastWindow.lock();
	if (!focused_window) {
		semmety_log(ERR, "no focused window {}", wsname);
		return;
	}

	const auto sourceWorkspace = focused_window->m_pWorkspace;
	if (!sourceWorkspace) {
		semmety_log(ERR, "no source workspace");
		return;
	}

	auto target = getWorkspaceIDNameFromString(wsname);

	if (target.id == WORKSPACE_INVALID) {
		semmety_log(ERR, "moveNodeToWorkspace called with invalid workspace {}", wsname);
		return;
	}

	auto targetWorkspace = g_pCompositor->getWorkspaceByID(target.id);
	if (!targetWorkspace) {
		semmety_log(LOG, "creating target workspace {} for node move", target.id);

		targetWorkspace =
		    g_pCompositor->createNewWorkspace(target.id, sourceWorkspace->monitorID(), target.name);
		if (!targetWorkspace) {
			semmety_log(ERR, "could not find target workspace '{}', '{}'", wsname, target.id);
			return;
		}
	}

	if (sourceWorkspace == targetWorkspace) {
		semmety_log(ERR, "source and target workspaces are the same");
		return;
	}

	auto sourceWrapper = getOrCreateWorkspaceWrapper(sourceWorkspace);
	auto targetWrapper = getOrCreateWorkspaceWrapper(targetWorkspace);

	sourceWrapper.removeWindow(focused_window);
	// onWindowCreatedTiling is called when the new window is put on the new monitor

	g_pHyprRenderer->damageWindow(focused_window);
	g_pCompositor->moveWindowToWorkspaceSafe(focused_window, targetWorkspace);
	focused_window->updateToplevel();
	focused_window->updateDynamicRules();
	focused_window->uncacheWindowDecos();

	sourceWrapper.apply();
	updateBar();
}

void SemmetyLayout::tickHook(void*, SCallbackInfo&, std::any) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}
	if (g_pLayoutManager->getCurrentLayout() != layout) return;

	for (const auto& monitor: g_pCompositor->m_vMonitors) {
		if (monitor->activeWorkspace == nullptr) {
			continue;
		}

		const auto activeWorkspace = monitor->activeWorkspace;
		if (activeWorkspace == nullptr) {
			continue;
		}

		const auto ww = layout->getOrCreateWorkspaceWrapper(monitor->activeWorkspace);
		auto emptyFrames = ww.getEmptyFrames();

		for (const auto& frame: emptyFrames) {
			frame->damageEmptyFrameBox(*monitor);
		}
	}
}

void SemmetyLayout::renderHook(void*, SCallbackInfo&, std::any data) {
	auto render_stage = std::any_cast<eRenderStage>(data);

	static auto PACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
	static auto PINACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");
	static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

	auto* const ACTIVECOL = (CGradientValueData*) (PACTIVECOL.ptr())->getData();
	auto* const INACTIVECOL = (CGradientValueData*) (PINACTIVECOL.ptr())->getData();

	auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
	if (monitor == nullptr) {
		return;
	}

	if (monitor->activeWorkspace == nullptr) {
		return;
	}

	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}
	auto ww = layout->getOrCreateWorkspaceWrapper(monitor->activeWorkspace);
	auto emptyFrames = ww.getEmptyFrames();

	switch (render_stage) {
	case RENDER_PRE_WINDOWS:
		for (const auto& frame: emptyFrames) {
			CBorderPassElement::SBorderData borderData;
			if (ww.focused_frame == frame) {
				borderData.grad1 = *ACTIVECOL;
			} else {
				borderData.grad1 = *INACTIVECOL;
			}
			borderData.box = frame->getEmptyFrameBox(*monitor);

			borderData.borderSize = *PBORDERSIZE;
			borderData.roundingPower = *PROUNDINGPOWER;
			borderData.round = *PROUNDING;
			auto element = CBorderPassElement(borderData);
			auto pass = makeShared<CBorderPassElement>(element);
			g_pHyprRenderer->m_sRenderPass.add(pass);
		}

		break;
	default: break;
	}
}

void SemmetyLayout::activateWindow(PHLWINDOW window) {
	auto layout = g_SemmetyLayout.get();
	auto ww = layout->getOrCreateWorkspaceWrapper(window->m_pWorkspace);

	const auto frame = ww.getFrameForWindow(window);
	if (frame) {
		ww.setFocusedFrame(frame);
	} else {
		ww.putWindowInFocusedFrame(window);
	}

	ww.apply();
	updateBar();
	g_pAnimationManager->scheduleTick();
}

void SemmetyLayout::activeWindowHook(void*, SCallbackInfo&, std::any data) {
	const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
	if (PWINDOW == nullptr) {
		return;
	}

	if (PWINDOW->m_bIsFloating) {
		return;
	}

	if (PWINDOW->m_pWorkspace == nullptr) {
		return;
	}

	g_SemmetyLayout->activateWindow(PWINDOW);
}

json SemmetyLayout::getWorkspacesJson() {
	const auto ws = workspace_for_action();

	json jsonWorkspaces = json::array();
	// TODO: get max workspace ID iterate to that if it's large than 8
	for (int workspaceIndex = 0; workspaceIndex < 8; workspaceIndex++) {
		auto it = std::find_if(
		    workspaceWrappers.begin(),
		    workspaceWrappers.end(),
		    [workspaceIndex](const auto& workspaceWrapper) {
			    return workspaceWrapper.workspace != nullptr
			        && workspaceWrapper.workspace->m_iID == workspaceIndex + 1;
		    }
		);

		if (it == workspaceWrappers.end()) {
			jsonWorkspaces.push_back(
			    {{"id", workspaceIndex + 1}, {"numWindows", 0}, {"name", ""}, {"focused", false}}
			);

			continue;
		}

		jsonWorkspaces.push_back(
		    {{"id", workspaceIndex + 1},
		     {"numWindows", it->windows.size()},
		     {"name", it->workspace->m_szName},
		     {"focused", &(*it) == &(*ws)}}
		);
	}

	return jsonWorkspaces;
}
