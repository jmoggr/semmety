#pragma once

#include <list>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "SemmetyWorkspaceWrapper.hpp"
#include "json.hpp"
using json = nlohmann::json;

class SemmetyLayout;
class SemmetyWorkspaceWrapper;

class SemmetyLayout: public IHyprLayout {
public:
	void onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT) override;
	void onWindowRemovedTiling(PHLWINDOW) override;
	bool isWindowTiled(PHLWINDOW) override;
	void recalculateMonitor(const MONITORID&) override;
	void recalculateWindow(PHLWINDOW) override;
	// void onBeginDragWindow() override;

	PHLWINDOW getNextWindowCandidate(PHLWINDOW) override;
	void
	resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr)
	    override;
	void fullscreenRequestForWindow(
	    PHLWINDOW pWindow,
	    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
	    const eFullscreenMode EFFECTIVE_MODE
	) override;

	bool isWindowReachable(PHLWINDOW) override;
	std::any layoutMessage(SLayoutMessageHeader, std::string) override;
	SWindowRenderLayoutHints requestRenderHints(PHLWINDOW) override;
	void switchWindows(PHLWINDOW, PHLWINDOW) override;
	void moveWindowTo(PHLWINDOW, const std::string& dir, bool silent) override;
	void alterSplitRatio(PHLWINDOW, float, bool) override;
	std::string getLayoutName() override;
	void replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) override;
	Vector2D predictSizeForNewWindowTiled() override;
	void onWindowFocusChange(PHLWINDOW) override;

	void onEnable() override;
	void onDisable() override;

	static void renderHook(void*, SCallbackInfo&, std::any);
	static void tickHook(void*, SCallbackInfo&, std::any);
	static void activeWindowHook(void*, SCallbackInfo&, std::any);
	static void workspaceHook(void*, SCallbackInfo&, std::any);
	static void urgentHook(void*, SCallbackInfo&, std::any);
	void moveWindowToWorkspace(std::string wsname);

	void recalculateWorkspace(const PHLWORKSPACE& workspace);
	SemmetyWorkspaceWrapper& getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace);

	std::list<SemmetyWorkspaceWrapper> workspaceWrappers;
	bool updateBarOnNextTick = false;
	void activateWindow(PHLWINDOW window);
	void changeWindowOrder(bool prev);
	json getWorkspacesJson();
};
