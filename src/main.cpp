#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <hyprlang.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
	PHANDLE = handle;

	const std::string HASH = __hyprland_api_get_hash();

	if (HASH != GIT_COMMIT_HASH) {
		HyprlandAPI::addNotification(
		    PHANDLE,
		    "[MyPlugin] Mismatched headers! Can't proceed.",
		    CHyprColor {1.0, 0.2, 0.2, 1.0},
		    5000
		);
		throw std::runtime_error("[MyPlugin] Version mismatch");
	}

	g_SemmetyLayout = std::make_unique<SemmetyLayout>();
	auto res = HyprlandAPI::addLayout(PHANDLE, "semmety", g_SemmetyLayout.get());

	if (res) {
		semmety_log(ERR, "success add layout");
	} else {

		semmety_log(ERR, "fail add layout");
	}

	semmety_log(ERR, "in init");
	registerDispatchers();

	HyprlandAPI::reloadConfig();

	return {"semmety", "Semi automatic tiling window manager", "jmoggr", "0.4"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
