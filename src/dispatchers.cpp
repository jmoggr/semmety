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
	auto workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) return;


}

void dispatch_split(std::string arg) {
	auto workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) return;

	auto focused_frame = workspace_wrapper.getFocusedFrame();
}

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
}
