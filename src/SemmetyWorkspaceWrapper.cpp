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
#include "utils.hpp"

bool isWindowHidden(PHLWINDOWREF window) { return window && window->isHidden(); }
bool isWindowFloating(PHLWINDOWREF window) { return window && window->m_bIsFloating; }

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l): layout(l) {
	workspace = w;

	auto& monitor = w->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;
	PHLWINDOWREF empty = {};
	auto frame = SemmetyLeafFrame::create(empty);

	this->root = frame;
	this->focused_frame = frame;

	frame->geometry = {pos, size};
	semmety_log(ERR, "init workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);
	semmety_log(ERR, "workspace has root frame: {}", frame->print(*this));
}

void SemmetyWorkspaceWrapper::putWindowInFocussedFrame(PHLWINDOWREF window) {
	if (!window) {
		return;
	}

	const auto replacedWindow = focused_frame->replaceWindow(*this, window);

	// Don't focus the window unless it is on the active workspace. This prevents active workspace
	// changing when a window is addded to an inactive workspace.
	if (workspace == g_pCompositor->m_pLastMonitor->activeWorkspace) {
		focusWindow(window);
	}

	if (!replacedWindow) {
		return;
	}

	auto emptyFrames = root->getEmptyFrames();
	if (emptyFrames.empty()) {
		replacedWindow->setHidden(true);
		return;
	}

	auto largestFrameIt = std::min_element(emptyFrames.begin(), emptyFrames.end(), frameAreaGreater);
	if (largestFrameIt == emptyFrames.end()) {
		replacedWindow->setHidden(true);
		return;
	}

	(*largestFrameIt)->setWindow(*this, replacedWindow);
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
	if (isTiled) {
		auto lastFocussed = root->getLastFocussedLeaf();
		setFocusedFrame(lastFocussed);

		auto emptyFrames = root->getEmptyFrames();

		if (emptyFrames.size() == 0) {
			window->setHidden(true);
			return;
		}

		// TODO: put floating window in closest frame

		// sort empty frames by size, largest first
		std::sort(emptyFrames.begin(), emptyFrames.end(), frameAreaGreater);
		emptyFrames[0]->setWindow(*this, window);
		if (emptyFrames[0] == focused_frame) {
			focusWindow(window);
		}
	} else {
		advanceFrameWithWindow(window, false);
	}
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	advanceFrameWithWindow(window, true);
	windows.erase(findWindowIt(window));
}

std::vector<PHLWINDOWREF>::iterator SemmetyWorkspaceWrapper::findWindowIt(PHLWINDOWREF window) {
	auto it = std::find(windows.begin(), windows.end(), window);
	if (it == windows.end()) {
		semmety_critical_error("Window is not in the workspace");
	}
	return it;
}

void SemmetyWorkspaceWrapper::advanceFrameWithWindow(PHLWINDOWREF window, bool focusNextWindow) {
	auto frameWithWindow = getFrameForWindow(window);
	if (!frameWithWindow) {
		return;
	}

	auto it = findWindowIt(window);
	size_t index = std::distance(windows.begin(), it);
	GetNextWindowParams params = nextTiledWindowParams;
	params.startFromIndex = index;

	auto nextWindow = getNextWindow(params);

	// _replacedWindow == window, you better be handling the window being removed
	auto _replacedWindow = frameWithWindow->replaceWindow(*this, nextWindow);

	if (focusNextWindow && frameWithWindow == focused_frame) {
		focusWindow(nextWindow);
	}
}

// get the index of the the most recently focused window in this workspace which was not hidden
size_t SemmetyWorkspaceWrapper::getLastFocusedWindowIndex() {
	std::optional<size_t> minFocusIndex;
	size_t minIdx = 0;

	for (size_t idx = 0; idx < windows.size(); ++idx) {
		const auto window = windows[idx];
		if (isWindowHidden(window)) continue;

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

bool windowMatchesVisibility(PHLWINDOWREF window, SemmetyWindowVisibility mode) {
	if (!window) {
		return false;
	}

	switch (mode) {
	case SemmetyWindowVisibility::Either: return true;
	case SemmetyWindowVisibility::Visible: return !window->isHidden();
	case SemmetyWindowVisibility::Hidden: return window->isHidden();
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

	// auto numSteps = windows.size();
	// if (!params.startFromIndex.has_value()) {
	// 	numSteps -= 1;
	// }

	for (size_t i = 0; i < windows.size(); i++) {
		const auto window = windows[index];

		if (windowMatchesMode(window, windowMode) && windowMatchesVisibility(window, windowVisibility))
		{
			auto f = getFrameForWindow(window);
			if (f) {
				semmety_log(ERR, "next winodw already in frame");
			}
			semmety_log(ERR, "Found next window {} {}", window->fetchTitle(), window->isHidden());

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
	return !getFrameForWindow(window);
}

SP<SemmetyLeafFrame> SemmetyWorkspaceWrapper::getFocusedFrame() { return focused_frame; }

void SemmetyWorkspaceWrapper::setFocusedFrame(SP<SemmetyFrame> frame) {
	static int focusOrder = 0;

	if (!frame) {
		semmety_critical_error("Cannot set a null frame as focused");
	}

	if (focused_frame == frame) {
		focusWindow(focused_frame->getWindow());
		return;
	}

	focused_frame = frame->getLastFocussedLeaf();
	focused_frame->focusOrder = ++focusOrder;
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

bool isWindowFocussed(PHLWINDOWREF window) {
	return g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow == window;
}

void SemmetyWorkspaceWrapper::printDebug() {
	semmety_log(ERR, "DEBUG\n{}", root->print(*this));

	for (const auto& window: windows) {
		const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(window.get()));
		const auto focusString = isWindowFocussed(window) ? "f" : " ";
		const auto hiddenString = isWindowHidden(window) ? "m" : " ";
		const auto floatingString = isWindowFloating(window) ? "m" : " ";

		semmety_log(
		    ERR,
		    "{} {} {} {} {}",
		    ptrString,
		    focusString,
		    hiddenString,
		    floatingString,
		    window.lock()->m_szTitle
		);
	}

	semmety_log(ERR, "workspace id + name '{}' '{}'", workspace->m_szName, workspace->m_iID);
	semmety_log(ERR, "DEBUG END");
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
		     {"minimized", isWindowHidden(window)}}
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

	auto element = windows[index];
	windows.erase(windows.begin() + index);

	// Insert it at the computed final position.
	// (Using finalPos directly works: when the element is removed the indices shift exactly so
	// that inserting at position finalPos puts it in the correct spot in the final ordering.)
	windows.insert(windows.begin() + finalPos, element);
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
		if (isWindowHidden(window)) {
			errors.push_back("Invariant violation: Window in a frame is hidden.");
		}
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
