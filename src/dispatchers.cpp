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
	auto workspace = workspace_for_action(true);
	if (!valid(workspace)) return;

	// auto args = CVarList(value);

	// static const auto no_cursor_warps = ConfigValue<Hyprlang::INT>("cursor:no_warps");
	// auto warp_cursor = !*no_cursor_warps;

	// int argi = 0;
	// auto shift = parseShiftArg(args[argi++]);
	// if (!shift) return;
	// if (workspace->m_bHasFullscreenWindow) {
	// 	g_Hy3Layout->focusMonitor(shift.value());
	// 	return;
	// }

	// auto visible = args[argi] == "visible";
	// if (visible) argi++;

	// if (args[argi] == "nowarp") warp_cursor = false;
	// else if (args[argi] == "warp") warp_cursor = true;

	// g_Hy3Layout->shiftFocus(workspace.get(), shift.value(), visible, warp_cursor);
}

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
}
