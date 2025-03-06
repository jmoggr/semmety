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

	auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
	if (monitor != nullptr && monitor->activeWorkspace != nullptr) {
		if (monitor->activeWorkspace == window->m_pWorkspace) {
			workspace_wrapper.apply();
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
	workspace_wrapper.apply();
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
	semmety_log(LOG, "EXIT onWindowFocusChange");
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
	semmety_log(LOG, "ENTER recalculateMonitor");
	const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

	if (!PMONITOR || !PMONITOR->activeWorkspace) {
		semmety_log(LOG, "EXIT recalculateMonitor -- null monitor or null workspace");
		return;
	}
	g_pHyprRenderer->damageMonitor(PMONITOR);

	// if (PMONITOR->activeSpecialWorkspace) {
	// 	recalculateWorkspace(PMONITOR->activeSpecialWorkspace);
	// }

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
	// This is only called from the hyprland code that closes windows. If the window is tiled then the
	// logic for closing a tiled window would have already been handled by onWindowRemovedTiling.
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
}

// void SemmetyLayout::onBeginDragWindow() { semmety_log(LOG, "STUB onBeginDragWindow"); }

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

	if (layout->updateBarOnNextTick) {
		updateBar();
		layout->updateBarOnNextTick = false;
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
void SemmetyLayout::workspaceHook(void*, SCallbackInfo&, std::any data) { updateBar(); }

void SemmetyLayout::urgentHook(void*, SCallbackInfo&, std::any data) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}

	// m_bIsUrgent is not set until after the hook event is handled, so we hack it this way
	layout->updateBarOnNextTick = true;
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
			    {{"id", workspaceIndex + 1},
			     {"numWindows", 0},
			     {"name", ""},
			     {"focused", false},
			     {"urgent", false}}
			);

			continue;
		}

		jsonWorkspaces.push_back(
		    {{"id", workspaceIndex + 1},
		     {"numWindows", it->windows.size()},
		     {"name", it->workspace->m_szName},
		     {"urgent", it->workspace->hasUrgentWindow()},
		     {"focused", &(*it) == &(*ws)}}
		);
	}

	return jsonWorkspaces;
}
