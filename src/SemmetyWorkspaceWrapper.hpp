#pragma once

#include <vector>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "SemmetyFrame.hpp"
#include "json.hpp"
#include "src/desktop/DesktopTypes.hpp"
using json = nlohmann::json;

class SemmetyLayout;

class SemmetyWorkspaceWrapper {
public:
	SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout&);
	PHLWORKSPACEREF workspace;
	SemmetyLayout& layout;
	std::vector<PHLWINDOWREF> windows;

	size_t getLastFocusedWindowIndex();
	PHLWINDOWREF getNextMinimizedWindow(std::optional<size_t> fromIndex = std::nullopt);
	PHLWINDOWREF getPrevMinimizedWindow();
	void addWindow(PHLWINDOWREF w);
	void removeWindow(PHLWINDOWREF window);
	SP<SemmetyLeafFrame> getFocusedFrame();
	void setFocusedFrame(SP<SemmetyFrame> frame);
	SP<SemmetyLeafFrame> getFrameForWindow(PHLWINDOWREF window) const;
	void updateHiddenAndFocusedWindows();
	void rebalance();
	void printDebug() const;
	void changeWindowOrder(bool prev);
	json getWorkspaceWindowsJson() const;
	void activateWindow(PHLWINDOWREF window);
	bool isWindowMinimized(PHLWINDOWREF window) const;
	bool isWindowFocussed(PHLWINDOWREF window) const;

	// TODO: should also be private
	SP<SemmetyFrame> root;

private:
	SP<SemmetyLeafFrame> focused_frame;
};
