#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/string/String.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

SDispatchResult dispatch_debug_v2(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr)
    semmety_log(ERR, "{}", workspace_wrapper->root->print());
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

        semmety_log(ERR, workspace_wrapper->root->print());

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
    workspace_wrapper->apply();
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_remove(std::string arg) {
    auto* workspace_wrapper = workspace_for_action(true);
    if (workspace_wrapper == nullptr)
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};

    auto focused_frame = workspace_wrapper->getFocusedFrame();

    if (!focused_frame->data.is_leaf()) {
        semmety_log(ERR, "Can only remove leaf frames");
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }

    auto parent = focused_frame->get_parent();
    if (!parent) {
        semmety_log(ERR, "Frame has no parent, cannot remove the root frame!");
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
    }

    auto& siblings = parent->data.as_parent().children;
    auto remaining_sibling = std::find_if(siblings.begin(), siblings.end(),
        [&focused_frame](const SP<SemmetyFrame>& sibling) {
            return sibling != focused_frame;
        });

    if (focused_frame->data.is_window()) {
        workspace_wrapper->minimized_windows.push_back(focused_frame->data.as_window());
    }

    if (remaining_sibling != siblings.end()) {
        parent->data = std::move((*remaining_sibling)->data);
    }

    workspace_wrapper->setFocusedFrame(parent);
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
