#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

SDispatchResult dispatch_debug_v2(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr)
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

    if (!valid(workspace_wrapper->workspace.lock())) {
        semmety_log(ERR, "workspace is not valid");
        semmety_log(ERR, "parent data after move: {}", parent->print());
    }

    auto p = workspace_wrapper->workspace.lock().get();
    if (p == nullptr) {
        
        semmety_log(ERR, "workspace is null");
    }

    auto w = workspace_wrapper->workspace;
    	auto m = g_pCompositor->m_pLastMonitor;
    auto& monitor = w->m_pMonitor;

    semmety_log(ERR, "monitor size {} {}", m->vecSize.x, m->vecSize.y);
    semmety_log(ERR, "workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);

    // frame->geometry.pos() = monitor->vecPosition + monitor->vecReservedTopLeft;
    // frame->geometry.size() = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;

    semmety_log(ERR, "{}", workspace_wrapper->root->print());

    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}


SDispatchResult dispatch_movefocus(std::string value) {
	// auto* workspace_wrapper = workspace_for_action(true);
	// if (workspace_wrapper == nullptr) return;
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}

SDispatchResult split(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr)
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

	  auto focused_frame = workspace_wrapper->getFocusedFrame();

	  
    
    focused_frame->data = SemmetyFrame::Parent(focused_frame, std::move(focused_frame->data), SemmetyFrame::Empty{});

    workspace_wrapper->focused_frame = *focused_frame->data.as_parent().children.begin();
    workspace_wrapper->apply();

    semmety_log(ERR, "post pslit:\n{}", workspace_wrapper->root->print());
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_remove(std::string arg) {
    auto* workspace_wrapper = workspace_for_action(true);
    if (workspace_wrapper == nullptr)
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

    auto focused_frame = workspace_wrapper->getFocusedFrame();

    if (!focused_frame || !focused_frame->data.is_leaf()) {
        semmety_log(ERR, "Can only remove leaf frames");
        return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }

    auto parent = focused_frame->get_parent();
    if (!parent || !parent->data.is_parent()) {
        semmety_log(ERR, "Frame has no parent, cannot remove the root frame!");
        return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }

    semmety_log(ERR, "here1");
    auto& siblings = parent->data.as_parent().children;
    auto remaining_sibling = std::find_if(siblings.begin(), siblings.end(),
        [&focused_frame](const SP<SemmetyFrame>& sibling) {
            return sibling != focused_frame;
        });

    semmety_log(ERR, "here2");
    if (focused_frame->data.is_window()) {
    semmety_log(ERR, "here3");
        workspace_wrapper->minimized_windows.push_back(focused_frame->data.as_window());
    }

    semmety_log(ERR, "here4");
    semmety_log(ERR, "Checking remaining_sibling validity");
    if (remaining_sibling != siblings.end()) {
        semmety_log(ERR, "remaining_sibling is valid");
    } else {
        semmety_log(ERR, "remaining_sibling is invalid (end iterator)");
    }

    semmety_log(ERR, "Checking parent validity");
    if (parent->data.is_parent()) {
        semmety_log(ERR, "parent is valid and is a parent");
    } else {
        semmety_log(ERR, "parent is invalid or not a parent");
    }

    if (remaining_sibling != siblings.end() && parent->data.is_parent()) {
        semmety_log(ERR, "remaining_sibling data: {}", (*remaining_sibling)->print());
        semmety_log(ERR, "parent data before move: {}", parent->print());
        parent->data = std::move((*remaining_sibling)->data);
    }

    semmety_log(ERR, "here5");
    workspace_wrapper->setFocusedFrame(parent);
    semmety_log(ERR, "post remove:\n{}", workspace_wrapper->root->print());
    workspace_wrapper->apply();
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}

SDispatchResult cycle_hidden(std::string arg) {
        semmety_log(ERR, "cycle hidden");
    auto* workspace_wrapper = workspace_for_action(true);
    if (workspace_wrapper == nullptr) {
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

}    auto focused_frame = workspace_wrapper->getFocusedFrame();
    
    if (!focused_frame->data.is_leaf()) {
        semmety_log(ERR, "Can only remove leaf frames");
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }
    
    if (workspace_wrapper->minimized_windows.empty()) {
        return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }    

    if (focused_frame->data.is_window()) {
        workspace_wrapper->minimized_windows.push_back(focused_frame->data.as_window());
    } 

    focused_frame->data = workspace_wrapper->minimized_windows.front();
    workspace_wrapper->minimized_windows.pop_front();
    workspace_wrapper->apply();
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:cycle_hidden", cycle_hidden);
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:split", split);
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:remove", dispatch_remove);
}
