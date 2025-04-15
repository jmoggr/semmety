#pragma once

#include <unordered_map>
#include <vector>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "SemmetyFrame.hpp"
#include "json.hpp"
#include "src/desktop/DesktopTypes.hpp"
using json = nlohmann::json;

class SemmetyLayout;

enum class SemmetyWindowMode {
	Tiled,
	Floating,
	Either,
};

enum class SemmetyWindowVisibility {
	Visible,
	Hidden,
	Either,
};

struct GetNextWindowParams {
	std::optional<bool> backward;
	std::optional<size_t> startFromIndex;
	std::optional<SemmetyWindowMode> windowMode;
	std::optional<SemmetyWindowVisibility> windowVisibility;
};

const GetNextWindowParams nextTiledWindowParams = {
    .windowMode = SemmetyWindowMode::Tiled,
    .windowVisibility = SemmetyWindowVisibility::Hidden,
};

class SemmetyWorkspaceWrapper {
public:
	SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout&);
	PHLWORKSPACEREF workspace;
	SemmetyLayout& layout;
	std::vector<PHLWINDOWREF> windows;
	std::unordered_map<std::string, std::vector<PHLWINDOWREF>> frameHistoryMap;

	size_t getLastFocusedWindowIndex();

	PHLWINDOWREF getNextWindow(const GetNextWindowParams& params = {});

	void addWindow(PHLWINDOWREF w);
	void removeWindow(PHLWINDOWREF window);
	SP<SemmetyLeafFrame> getFocusedFrame();
	void setFocusedFrame(SP<SemmetyFrame> frame);
	SP<SemmetyLeafFrame> getFrameForWindow(PHLWINDOWREF window) const;
	void advanceFrameWithWindow(PHLWINDOWREF window, bool focusNextWindow);
	std::vector<PHLWINDOWREF>::iterator findWindowIt(PHLWINDOWREF window);
	bool isWindowInFrame(PHLWINDOWREF window) const;
	bool isWindowVisible(PHLWINDOWREF window) const;
	void putWindowInFocussedFrame(PHLWINDOWREF window);
	void setWindowTiled(PHLWINDOWREF window, bool isTiled);
	void printDebug();
	void changeWindowOrder(bool prev);
	json getWorkspaceWindowsJson() const;
	void activateWindow(PHLWINDOWREF window);
	SP<SemmetyLeafFrame> getLargestEmptyFrame();
	void updateFrameHistory(SP<SemmetyFrame> frame, PHLWINDOWREF window);
	bool windowMatchesVisibility(PHLWINDOWREF window, SemmetyWindowVisibility mode);
	PHLWINDOWREF getNextWindowForFrame(SP<SemmetyLeafFrame> frame);
	std::vector<std::string> testInvariants();

	// TODO: should also be private
	SP<SemmetyFrame> root;

private:
	SP<SemmetyLeafFrame> focused_frame;
};
