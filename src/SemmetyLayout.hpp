#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <list>

#include "SemmetyFrame.hpp"
#include "log.hpp"


PHLWORKSPACE workspace_for_action(bool allow_fullscreen = false);

class SemmetyLayout: public IHyprLayout {
    void onEnable() override;
    void onDisable() override;

    /*
        Called when a window is created (mapped)
        The layout HAS TO set the goal pos and size (anim mgr will use it)
        If !animationinprogress, then the anim mgr will not apply an anim.
    */
    void onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT) override;

    SemmetyWorkspaceWrapper& getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace);
    void applyFrameDataToWindow(SemmetyFrame*, bool no_animation = false);
    // SemmetyFrame* getWorkspaceRootFrame(const CWorkspace* workspace) {

		// std::list<SemmetyNode> nodes;
		std::list<SemmetyWorkspaceWrapper> workspaceWrappers; 

    // /*
    //     Return tiled status
    // */
    bool isWindowTiled(PHLWINDOW) override;

    // /*
    //     Called when a window is removed (unmapped)
    // */
    // void onWindowRemoved(PHLWINDOW);
    void onWindowRemovedTiling(PHLWINDOW) override;
    // void onWindowRemovedFloating(PHLWINDOW);
    // /*
    //     Called when the monitor requires a layout recalculation
    //     this usually means reserved area changes
    // */
    void recalculateMonitor(const MONITORID&) override;

    // /*
    //     Called when the compositor requests a window
    //     to be recalculated, e.g. when pseudo is toggled.
    // */
    void recalculateWindow(PHLWINDOW) override;

    // /*
    //     Called when a window is requested to be floated
    // */
    // void changeWindowFloatingMode(PHLWINDOW);
    // /*
    //     Called when a window is clicked on, beginning a drag
    //     this might be a resize, move, whatever the layout defines it
    //     as.
    // */
    // void onBeginDragWindow();
    // /*
    //     Called when a user requests a resize of the current window by a vec
    //     Vector2D holds pixel values
    //     Optional pWindow for a specific window
    // */
    void resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr) override;
    // /*
    //     Called when a user requests a move of the current window by a vec
    //     Vector2D holds pixel values
    //     Optional pWindow for a specific window
    // */
    // void moveActiveWindow(const Vector2D&, PHLWINDOW pWindow = nullptr);
    // /*
    //     Called when a window is ended being dragged
    //     (mouse up)
    // */
    // void onEndDragWindow();
    // /*
    //     Called whenever the mouse moves, should the layout want to
    //     do anything with it.
    //     Useful for dragging.
    // */
    // void onMouseMove(const Vector2D&);

    // /*
    //     Called when a window / the user requests to toggle the fullscreen state of a window
    //     The layout sets all the fullscreen flags.
    //     It can either accept or ignore.
    // */
    void fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) override;

    // /*
    //     Called when a dispatcher requests a custom message
    //     The layout is free to ignore.
    //     std::any is the reply. Can be empty.
    // */
    std::any layoutMessage(SLayoutMessageHeader, std::string) override;

    // /*
    //     Required to be handled, but may return just SWindowRenderLayoutHints()
    //     Called when the renderer requests any special draw flags for
    //     a specific window, e.g. border color for groups.
    // */
    SWindowRenderLayoutHints requestRenderHints(PHLWINDOW) override;

    // /*
    //     Called when the user requests two windows to be swapped places.
    //     The layout is free to ignore.
    // */
    void switchWindows(PHLWINDOW, PHLWINDOW) override;

    // /*
    //     Called when the user requests a window move in a direction.
    //     The layout is free to ignore.
    // */
    void moveWindowTo(PHLWINDOW, const std::string& direction, bool silent = false) override;

    // /*
    //     Called when the user requests to change the splitratio by or to X
    //     on a window
    // */
    void alterSplitRatio(PHLWINDOW, float, bool exact = false) override;

    // /*
    //     Called when something wants the current layout's name
    // */
    std::string getLayoutName() override;

    // /*
    //     Called for getting the next candidate for a focus
    // */
    // PHLWINDOW getNextWindowCandidate(PHLWINDOW);

    // /*
    //     Internal: called when window focus changes
    // */
    // void onWindowFocusChange(PHLWINDOW);

    // /*
    //     Called for replacing any data a layout has for a new window
    // */
    void replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) override;

    // /*
    //     Determines if a window can be focused. If hidden this usually means the window is part of a group.
    // */
    // bool isWindowReachable(PHLWINDOW);

    // /*
    //     Called before an attempt is made to focus a window.
    //     Brings the window to the top of any groups and ensures it is not hidden.
    //     If the window is unmapped following this call, the focus attempt will fail.
    // */
    // void bringWindowToTop(PHLWINDOW);

    // /*
    //     Called via the foreign toplevel activation protocol.
    //     Focuses a window, bringing it to the top of its group if applicable.
    //     May be ignored.
    // */
    // void requestFocusForWindow(PHLWINDOW);

    // /*
    //     Called to predict the size of a newly opened window to send it a configure.
    //     Return 0,0 if unpredictable
    // */
    Vector2D predictSizeForNewWindowTiled() override;

    // /*
    //     Prefer not overriding, use predictSizeForNewWindowTiled.
    // */
    // Vector2D predictSizeForNewWindow(PHLWINDOW pWindow);
    // Vector2D predictSizeForNewWindowFloating(PHLWINDOW pWindow);


};
