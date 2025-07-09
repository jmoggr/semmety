#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <hyprlang.hpp>

#include "SemmetyLayout.hpp"
#include "dispatchers.hpp"
#include "globals.hpp"
#include "src/log.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
	PHANDLE = handle;

	semmety_log(ERR, "HERE -1");
	const std::string HASH = __hyprland_api_get_hash();

	if (HASH != GIT_COMMIT_HASH) {
		HyprlandAPI::addNotification(
		    PHANDLE,
		    "[MyPlugin] Mismatched headers! Can't proceed.",
		    CHyprColor {1.0, 0.2, 0.2, 1.0},
		    5000
		);
		semmety_log(ERR, "HERE 0");
		semmety_critical_error("[MyPlugin] Version mismatch {} != {}", HASH, GIT_COMMIT_HASH);
	}

	semmety_log(ERR, "HERE 1");
	g_SemmetyLayout = std::make_unique<SemmetyLayout>();
	[[maybe_unused]] auto res = HyprlandAPI::addLayout(PHANDLE, "semmety", g_SemmetyLayout.get());
	semmety_log(ERR, "HERE 2");
	if (res) {
		semmety_log(ERR, "HERE 3");
		// TODO: handle error?
	}

	semmety_log(ERR, "HERE 4");
	registerDispatchers();
	semmety_log(ERR, "HERE 5");
	HyprlandAPI::reloadConfig();
	semmety_log(ERR, "HERE 6");

	return {"semmety", "Semi automatic tiling window manager", "jmoggr", "0.4"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
