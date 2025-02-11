
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "SemmetyLayout.hpp"
#include "globals.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "SemmetyWorkspaceWrapper.hpp"

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
    workspace_wrapper.apply();
     

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
