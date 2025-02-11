
#include <hyprland/src/Compositor.hpp>
#include "SemmetyLayout.hpp"

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

    // Logic for creating a window in tiling mode

	

	// this->nodes.push_back({
	//     .parent = nullptr,
	//     .data = window,
	// });

	// this->insertNode(this->nodes.back());

	// auto& workspaceWrapper = this->getOrCreateWorkspaceWrapper(window->m_pWorkspace);

	
}

SemmetyWorkspaceWrapper& SemmetyLayout::getOrCreateWorkspaceWrapper(PHLWORKSPACEREF workspace) {
    auto* wrkspc = workspace.get();
    if (wrkspc == nullptr) {
        throw std::runtime_error("getOrCreateWorkspaceWrapper called with null workspace");
    }

    // Logic for getting or creating a workspace wrapper

    this->workspaceWrappers.emplace_back(workspace);

    return &this->workspaceWrappers.back();
}
bool SemmetyLayout::isWindowTiled(PHLWINDOW) {
    // Logic to determine if a window is tiled
    return false;
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
