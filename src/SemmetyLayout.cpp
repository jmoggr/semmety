
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

	// auto* existing = this->getNodeFromWindow(window.get());
	// if (existing != nullptr) {
	// 	semmety_log(
	// 	    ERR,
	// 	    "onWindowCreatedTiling called with a window ({:x}) that is already tiled (node: {:x})",
	// 	    (uintptr_t) window.get(),
	// 	    (uintptr_t) existing
	// 	);
	// 	return;
	// }

	

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

    // for (auto& workspaceWrapper : this->workspaceWrappers) {
    //     if (workspaceWrapper.workspace.get() == workspace) {
    //         return &workspaceWrapper;
    //     }
    // }

    this->workspaceWrappers.emplace_back(workspace);

    return &this->workspaceWrappers.back();
}
