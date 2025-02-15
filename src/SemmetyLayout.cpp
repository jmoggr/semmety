
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>


#include <hyprland/src/render/pass/BorderPassElement.hpp>

#include "SemmetyLayout.hpp"
#include "globals.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include "SemmetyWorkspaceWrapper.hpp"


SP<HOOK_CALLBACK_FN> renderHookPtr;

SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen) {
  auto layout = g_SemmetyLayout.get();
	if (g_pLayoutManager->getCurrentLayout() != layout) return nullptr;

	auto workspace = g_pCompositor->m_pLastMonitor->activeSpecialWorkspace;
	if (!valid(workspace)) workspace = g_pCompositor->m_pLastMonitor->activeWorkspace;

	if (!valid(workspace)) return nullptr;
	if (!allow_fullscreen && workspace->m_bHasFullscreenWindow) return nullptr;

	semmety_log(ERR, "getting workspace for action {}", workspace->m_iID);

	return &layout->getOrCreateWorkspaceWrapper(workspace);
}



void SemmetyLayout::onEnable() {
    	for (auto& window: g_pCompositor->m_vWindows) {
          if (window->isHidden() || !window->m_bIsMapped || window->m_bFadingOut || window->m_bIsFloating)
			      continue;

		      this->onWindowCreatedTiling(window);
	    }

	    
	renderHookPtr = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", &SemmetyLayout::renderHook);
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
        	// semmety_log(LOG, "using existing workspace");
            return wrapper;
        }
    }

	semmety_log(LOG, "creating new workspace");
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

void SemmetyLayout::renderHook(void*, SCallbackInfo&, std::any data) {
	auto render_stage = std::any_cast<eRenderStage>(data);

    static auto PACTIVECOL              = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
    static auto PINACTIVECOL            = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
    static auto PBORDERSIZE       = CConfigValue<Hyprlang::INT>("general:border_size");

    static auto PROUNDING      = CConfigValue<Hyprlang::INT>("decoration:rounding");
    static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

    auto* const ACTIVECOL              = (CGradientValueData*)(PACTIVECOL.ptr())->getData();
    auto* const INACTIVECOL            = (CGradientValueData*)(PINACTIVECOL.ptr())->getData();    


  auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
  if (monitor == nullptr) {
      return;
  }
 
    auto layout = g_SemmetyLayout.get();
    auto w = layout->getOrCreateWorkspaceWrapper(monitor->activeWorkspace);
    auto emptyFrames = w.getEmptyFrames();
  
  // :

	switch (render_stage) {
	case RENDER_PRE_WINDOWS:

	    
for (const auto& frame : emptyFrames)    {
        
    CBorderPassElement::SBorderData box;
    if (w.focused_frame == frame) {
        
    box.grad1 = *ACTIVECOL;
    } else {
        box.grad1 = *INACTIVECOL;
    }
    // box.box = {20, 20, 100, 100};
    // box.box = frame->geometry;

    // auto geo = frame->geometry;

     // geo.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_pWindow.lock()));

    // const auto PWORKSPACE = m_pWindow->m_pWorkspace;

    // if (!PWORKSPACE)
    //     return box;

    // const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset->value() : Vector2D();
    // return box.translate(WORKSPACEOFFSET);    
    // 

		// auto reserved = window->getFullWindowReservedArea();
		auto geo = frame->getStandardWindowArea(frame->geometry, {}, monitor->activeWorkspace);
    // auto geo = frame.

     CBox windowBox = geo.translate(-monitor->vecPosition).scale(monitor->scale).round();   
     box.box = windowBox;
     //.expand(m_pWindow->getRealBorderSize())

     box.borderSize = *PBORDERSIZE;
    box.roundingPower = *PROUNDINGPOWER;
    box.round = *PROUNDING;
    auto element = CBorderPassElement(box);
    auto pass = makeShared<CBorderPassElement>(element);
    g_pHyprRenderer->m_sRenderPass.add(pass);
    }
    

    	
		break;
	default: break;
	}
}

