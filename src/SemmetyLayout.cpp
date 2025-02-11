
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "SemmetyLayout.hpp"
#include "globals.hpp"

PHLWORKSPACE workspace_for_action(bool allow_fullscreen) {
	if (g_pLayoutManager->getCurrentLayout() != g_SemmetyLayout.get()) return nullptr;

	auto workspace = g_pCompositor->m_pLastMonitor->activeSpecialWorkspace;
	if (!valid(workspace)) workspace = g_pCompositor->m_pLastMonitor->activeWorkspace;

	if (!valid(workspace)) return nullptr;
	if (!allow_fullscreen && workspace->m_bHasFullscreenWindow) return nullptr;

	return workspace;
}



void SemmetyLayout::onEnable() {
    	for (auto& window: g_pCompositor->m_vWindows) {
          if (window->isHidden() || !window->m_bIsMapped || window->m_bFadingOut || window->m_bIsFloating)
			      continue;

		      this->onWindowCreatedTiling(window);
	    }
}

void SemmetyLayout::onDisable() {
    // Logic for disabling the layout
}

void SemmetyLayout::onWindowCreatedTiling(PHLWINDOW window, eDirection) {
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
     

   //  	auto& monitor = workspace_wrapper.workspace->m_pMonitor;

			// auto width =
			//     monitor->vecSize.x - monitor->vecReservedBottomRight.x - monitor->vecReservedTopLeft.x;
			// auto height =
			//     monitor->vecSize.y - monitor->vecReservedBottomRight.y - monitor->vecReservedTopLeft.y;

			// this->nodes.push_back({
			//     .data = height > width ? Hy3GroupLayout::SplitV : Hy3GroupLayout::SplitH,
			//     .position = monitor->vecPosition + monitor->vecReservedTopLeft,
			//     .size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight,
			//     .workspace = node.workspace,
			//     .layout = this,
			// });

    
	// node.markFocused();
	// opening_into->recalcSizePosRecursive();

	// this->nodes.push_back({
	//     .parent = nullptr,
	//     .data = window,
	// });

	// this->insertNode(this->nodes.back());
}

SemmetyWorkspaceWrapper& SemmetyLayout::getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace) {
    auto* wrkspc = workspace.get();

    for (auto& wrapper : this->workspaceWrappers) {
        if (wrapper.workspace.get() == wrkspc) {
            return wrapper;
        }
    }

    this->workspaceWrappers.emplace_back(workspace, *this);
    return this->workspaceWrappers.back();
}



void SemmetyLayout::applyFrameDataToWindow(SemmetyFrame* frame, bool no_animation) {
	if (frame->data.is_parent()) return;
	auto window = frame->data.as_window().window;

	auto root_frame = this->getWorkspaceRootFrame(window->m_pWorkspace.get());

	auto& monitor = frame->workspace->m_pMonitor;

	if (monitor == nullptr) {
		semmety_log(
		    ERR,
		    "frame {:x}'s workspace has no associated monitor, cannot apply frame data",
		    (uintptr_t) frame
		);
		errorNotif();
		return;
	}

	if (!valid(window) || !window->m_bIsMapped) {
		semmety_log(
		    ERR,
		    "semmety {:x} is an unmapped window ({:x}), cannot apply frame data, removing from tiled "
		    "layout",
		    (uintptr_t) node,
		    (uintptr_t) window.get()
		);
		errorNotif();
		this->onWindowRemovedTiling(window);
		return;
	}

	window->unsetWindowData(PRIORITY_LAYOUT);

	auto FrameBox = CBox(Frame->position, Frame->size);
	FrameBox.round();

	window->m_vSize = FrameBox.size();
	window->m_vPosition = FrameBox.pos();

	// auto only_Frame = root_Frame != nullptr && root_Frame->data.as_group().children.size() == 1
	//               && root_Frame->data.as_group().children.front()->data.is_window();


	// if (!window->m_pWorkspace->m_bIsSpecialWorkspace
	//     && ((*no_gaps_when_only != 0 && (only_Frame || window->isFullscreen()))
	//         || window->isEffectiveInternalFSMode(FSMODE_FULLSCREEN)))
	// {
	// 	window->m_sWindowData.decorate = CWindowOverridableVar(
	// 	    true,
	// 	    PRIORITY_LAYOUT
	// 	); // a little curious but copying what dwindle does
	// 	window->m_sWindowData.noBorder =
	// 	    CWindowOverridableVar(*no_gaps_when_only != 2, PRIORITY_LAYOUT);
	// 	window->m_sWindowData.noRounding = CWindowOverridableVar(true, PRIORITY_LAYOUT);
	// 	window->m_sWindowData.noShadow = CWindowOverridableVar(true, PRIORITY_LAYOUT);

	// 	window->updateWindowDecos();

	// 	const auto reserved = window->getFullWindowReservedArea();

	// 	*window->m_vRealPosition = window->m_vPosition + reserved.topLeft;
	// 	*window->m_vRealSize = window->m_vSize - (reserved.topLeft + reserved.bottomRight);

	// 	window->sendWindowSize(true);
	// } else {
		auto reserved = window->getFullWindowReservedArea();
		auto wb = Frame->getStandardWindowArea({-reserved.topLeft, -reserved.bottomRight});

		*window->m_vRealPosition = wb.pos();
		*window->m_vRealSize = wb.size();

		window->sendWindowSize(true);

		if (no_animation) {
			g_pHyprRenderer->damageWindow(window);

			window->m_vRealPosition->warp();
			window->m_vRealSize->warp();

			g_pHyprRenderer->damageWindow(window);
		}

		window->updateWindowDecos();
	// }
}


bool SemmetyLayout::isWindowTiled(PHLWINDOW) {
    // Logic to determine if a window is tiled
    return true;
}

void SemmetyLayout::onWindowRemovedTiling(PHLWINDOW) {
    // Logic for handling window removal in tiling mode
}

void SemmetyLayout::recalculateMonitor(const MONITORID&) {
    // Logic for recalculating monitor layout
}

void SemmetyLayout::recalculateWindow(PHLWINDOW) {
    // Logic for recalculating window layout
}

void SemmetyLayout::resizeActiveWindow(const Vector2D&, eRectCorner, PHLWINDOW) {
    // Logic for resizing active window
}

void SemmetyLayout::fullscreenRequestForWindow(PHLWINDOW, const eFullscreenMode, const eFullscreenMode) {
    // Logic for handling fullscreen requests
}

std::any SemmetyLayout::layoutMessage(SLayoutMessageHeader, std::string) {
    // Logic for handling layout messages
    return std::any();
}

SWindowRenderLayoutHints SemmetyLayout::requestRenderHints(PHLWINDOW) {
    // Logic for requesting render hints
    return SWindowRenderLayoutHints();
}

void SemmetyLayout::switchWindows(PHLWINDOW, PHLWINDOW) {
    // Logic for switching windows
}

void SemmetyLayout::moveWindowTo(PHLWINDOW, const std::string&, bool) {
    // Logic for moving window
}

void SemmetyLayout::alterSplitRatio(PHLWINDOW, float, bool) {
    // Logic for altering split ratio
}

std::string SemmetyLayout::getLayoutName() {
    // Logic for getting layout name
    return "SemmetyLayout";
}

void SemmetyLayout::replaceWindowDataWith(PHLWINDOW, PHLWINDOW) {
    // Logic for replacing window data
}

Vector2D SemmetyLayout::predictSizeForNewWindowTiled() {
    // Logic for predicting size for new window
    return Vector2D();
}
