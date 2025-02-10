#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/string/String.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

void dispatch_debug(std::string arg) {
	semmety_log(LOG, "Semmety Debug");
}

void registerDispatchers() {
	HyprlandAPI::addDispatcher(PHANDLE, "semmety:debug", dispatch_debug);
}
