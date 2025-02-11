#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include "SemmetyFrame.hpp"

#include <list>

class SemmetyWorkspaceWrapper {
public:
	PHLWORKSPACEREF workspace;
	std::list<PHLWINDOWREF> minimized_windows;
	SP<SemmetyFrame> root;
	SP<SemmetyFrame> focused_frame;

	SemmetyWorkspaceWrapper(PHLWORKSPACEREF w);
    void addWindow(PHLWINDOWREF w);
  void putWindowInFocusedFrame(PHLWINDOWREF w);
  SemmetyFrame& getFocusedFrame();
    std::list<SP<SemmetyFrame>> getWindowFrames() const;
    SP<SemmetyFrame> getFrameForWindow(PHLWINDOWREF window) const;
    void minimizeWindow(PHLWINDOWREF window);
    void removeWindow(PHLWINDOWREF window);
};
