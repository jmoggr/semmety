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
	workspace_wrapper.apply();
	g_pAnimationManager->scheduleTick();
}

void SemmetyLayout::onWindowRemovedTiling(PHLWINDOW window) {
	if (window->m_pWorkspace == nullptr) {
		return;
	}
	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);
	workspace_wrapper.removeWindow(window);
	workspace_wrapper.apply();
	g_pAnimationManager->scheduleTick();
}

void SemmetyLayout::onWindowFocusChange(PHLWINDOW window) {
	if (window == nullptr) {
		return;
	}

	if (window->m_pWorkspace == nullptr) {
		return;
	}

	const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(&*window));
	auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_pWorkspace);

	const auto frame = workspace_wrapper.getFrameForWindow(window);
	if (frame == nullptr) {
		return;
	}

	workspace_wrapper.setFocusedFrame(frame);
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

	// semmety_log(ERR, "split after \n{}", focused_frame->print(0, workspace_wrapper));
	ww.setFocusedFrame(ww.root);
	ww.apply();

	// this->workspaceWrappers.emplace_back(workspace, *this);
	this->workspaceWrappers.emplace_back(ww);
	return this->workspaceWrappers.back();
}

void SemmetyLayout::recalculateMonitor(const MONITORID& monid) {
	const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

	if (!PMONITOR || !PMONITOR->activeWorkspace) {
		return;
	}
	g_pHyprRenderer->damageMonitor(PMONITOR);

	// if (PMONITOR->activeSpecialWorkspace) {
	// 	recalculateWorkspace(PMONITOR->activeSpecialWorkspace);
	// }

	recalculateWorkspace(PMONITOR->activeWorkspace);
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
	return true;
	// return getNodeFromWindow(pWindow) != nullptr;
}

void SemmetyLayout::onBeginDragWindow() {}

void SemmetyLayout::resizeActiveWindow(
    const Vector2D& pixResize,
    eRectCorner corner,
    PHLWINDOW pWindow
) {}

void SemmetyLayout::fullscreenRequestForWindow(
    PHLWINDOW pWindow,
    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
    const eFullscreenMode EFFECTIVE_MODE
) {}

void SemmetyLayout::recalculateWindow(PHLWINDOW pWindow) {}

SWindowRenderLayoutHints SemmetyLayout::requestRenderHints(PHLWINDOW pWindow) { return {}; }

void SemmetyLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {}

void SemmetyLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {}

void SemmetyLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {}

std::any SemmetyLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {}

void SemmetyLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {}

Vector2D SemmetyLayout::predictSizeForNewWindowTiled() { return {}; }

std::string SemmetyLayout::getLayoutName() { return "dwindle"; }

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

void SemmetyLayout::activeWindowHook(void*, SCallbackInfo&, std::any data) {
	const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
	if (PWINDOW == nullptr) {
		return;
	}

	if (PWINDOW->m_pWorkspace == nullptr) {
		return;
	}

	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}
	auto ww = layout->getOrCreateWorkspaceWrapper(PWINDOW->m_pWorkspace);

	const auto frame = ww.getFrameForWindow(PWINDOW);
	if (frame) {
		ww.setFocusedFrame(frame);
	} else {
		ww.putWindowInFocusedFrame(PWINDOW);
	}

	ww.apply();
	g_pAnimationManager->scheduleTick();
}
