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

	auto& focused_frame = workspace_wrapper->getFocusedFrame();

    // Create a new parent frame data
    SemmetyFrame::Parent parentData;

    // Determine the data for the first child
    if (focused_frame.data.is_window()) {
        // If the focused frame had a window, the first child should have that window
        auto window = focused_frame.data.as_window();
        auto firstChild = std::make_shared<SemmetyFrame>(window);
        parentData.children.push_back(firstChild);
    } else {
        // Otherwise, the first child should be empty
        auto firstChild = std::make_shared<SemmetyFrame>(SemmetyFrame::Empty{});
        parentData.children.push_back(firstChild);
    }

    // The second child should always be empty
    auto secondChild = std::make_shared<SemmetyFrame>(SemmetyFrame::Empty{});
    parentData.children.push_back(secondChild);

    // Set the focused frame's data to be the new parent
    focused_frame.data = SemmetyFrame::FrameData(std::move(parentData));

}

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
}
