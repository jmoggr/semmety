#pragma once

#include <map>
#include <vector>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "SemmetyFrame.hpp"
#include "src/desktop/DesktopTypes.hpp"
#include "json.hpp"
using json = nlohmann::json;

enum class Direction { Up, Down, Left, Right };
// Forward declaration
class SemmetyLayout;

class SemmetyWorkspaceWrapper {
public:
	PHLWORKSPACEREF workspace;
	SemmetyLayout& layout;
	std::vector<PHLWINDOWREF> windows;
	SP<SemmetyFrame> root;
	SP<SemmetyFrame> focused_frame;

	size_t getLastFocusedWindowIndex();
	PHLWINDOWREF getNextMinimizedWindow();
	PHLWINDOWREF getPrevMinimizedWindow();
	SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout&);
	void addWindow(PHLWINDOWREF w);
	void removeWindow(PHLWINDOWREF window);
	void putWindowInFocusedFrame(PHLWINDOWREF w);
	SP<SemmetyFrame> getFocusedFrame();
	void setFocusedFrame(SP<SemmetyFrame> frame);
	std::list<SP<SemmetyFrame>> getWindowFrames() const;
	SP<SemmetyFrame> getFrameForWindow(PHLWINDOWREF window) const;
	std::list<SP<SemmetyFrame>> getEmptyFrames() const;
	std::list<SP<SemmetyFrame>> getLeafFrames() const;
	SP<SemmetyFrame> getNeighborByDirection(SP<SemmetyFrame> basis, Direction dir);
	void apply();
	void rebalance();
	void printDebug();
	std::list<PHLWINDOWREF> getMinimizedWindows() const;
	void setFocusShortcut(std::string shortcutKey);
	void activateFocusShortcut(std::string shortcutKey);
	std::map<std::string, PHLWINDOWREF> focusShortcuts;
	json getWorkspaceWindowsJson();
};
