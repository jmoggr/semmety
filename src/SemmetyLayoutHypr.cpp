#include <optional>
#include <type_traits>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/EventManager.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>

#include "SemmetyFrame.hpp"
#include "SemmetyFrameUtils.hpp"
#include "SemmetyLayout.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "src/desktop/DesktopTypes.hpp"
#include "src/layout/IHyprLayout.hpp"
#include "utils.hpp"

SP<HOOK_CALLBACK_FN> renderHookPtr;
SP<HOOK_CALLBACK_FN> tickHookPtr;
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

	workspaceHookPtr =
	    HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", &SemmetyLayout::workspaceHook);
	urgentHookPtr =
	    HyprlandAPI::registerCallbackDynamic(PHANDLE, "urgent", &SemmetyLayout::urgentHook);
}

void SemmetyLayout::onDisable() {
	renderHookPtr.reset();
	tickHookPtr.reset();
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

void SemmetyLayout::onWindowCreatedFloating(PHLWINDOW window) {
	entryWrapper("onWindowCreatedFloating", [&]() -> std::optional<std::string> {
		IHyprLayout::onWindowCreatedFloating(window);

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

		if (!window->m_bIsFloating) {
			return "window is tiled";
		}

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
		workspace_wrapper.addWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::changeWindowFloatingMode(PHLWINDOW window) {
	entryWrapper("changeWindowFloatingMode", [&]() -> std::optional<std::string> {
		if (window->isFullscreen()) {
			Debug::log(LOG, "changeWindowFloatingMode: fullscreen");
			g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
		}

		window->m_bPinned = false;

		const auto TILED = isWindowTiled(window);

		// event
		g_pEventManager->postEvent(SHyprIPCEvent {
		    "changefloatingmode",
		    std::format("{:x},{}", (uintptr_t) window.get(), (int) TILED)
		});
		EMIT_HOOK_EVENT("changeFloatingMode", window);
		const auto ws = workspace_for_window(window);
		if (!ws) {
			semmety_log(ERR, "can't get workspace");
		}

		if (!TILED) {
			const auto PNEWMON = g_pCompositor->getMonitorFromVector(
			    window->m_vRealPosition->value() + window->m_vRealSize->value() / 2.f
			);
			const auto workspace = PNEWMON->activeSpecialWorkspace ? PNEWMON->activeSpecialWorkspace
			                                                       : PNEWMON->activeWorkspace;
			window->m_pMonitor = PNEWMON;
			window->moveToWorkspace(workspace);
			window->updateGroupOutputs();

			const auto PWORKSPACE = PNEWMON->activeSpecialWorkspace ? PNEWMON->activeSpecialWorkspace
			                                                        : PNEWMON->activeWorkspace;

			if (PWORKSPACE->m_bHasFullscreenWindow)
				g_pCompositor->setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);

			// save real pos cuz the func applies the default 5,5 mid
			const auto PSAVEDPOS = window->m_vRealPosition->goal();
			const auto PSAVEDSIZE = window->m_vRealSize->goal();

			// // if the window is pseudo, update its size
			if (!window->m_bDraggingTiled) window->m_vPseudoSize = window->m_vRealSize->goal();

			window->m_vLastFloatingSize = PSAVEDSIZE;

			// move to narnia because we don't wanna find our own node. onWindowCreatedTiling should apply
			// the coords back.
			// window->m_vPosition = Vector2D(-999999, -999999);

			ws->setWindowTiled(window, true);

			// window->m_vRealPosition->setValue(PSAVEDPOS);
			// window->m_vRealSize->setValue(PSAVEDSIZE);

			// fix pseudo leaving artifacts
			g_pHyprRenderer->damageMonitor(window->m_pMonitor.lock());
		} else {
			g_pHyprRenderer->damageWindow(window, true);

			ws->setWindowTiled(window, false);

			g_pCompositor->changeWindowZOrder(window, true);

			CBox wb = {
			    window->m_vRealPosition->goal()
			        + (window->m_vRealSize->goal() - window->m_vLastFloatingSize) / 2.f,
			    window->m_vLastFloatingSize
			};
			wb.round();

			if (!(window->m_bIsFloating && window->m_bIsPseudotiled)
			    && DELTALESSTHAN(window->m_vRealSize->value().x, window->m_vLastFloatingSize.x, 10)
			    && DELTALESSTHAN(window->m_vRealSize->value().y, window->m_vLastFloatingSize.y, 10))
			{
				wb = {wb.pos() + Vector2D {10, 10}, wb.size() - Vector2D {20, 20}};
			}

			*window->m_vRealPosition = wb.pos();
			*window->m_vRealSize = wb.size();

			window->m_vSize = wb.size();
			window->m_vPosition = wb.pos();

			g_pHyprRenderer->damageMonitor(window->m_pMonitor.lock());

			window->unsetWindowData(PRIORITY_LAYOUT);
			window->updateWindowData();

			g_pCompositor->updateWindowAnimatedDecorationValues(window);
			g_pHyprRenderer->damageWindow(window);
		}

		window->updateToplevel();

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

		// from dwindle for unknown reasons
		window->unsetWindowData(PRIORITY_LAYOUT);
		window->updateWindowData();
		if (window->isFullscreen()) {
			g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
		}

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
		workspace_wrapper.removeWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::onWindowRemovedFloating(PHLWINDOW window) {
	entryWrapper("onWindowRemovedFloating", [&]() -> std::optional<std::string> {
		if (!window) {
			return "window is null";
		}

		if (window->m_pWorkspace == nullptr) {
			// TODO: workspace being null is likely a bug in hyprland
			for (auto workspace: workspaceWrappers) {
				workspace.removeWindow(window);
			}

			return format("workspace is null for window {}", windowToString(window));
		}

		semmety_log(
		    LOG,
		    "onWindowRemovedFloating window {:x} (floating: {}, monitor: {}, workspace: {}, title: {})",
		    (uintptr_t) window.get(),
		    window->m_bIsFloating,
		    window->monitorID(),
		    window->m_pWorkspace->m_iID,
		    window->fetchTitle()
		);

		// from dwindle for unknown reasons
		window->unsetWindowData(PRIORITY_LAYOUT);
		window->updateWindowData();
		if (window->isFullscreen()) {
			g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
		}

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
		workspace_wrapper.removeWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::onWindowFocusChange(PHLWINDOW window) {
	if (entryCount > 0) {
		return;
	}

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

		return std::nullopt;
	});
}

bool SemmetyLayout::isWindowTiled(PHLWINDOW pWindow) {
	return entryWrapper("isWindowTiled", [&]() {
		for (const auto& ws: workspaceWrappers) {
			if (std::find(ws.windows.begin(), ws.windows.end(), pWindow) != ws.windows.end()) {
				return !!ws.getFrameForWindow(pWindow);
			}
		}
		return false;
	});
}

PHLWINDOW SemmetyLayout::getNextWindowCandidate(PHLWINDOW window) {
	return entryWrapper("getNextWindowCandidate", [&]() -> PHLWINDOW {
		if (window->m_pWorkspace->m_bHasFullscreenWindow) {
			return window->m_pWorkspace->getFullscreenWindow();
		}

		// This is only called from the hyprland code that closes windows. If the window is
		// tiled then the logic for closing a tiled window would have already been handled by
		// onWindowRemovedTiling.
		// TODO: return nothing when we have dedicated handling for floating windows
		if (!window->m_bIsFloating) {
			return {};
		}

		auto ws = workspace_for_window(window);
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

Direction oppositeDirection(Direction dir) {
	switch (dir) {
	case Direction::Up: return Direction::Down;
	case Direction::Down: return Direction::Up;
	case Direction::Left: return Direction::Right;
	case Direction::Right: return Direction::Left;
	}

	std::unreachable();
}

void SemmetyLayout::resizeActiveWindow(
    const Vector2D& delta,
    eRectCorner corner,
    PHLWINDOW pWindow
) {
	auto window = pWindow ? pWindow : g_pCompositor->m_pLastWindow.lock();
	if (!valid(window)) {
		return;
	}

	if (window->m_bIsFloating) {
		const auto required_size = Vector2D(
		    std::max((window->m_vRealSize->goal() + delta).x, 20.0),
		    std::max((window->m_vRealSize->goal() + delta).y, 20.0)
		);
		*window->m_vRealSize = required_size;
		return;
	}

	auto workspace = workspace_for_window(window);
	if (!workspace) {
		return;
	}

	auto frame = workspace->getFrameForWindow(window);
	if (!frame) {
		return;
	}

	auto resizeDelta = delta;
	SP<SemmetySplitFrame> horizontalParent, verticalParent;

	switch (corner) {
	case CORNER_TOPLEFT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up);

		// Not sure why the signs on top and left corners are flipped. We could work with them as is,
		// but flipping them makes the rest of the code a bit simpler.
		resizeDelta.x *= -1;
		resizeDelta.y *= -1;
		break;
	case CORNER_TOPRIGHT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up);
		resizeDelta.y *= -1;
		break;
	case CORNER_BOTTOMRIGHT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Down);
		break;
	case CORNER_BOTTOMLEFT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Down);
		resizeDelta.x *= -1;
		break;
	case CORNER_NONE:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up, Direction::Down);
		break;
	}

	if (!horizontalParent && !verticalParent) {
		return;
	}

	if (horizontalParent) {
		if (horizontalParent->children.second->isSameOrDescendant(frame)) {
			resizeDelta.x *= -1;
		}

		horizontalParent->resize(resizeDelta.x);
	}

	if (verticalParent) {
		if (verticalParent->children.second->isSameOrDescendant(frame)) {
			resizeDelta.y *= -1;
		}

		verticalParent->resize(resizeDelta.y);
	}

	SP<SemmetyFrame> commonParent;
	if (!horizontalParent) {
		commonParent = verticalParent;
	} else if (!verticalParent) {
		commonParent = horizontalParent;
	} else {
		commonParent = getCommonParent(*workspace, horizontalParent, verticalParent);
	}

	commonParent->applyRecursive(*workspace, std::nullopt, true);
}

void SemmetyLayout::fullscreenRequestForWindow(
    PHLWINDOW window,
    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
    const eFullscreenMode EFFECTIVE_MODE
) {
	entryWrapper("fullscreenRequestForWindow", [&]() -> std::optional<std::string> {
		auto workspace = workspace_for_window(window);
		if (!workspace) {
			return "Failed to get workspace for window";
		}

		semmety_log(ERR, "current: {}, effective: {}", CURRENT_EFFECTIVE_MODE, EFFECTIVE_MODE);

		if (EFFECTIVE_MODE == FSMODE_NONE) {
			auto frame = workspace->getFrameForWindow(window);
			if (frame) {
				frame->applyRecursive(*workspace, std::nullopt, false);
			} else {
				// TODO: is floating?
				*window->m_vRealPosition = window->m_vLastFloatingPosition;
				*window->m_vRealSize = window->m_vLastFloatingSize;

				window->unsetWindowData(PRIORITY_LAYOUT);
			}
		} else {
			// if (target_mode == FSMODE_FULLSCREEN) {

			// save position and size if floating
			if (window->m_bIsFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
				window->m_vLastFloatingSize = window->m_vRealSize->goal();
				window->m_vLastFloatingPosition = window->m_vRealPosition->goal();
				window->m_vPosition = window->m_vRealPosition->goal();
				window->m_vSize = window->m_vRealSize->goal();
			}

			window->updateDynamicRules();
			window->updateWindowDecos();

			const auto& monitor = window->m_pMonitor;

			*window->m_vRealPosition = monitor->vecPosition;
			*window->m_vRealSize = monitor->vecSize;
			g_pCompositor->changeWindowZOrder(window, true);

			// TODO: maximize
		}

		return std::nullopt;
	});
}

void SemmetyLayout::recalculateWindow(PHLWINDOW window) {
	entryWrapper("recalculateWindow", [&]() -> std::optional<std::string> {
		if (!window) {
			return "window is null";
		}

		auto workspace = workspace_for_window(window);
		if (!workspace) {
			return "Failed to find workspace for window";
		}

		auto frame = workspace->getFrameForWindow(window);
		if (!frame) {
			return "Failed to find frame for window";
		}

		frame->applyRecursive(*workspace);

		return std::nullopt;
	});
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
