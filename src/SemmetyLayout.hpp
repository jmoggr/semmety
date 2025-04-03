#pragma once

#include <list>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "SemmetyWorkspaceWrapper.hpp"
#include "json.hpp"

using json = nlohmann::json;

class SemmetyLayout: public IHyprLayout {
public:
	//
	// IHyprLayout
	//

	void onEnable() override;
	void onDisable() override;
	// void onWindowCreated(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT) override;
	void onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT) override;
	// void onWindowCreatedFloating(PHLWINDOW) override;
	// bool onWindowCreatedAutoGroup(PHLWINDOW) override;
	bool isWindowTiled(PHLWINDOW) override;
	// void onWindowRemoved(PHLWINDOW) override;
	void onWindowRemovedTiling(PHLWINDOW) override;
	// void onWindowRemovedFloating(PHLWINDOW) override;
	void recalculateMonitor(const MONITORID&) override;
	void recalculateWindow(PHLWINDOW) override;
	// void changeWindowFloatingMode(PHLWINDOW) override;
	void onBeginDragWindow() override;
	// void onEndDragWindow() override;
	// void onMouseMove(const Vector2D&) override;
	void
	resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr)
	    override;
	void fullscreenRequestForWindow(
	    PHLWINDOW pWindow,
	    const eFullscreenMode CURRENT_EFFECTIVE_MODE,
	    const eFullscreenMode EFFECTIVE_MODE
	) override;
	std::any layoutMessage(SLayoutMessageHeader, std::string) override;
	SWindowRenderLayoutHints requestRenderHints(PHLWINDOW) override;
	void switchWindows(PHLWINDOW, PHLWINDOW) override;
	void moveWindowTo(PHLWINDOW, const std::string& direction, bool silent = false) override;
	// void moveActiveWindow(const Vector2D&, PHLWINDOW pWindow = nullptr) override;
	void alterSplitRatio(PHLWINDOW, float, bool exact = false) override;
	std::string getLayoutName() override;
	PHLWINDOW getNextWindowCandidate(PHLWINDOW) override;
	void onWindowFocusChange(PHLWINDOW) override;
	void replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) override;
	Vector2D predictSizeForNewWindowTiled() override;
	// Vector2D predictSizeForNewWindow(PHLWINDOW pWindow) override;
	// Vector2D predictSizeForNewWindowFloating(PHLWINDOW pWindow) override;
	bool isWindowReachable(PHLWINDOW) override;
	// void bringWindowToTop(PHLWINDOW) override;
	// void requestFocusForWindow(PHLWINDOW) override;

	//
	// Semmety
	//

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
