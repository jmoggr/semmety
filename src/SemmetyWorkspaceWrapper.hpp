#pragma once

#include <list>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "SemmetyFrame.hpp"

// Forward declaration
class SemmetyLayout;

class SemmetyWorkspaceWrapper {
public:
	PHLWORKSPACEREF workspace;
	SemmetyLayout& layout;
	std::list<PHLWINDOWREF> minimized_windows;
	SP<SemmetyFrame> root;
	SP<SemmetyFrame> focused_frame;

	SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout&);
	void addWindow(PHLWINDOWREF w);
	void putWindowInFocusedFrame(PHLWINDOWREF w);
	SP<SemmetyFrame> getFocusedFrame();
	void setFocusedFrame(SP<SemmetyFrame> frame);
	std::list<SP<SemmetyFrame>> getWindowFrames() const;
	SP<SemmetyFrame> getFrameForWindow(PHLWINDOWREF window) const;
	void minimizeWindow(PHLWINDOWREF window);
	void removeWindow(PHLWINDOWREF window);
	std::list<SP<SemmetyFrame>> getEmptyFrames() const;
	std::list<SP<SemmetyFrame>> getLeafFrames() const;
	void apply();
	void rebalance();
};
