#include "SemmetyWorkspaceWrapper.hpp"
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include <format>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

#include "SemmetyFrameUtils.hpp"
#include "log.hpp"
#include "src/SemmetyFrame.hpp"
#include "utils.hpp"

bool isWindowFloating(PHLWINDOWREF window) { return window && window->m_bIsFloating; }

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l): layout(l) {
	workspace = w;

	auto& monitor = w->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;

	auto frame = SemmetyLeafFrame::create({}, true);
	frame->geometry = {pos, size};

	root = frame;
	focused_frame = frame;

	semmety_log(ERR, "init workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);
	semmety_log(ERR, "workspace has root frame: {}", frame->print(*this));
}

void SemmetyWorkspaceWrapper::putWindowInFrame(PHLWINDOWREF window, SP<SemmetyLeafFrame> frame) {
	if (!window) {
		return;
	}

	const auto replacedWindow = frame->replaceWindow(*this, window);

	// Don't focus the window unless it is on the active workspace. This prevents active workspace
	// changing when a window is addded to an inactive workspace.
	if (workspace == g_pCompositor->m_pLastMonitor->activeWorkspace) {
		focusWindow(window);
	}

	if (!replacedWindow) {
		return;
	}

	auto emptyFrame = getLargestEmptyFrame();
	if (!emptyFrame) {
		replacedWindow->setHidden(true);
		return;
	}

	emptyFrame->setWindow(*this, replacedWindow);
}

void SemmetyWorkspaceWrapper::putWindowInFocussedFrame(PHLWINDOWREF window) {
	putWindowInFrame(window, focused_frame);
}

SP<SemmetyLeafFrame> SemmetyWorkspaceWrapper::getLargestEmptyFrame() {
	auto emptyFrames = root->getEmptyFrames();
	auto largestEmptyFrame =
	    std::min_element(emptyFrames.begin(), emptyFrames.end(), frameAreaGreater);

	if (largestEmptyFrame == emptyFrames.end()) {
		return nullptr;
	}

	return *largestEmptyFrame;
}

void SemmetyWorkspaceWrapper::addWindow(PHLWINDOWREF window) {
	if (!window) {
		semmety_critical_error("add window called with an invalid window");
	}

	windows.push_back(window);
	if (!window->m_bIsFloating) {
		putWindowInFocussedFrame(window);
	}
}

void SemmetyWorkspaceWrapper::setWindowTiled(PHLWINDOWREF window, bool isTiled) {
	if (!window) {
		// TODO: blow up in debug mode?
		return;
	}

	if (isTiled) {
		auto frame = getMostOverlappingLeafFrame(*this, window);
		if (!frame) {
			frame = getLargestEmptyFrame();
		}

		if (!frame) {
			frame = root->getLastFocussedLeaf();
		}

		if (!frame) {
			semmety_critical_error("frame is null, this should no be possible, getLastFocussedLeaf "
			                       "should always return something");
		}

		putWindowInFrame(window, frame);
		setFocusedFrame(frame);
	} else {
		auto frameWithWindow = getFrameForWindow(window);
		if (!frameWithWindow) {
			return;
		}

		auto newWindow = getNextWindowForFrame(frameWithWindow);
		auto _removedWindow = frameWithWindow->replaceWindow(*this, newWindow);
	}
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	for (auto& [key, vec]: frameHistoryMap) {
		vec.erase(std::remove(vec.begin(), vec.end(), window), vec.end());
	}

	auto frameWithWindow = getFrameForWindow(window);
	if (!frameWithWindow) {
		windows.erase(findWindowIt(window));
		return;
	}

	auto newWindow = getNextWindowForFrame(frameWithWindow);
	auto _removedWindow = frameWithWindow->replaceWindow(*this, newWindow);
	if (newWindow && frameWithWindow == focused_frame) {
		focusWindow(newWindow);
	}

	windows.erase(findWindowIt(window));
}

std::vector<PHLWINDOWREF>::iterator SemmetyWorkspaceWrapper::findWindowIt(PHLWINDOWREF window) {
	return std::find(windows.begin(), windows.end(), window);
}

PHLWINDOWREF SemmetyWorkspaceWrapper::getNextWindowForFrame(SP<SemmetyLeafFrame> frame) {
	const auto path = getFramePath(frame, root);
	auto& vec = frameHistoryMap[path];

	for (auto& window: std::views::reverse(vec)) {
		// The most recent window for a frame will be the one which is still in it, so this will skip
		// that window
		if (!window || isWindowVisible(window)) {
			continue;
		}

		return window;
	}

	GetNextWindowParams params = nextTiledWindowParams;
	if (auto window = frame->getWindow()) {
		params.startFromIndex = std::distance(windows.begin(), findWindowIt(window));
	}

	return getNextWindow(params);
}

bool SemmetyWorkspaceWrapper::isWindowVisible(PHLWINDOWREF window) const {
	if (window->m_bIsFloating) {
		return !window->isHidden();
	}

	// we don't use isHidden for tiled windows
	return isWindowInFrame(window);
}

// get the index of the the most recently focused window in this workspace which was not hidden
size_t SemmetyWorkspaceWrapper::getLastFocusedWindowIndex() {
	std::optional<size_t> minFocusIndex;
	size_t minIdx = 0;

	for (size_t idx = 0; idx < windows.size(); ++idx) {
		const auto window = windows[idx];
		if (!isWindowVisible(window)) {
			continue;
		}

		if (auto focus = getFocusHistoryIndex(window.lock())) {
			if (!minFocusIndex || *focus < *minFocusIndex) {
				minFocusIndex = *focus;
				minIdx = idx;
			}
		}
	}

	return minIdx;
}

bool windowMatchesMode(PHLWINDOWREF window, SemmetyWindowMode mode) {
	if (!window) {
		return false;
	}

	switch (mode) {
	case SemmetyWindowMode::Either: return true;
	case SemmetyWindowMode::Tiled: return !window->m_bIsFloating;
	case SemmetyWindowMode::Floating: return window->m_bIsFloating;
	}

	return false;
}

bool SemmetyWorkspaceWrapper::windowMatchesVisibility(
    PHLWINDOWREF window,
    SemmetyWindowVisibility mode
) {
	if (!window) {
		return false;
	}

	switch (mode) {
	case SemmetyWindowVisibility::Either: return true;
	case SemmetyWindowVisibility::Visible: return isWindowVisible(window);
	case SemmetyWindowVisibility::Hidden: return !isWindowVisible(window);
	}

	return false;
}

PHLWINDOWREF
SemmetyWorkspaceWrapper::getNextWindow(const GetNextWindowParams& params) {
	auto windowMode = params.windowMode.value_or(SemmetyWindowMode::Either);
	auto windowVisibility = params.windowVisibility.value_or(SemmetyWindowVisibility::Either);
	auto backward = params.backward.value_or(false);

	if (windows.size() == 0) {
		return {};
	}

	auto advanceIndex = [&](size_t currentIndex) -> size_t {
		return (windows.size() + currentIndex + (backward ? -1 : 1)) % windows.size();
	};

	size_t index;
	if (params.startFromIndex.has_value()) {
		index = params.startFromIndex.value() % windows.size();
	} else {
		index = getLastFocusedWindowIndex() % windows.size();
		index = advanceIndex(index);
	}

	for (size_t i = 0; i < windows.size(); i++) {
		const auto window = windows[index];

		if (windowMatchesMode(window, windowMode) && windowMatchesVisibility(window, windowVisibility))
		{
			return window;
		}

		index = advanceIndex(index);
	}

	return {};
}

SP<SemmetyLeafFrame> SemmetyWorkspaceWrapper::getFrameForWindow(PHLWINDOWREF window) const {
	const auto leafFrames = root->getLeafFrames();

	for (const auto& frame: leafFrames) {
		if (frame->getWindow() == window) {
			return frame;
		}
	}

	return nullptr;
}

bool SemmetyWorkspaceWrapper::isWindowInFrame(PHLWINDOWREF window) const {
	return !!getFrameForWindow(window);
}

SP<SemmetyLeafFrame> SemmetyWorkspaceWrapper::getFocusedFrame() { return focused_frame; }

void SemmetyWorkspaceWrapper::setFocusedFrame(SP<SemmetyFrame> frame) {
	static int focusOrder = 0;
	static auto PACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
	static auto PINACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
	auto* const ACTIVECOL = (CGradientValueData*) (PACTIVECOL.ptr())->getData();
	auto* const INACTIVECOL = (CGradientValueData*) (PINACTIVECOL.ptr())->getData();

	if (!frame) {
		semmety_critical_error("Cannot set a null frame as focused");
	}

	if (focused_frame == frame) {
		focusWindow(focused_frame->getWindow());
		return;
	}

	focused_frame->setBorderColor(*INACTIVECOL);

	focused_frame = frame->getLastFocussedLeaf();
	focused_frame->focusOrder = ++focusOrder;
	focused_frame->setBorderColor(*ACTIVECOL);
	focusWindow(focused_frame->getWindow());
}

void SemmetyWorkspaceWrapper::activateWindow(PHLWINDOWREF window) {
	if (!window) {
		return;
	}

	if (window->m_bIsFloating) {
		g_pCompositor->changeWindowZOrder(window.lock(), true);
		return;
	}

	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		setFocusedFrame(frameWithWindow);
		return;
	}

	// TODO: if window is not in workspace?
	putWindowInFocussedFrame(window);
}

void SemmetyWorkspaceWrapper::updateFrameHistory(SP<SemmetyFrame> frame, PHLWINDOWREF window) {
	const auto path = getFramePath(frame, root);

	auto& vec = frameHistoryMap[path];

	vec.erase(std::remove(vec.begin(), vec.end(), window), vec.end());
	vec.push_back(window);
}

bool isWindowFocussed(PHLWINDOWREF window) {
	return g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow == window;
}

std::string SemmetyWorkspaceWrapper::getDebugString() {
	std::string out = "";

	if (!workspace) {
		return "workspace is empty";
	}

	out += format("workspace id + name '{}' '{}'\n", workspace->m_szName, workspace->m_iID);
	out += "tiles:\n" + root->print(*this);

	out += "\nwindows:\n";
	for (const auto& window: windows) {
		if (!window) {
			out += "window is null";
			continue;
		}

		const auto ptrString = std::format("{:x}", (uintptr_t) window.get());
		const auto focusString = isWindowFocussed(window) ? "focus" : "     ";
		const auto hiddenString = isWindowVisible(window) ? "visible" : "hidden ";
		const auto floatingString = isWindowFloating(window) ? "floating" : "tiled   ";
		const auto frameString = isWindowInFrame(window) ? "inframe" : "       ";

		out += format(
		    "{} {} {} {} {} {}\n",
		    ptrString,
		    focusString,
		    hiddenString,
		    floatingString,
		    frameString,
		    window.lock()->m_szTitle
		);
	}

	// out += "\n";
	// for (auto& [key, vec]: frameHistoryMap) {
	// 	std::string ids = "";
	// 	for (auto& window: vec) {
	// 		ids = format("{} {}", ids, std::format("{:x}", (uintptr_t) window.get()));
	// 	}

	// 	out += format("{} {}\n", key, ids);
	// }

	return out;
}

void SemmetyWorkspaceWrapper::printDebug() {
	std::string debugStr = getDebugString();
	std::istringstream stream(debugStr);
	std::string line;
	while (std::getline(stream, line)) {
		semmety_log(ERR, "{}", line);
	}
}

json SemmetyWorkspaceWrapper::getWorkspaceWindowsJson() const {
	json jsonWindows = json::array();
	for (const auto& window: windows) {
		jsonWindows.push_back(
		    {{"address", std::format("{:x}", (uintptr_t) window.get())},
		     {"urgent", window->m_bIsUrgent},
		     {"title", window->fetchTitle()},
		     {"appid", window->fetchClass()},
		     {"focused", isWindowFocussed(window)},
		     {"minimized", !isWindowVisible(window)}}
		);
	}

	return jsonWindows;
}

void SemmetyWorkspaceWrapper::changeWindowOrder(bool prev) {
	if (windows.size() < 2) {
		return;
	}

	if (!g_pCompositor->m_pLastWindow) {
		return;
	}

	auto focusedWindow = g_pCompositor->m_pLastWindow.lock();
	auto it = findWindowIt(focusedWindow);
	size_t index = std::distance(windows.begin(), it);
	int offset = prev ? -1 : 1;
	size_t n = windows.size();

	size_t finalPos = (index + offset + n) % n;

	auto window = windows[index];
	windows.erase(windows.begin() + index);

	windows.insert(windows.begin() + finalPos, window);
}

std::vector<std::string> SemmetyWorkspaceWrapper::testInvariants() {
	std::vector<std::string> errors;

	// 1. Check that root is non-null.
	if (!root) {
		errors.push_back("Invariant violation: root is null.");
	}

	// 2. Check that all window pointers in windows are non-null.
	for (size_t i = 0; i < windows.size(); ++i) {
		if (windows[i] == nullptr) {
			errors.push_back(std::format("Invariant violation: workspace.windows[{}] is null.", i));
		}
	}

	// 3. Check that all windows in windows are unique.
	std::unordered_set<PHLWINDOWREF> workspaceWindowSet;
	for (size_t i = 0; i < windows.size(); ++i) {
		PHLWINDOWREF w = windows[i];
		if (!w) {
			continue;
		}

		if (workspaceWindowSet.find(w) != workspaceWindowSet.end()) {
			errors.push_back(std::format(
			    "Invariant violation: Duplicate window pointer found in workspace.windows at index {}.",
			    i
			));
		} else {
			workspaceWindowSet.insert(w);
		}
	}

	// 4. Traverse the frame tree starting at root to perform several frame-related checks.
	// We'll collect all frames, and also check that each frame is unique.
	std::vector<SP<SemmetyFrame>> allFrames;
	std::unordered_set<SemmetyFrame*> frameSet;

	// Recursive lambda to traverse the frame tree.
	std::function<void(const SP<SemmetyFrame>&)> traverseFrames = [&](const SP<SemmetyFrame>& frame) {
		if (!frame) {
			errors.push_back("Invariant violation: null frame encountered in frame tree.");
			return;
		}

		// Check for duplicate frames.
		if (frameSet.find(frame.get()) != frameSet.end()) {
			errors.push_back("Invariant violation: Duplicate frame encountered in frame tree.");
		} else {
			frameSet.insert(frame.get());
			allFrames.push_back(frame);
		}

		if (!frame->isSplit()) {
			return;
		}

		// If the frame is a split frame, we need to check:
		// - both children are valid (non-null)
		// - then traverse both children.
		auto splitFrame = frame->asSplit();
		if (!splitFrame->children.first) {
			errors.push_back("Invariant violation: Split frame has a null first child.");
		}
		if (!splitFrame->children.second) {
			errors.push_back("Invariant violation: Split frame has a null second child.");
		}
		if (splitFrame->children.first) {
			traverseFrames(splitFrame->children.first);
		}
		if (splitFrame->children.second) {
			traverseFrames(splitFrame->children.second);
		}
	};

	if (root) {
		traverseFrames(root);
	}

	// 5. Check that in leaf frames:
	// - window pointer is non-null (if the frame is not empty),
	// - windows in frames are unique,
	// - and that windows in frames are not minimized/hidden.
	std::unordered_set<PHLWINDOWREF> frameWindowSet;
	for (const auto& frame: allFrames) {
		if (!frame->isLeaf()) {
			continue;
		}

		auto leafFrame = frame->asLeaf();
		if (leafFrame->isEmpty()) {
			continue;
		}

		auto window = leafFrame->getWindow();
		if (window == nullptr) {
			errors.push_back("Invariant violation: Leaf frame contains a null window pointer.");
			continue;
		}

		// Check uniqueness of windows in frames.
		if (frameWindowSet.find(window) != frameWindowSet.end()) {
			errors.push_back("Invariant violation: Duplicate window pointer found in frames.");
		} else {
			frameWindowSet.insert(window);
		}

		// Windows in frames should not be hidden/minimized.
		// if (isWindowHidden(window)) {
		// 	errors.push_back("Invariant violation: Window in a frame is hidden.");
		// }
	}

	// 6. There should be no empty leaf frames if there are minimized windows.
	// bool hasMinimizedWindow = false;
	// for (const auto& w: windows) {
	// 	if (isWindowMinimized(w)) {
	// 		hasMinimizedWindow = true;
	// 		break;
	// 	}
	// }

	// if (hasMinimizedWindow) {
	// 	for (const auto& frame: allFrames) {
	// 		if (!frame->isLeaf()) {
	// 			continue;
	// 		}

	// 		auto leafFrame = frame->asLeaf();
	// 		if (leafFrame->isEmpty()) {
	// 			errors.push_back("Invariant violation: An empty leaf frame exists despite the presence "
	// 			                 "of minimized windows.");
	// 		}
	// 	}
	// }

	// // 7. Windows not assigned to any frame should be hidden/minimized.
	// // For every window in windows that does not appear in a frame, check that it is
	// // minimized.
	// for (const auto& w: windows) {
	// 	if (isWindowMinimized(w) && !w->isHidden()) {
	// 		errors.push_back(std::format(
	// 		    "Invariant violation: Window not assigned to any frame is not hidden. {}",
	// 		    w->fetchTitle()
	// 		));
	// 	}
	// }

	return errors;
}
