#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/string/String.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

SDispatchResult dispatch_debug_v2(std::string arg) {
    semmety_log(LOG, "Semmety Debug");
    return SDispatchResult{.passEvent = false, .success = true, .error = ""};
}


void dispatch_movefocus(std::string value) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) return;


}

void dispatch_split(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) return;

	auto focused_frame = workspace_wrapper->getFocusedFrame();

  focused_frame->data = SemmetyFrame::Parent(focused_frame, std::move(focused_frame->data), SemmetyFrame::Empty{});
}

void dispatch_remove(std::string arg) {
    auto* workspace_wrapper = workspace_for_action(true);
    if (workspace_wrapper == nullptr) return;

    auto focused_frame = workspace_wrapper->getFocusedFrame();

    if (!focused_frame->data.is_leaf()) {
        semmety_log(ERR, "Can only remove leaf frames");
        return;
    }

    auto parent = focused_frame->get_parent();
    if (!parent) {
        semmety_log(DEBUG, "Frame has no parent, cannot remove the root frame!");
        return;
    }

    auto& children = parent->data.as_parent().children;
    auto remaining_child = std::find_if(children.begin(), children.end(),
        [&focused_frame](const SP<SemmetyFrame>& child) {
            return child != focused_frame;
        });

    if (focused_frame->data.is_window()) {
        workspace_wrapper->minimized_windows.push_back(focused_frame->data.as_window());
    }

    if (remaining_child != children.end()) {
        parent->data = (*remaining_child)->data;
    }
}
}

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
}
