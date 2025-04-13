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

void SemmetyWorkspaceWrapper::rebalance() {
	auto emptyFrames = root->getEmptyFrames();

	// sort empty frames by size, largest first
	std::sort(emptyFrames.begin(), emptyFrames.end(), frameAreaGreater);

	for (auto& frame: emptyFrames) {
		auto window = getNextMinimizedWindow();
		if (window == nullptr) {
			break;
		}

		frame->setWindow(*this, window);
	}
}

void SemmetyWorkspaceWrapper::putWindowInFocussedFrame(PHLWINDOWREF window) {
	if (!window) {
		return;
	}

	const auto replacedWindow = focused_frame->replaceWindow(*this, window);
	focusWindow(window);
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
	putWindowInFocussedFrame(window);
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	auto it = std::find(windows.begin(), windows.end(), window);
	if (it == windows.end()) {
		semmety_critical_error("removeWindow called with window that is not in the workspace");
	}

	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		size_t index = std::distance(windows.begin(), it);
		auto nextWindow = getNextMinimizedWindow(index);
		const auto _removedWindow = frameWithWindow->replaceWindow(*this, nextWindow);
		if (frameWithWindow == focused_frame) {
			focusWindow(nextWindow);
		}
	}

	windows.erase(it);
}

// get the index of the the most recently focused window in this workspace which was not minimized
size_t SemmetyWorkspaceWrapper::getLastFocusedWindowIndex() {
	std::optional<size_t> minFocusIndex;
	size_t minIdx = 0;

	for (size_t idx = 0; idx < windows.size(); ++idx) {
		const auto window = windows[idx];
		if (isWindowMinimized(window)) continue;

		if (auto focus = getFocusHistoryIndex(window.lock())) {
			if (!minFocusIndex || *focus < *minFocusIndex) {
				minFocusIndex = *focus;
				minIdx = idx;
			}
		}
	}

	return minIdx;
}

PHLWINDOWREF SemmetyWorkspaceWrapper::getNextMinimizedWindow(std::optional<size_t> fromIndex) {
	size_t index = fromIndex.value_or(getLastFocusedWindowIndex());
	if (index >= windows.size()) {
		index = getLastFocusedWindowIndex();
	}

	for (size_t i = 0; i < windows.size(); i++) {
		const auto window = windows[index];
		if (isWindowMinimized(window)) {
			return window;
		}

		index = (index + 1) % windows.size();
	}

	return {};
}

PHLWINDOWREF SemmetyWorkspaceWrapper::getPrevMinimizedWindow() {
	size_t index = getLastFocusedWindowIndex();

	for (size_t i = 0; i < windows.size(); i++) {
		const auto window = windows[index];
		if (isWindowMinimized(window)) {
			return window;
		}

		index = (index == 0) ? windows.size() - 1 : index - 1;
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

bool SemmetyWorkspaceWrapper::isWindowMinimized(PHLWINDOWREF window) const {
	return !getFrameForWindow(window);
}

bool SemmetyWorkspaceWrapper::isWindowFocussed(PHLWINDOWREF window) const {
	const auto focusedWindow = focused_frame->getWindow();
	return focusedWindow && focusedWindow == window;
}

SP<SemmetyLeafFrame> SemmetyWorkspaceWrapper::getFocusedFrame() { return focused_frame; }

void SemmetyWorkspaceWrapper::setFocusedFrame(SP<SemmetyFrame> frame) {
	static int focusOrder = 0;

	if (!frame) {
		semmety_critical_error("Cannot set a null frame as focused");
	}

	if (focused_frame == frame) {
		return;
	}

	focused_frame = frame->getLastFocussedLeaf();
	focused_frame->focusOrder = ++focusOrder;
	focusWindow(focused_frame->getWindow());
}

void SemmetyWorkspaceWrapper::activateWindow(PHLWINDOWREF window) {
	auto frameWithWindow = getFrameForWindow(window);

	if (frameWithWindow) {
		setFocusedFrame(frameWithWindow);
		return;
	}

	// TODO: if window is not in workspace?
	putWindowInFocussedFrame(window);
}

void SemmetyWorkspaceWrapper::printDebug() {
	semmety_log(ERR, "DEBUG\n{}", root->print(*this));

	for (const auto& window: windows) {
		const auto isMinimized = isWindowMinimized(window);
		const auto isFocused = isWindowFocussed(window);

		const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(window.get()));
		const auto focusString = isFocused ? "f" : " ";
		const auto minimizedString = isMinimized ? "m" : " ";

		semmety_log(
		    ERR,
		    "{} {} {} {}",
		    ptrString,
		    focusString,
		    minimizedString,
		    window.lock()->m_szTitle
		);
	}

	semmety_log(ERR, "workspace id + name '{}' '{}'", workspace->m_szName, workspace->m_iID);
	semmety_log(ERR, "DEBUG END");
}

json SemmetyWorkspaceWrapper::getWorkspaceWindowsJson() const {
	json jsonWindows = json::array();
	for (const auto& window: windows) {
		const auto isFocused = isWindowFocussed(window);
		const auto isMinimized = isWindowMinimized(window);

		const auto address = std::format("{:x}", (uintptr_t) window.get());
		jsonWindows.push_back(
		    {{"address", address},
		     {"urgent", window->m_bIsUrgent},
		     {"title", window->fetchTitle()},
		     {"appid", window->fetchClass()},
		     {"focused", isFocused},
		     {"minimized", isMinimized}}
		);
	}

	return jsonWindows;
}

void SemmetyWorkspaceWrapper::changeWindowOrder(bool prev) {
	if (windows.size() < 2) {
		return;
	}

	// TODO: is this the correct way of getting the last focused window?
	if (!g_pCompositor->m_pLastWindow) {
		return;
	}

	auto focusedWindow = g_pCompositor->m_pLastWindow.lock();
	auto it = std::find(windows.begin(), windows.end(), focusedWindow);
	if (it == windows.end()) {
		return;
	}

	size_t index = std::distance(windows.begin(), it);
	int offset = prev ? -1 : 1;
	size_t newIndex = getWrappedOffsetIndex3(index, offset, windows.size());

	std::swap(windows[index], windows[newIndex]);
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
		if (isWindowMinimized(window)) {
			errors.push_back("Invariant violation: Window in a frame is minimized/hidden.");
		}
	}

	// 6. There should be no empty leaf frames if there are minimized windows.
	bool hasMinimizedWindow = false;
	for (const auto& w: windows) {
		if (isWindowMinimized(w)) {
			hasMinimizedWindow = true;
			break;
		}
	}

	if (hasMinimizedWindow) {
		for (const auto& frame: allFrames) {
			if (!frame->isLeaf()) {
				continue;
			}

			auto leafFrame = frame->asLeaf();
			if (leafFrame->isEmpty()) {
				errors.push_back("Invariant violation: An empty leaf frame exists despite the presence "
				                 "of minimized windows.");
			}
		}
	}

	// 7. Windows not assigned to any frame should be hidden/minimized.
	// For every window in windows that does not appear in a frame, check that it is
	// minimized.
	for (const auto& w: windows) {
		if (isWindowMinimized(w) && !w->isHidden()) {
			errors.push_back(std::format(
			    "Invariant violation: Window not assigned to any frame is not hidden. {}",
			    w->fetchTitle()
			));
		}
	}

	return errors;
}
