#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <hyprlang.hpp>

#include "SemmetyEventManager.hpp"
#include "SemmetyLayout.hpp"
#include "dispatchers.hpp"
#include "globals.hpp"
#include "src/log.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
	PHANDLE = handle;

	const std::string HASH = __hyprland_api_get_hash();

	Debug::log(ERR, "[semmety] new!");
	// Log version info but don't fail - version checks are often too strict
	HyprlandAPI::addNotification(
	    PHANDLE,
	    std::format("[semmety] Loaded (Hyprland API: {})", HASH).c_str(),
	    CHyprColor {0.2, 1.0, 0.2, 1.0},
	    3000
	);

	g_SemmetyEventManager = makeUnique<SemmetyEventManager>();
	g_SemmetyLayout = std::make_unique<SemmetyLayout>();
	[[maybe_unused]] auto res = HyprlandAPI::addLayout(PHANDLE, "semmety", g_SemmetyLayout.get());
	if (res) {
		// TODO: handle error?
	}

	registerDispatchers();
	HyprlandAPI::reloadConfig();

	return {"semmety", "Semi automatic tiling window manager", "jmoggr", "0.4"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
