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
	std::unordered_map<PHLWINDOWREF, std::vector<std::string>> windowFrameHistory;

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
	std::string getDebugString();
	void changeWindowOrder(bool prev);
	json getWorkspaceWindowsJson() const;
	void activateWindow(PHLWINDOWREF window);
	SP<SemmetyLeafFrame> getLargestEmptyFrame();
	void updateFrameHistory(SP<SemmetyFrame> frame, PHLWINDOWREF window);
	std::vector<std::string> getWindowFrameHistory(PHLWINDOWREF window) const;
	bool windowMatchesVisibility(PHLWINDOWREF window, SemmetyWindowVisibility mode);
	PHLWINDOWREF getNextWindowForFrame(SP<SemmetyLeafFrame> frame);
	void putWindowInFrame(PHLWINDOWREF window, SP<SemmetyLeafFrame> frame);
	std::vector<std::string> testInvariants();
	const SP<SemmetyFrame>& getRoot() const;
	void setRootGeometry(const CBox& geometry);

private:
	SP<SemmetyFrame> root;
	SP<SemmetyLeafFrame> focused_frame;

	void traverseFramesForInvariants(
	    const SP<SemmetyFrame>& frame,
	    std::vector<std::string>& errors,
	    std::unordered_set<SemmetyFrame*>& frameSet,
	    std::vector<SP<SemmetyFrame>>& allFrames
	);

	friend void replaceNode(SP<SemmetyFrame>, SP<SemmetyFrame>, SemmetyWorkspaceWrapper&);
};
