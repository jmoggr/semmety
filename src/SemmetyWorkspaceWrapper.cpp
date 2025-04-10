#include "SemmetyWorkspaceWrapper.hpp"
#include <algorithm>
#include <string>

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

	// TODO: apply stuff to frame like in replaceNode
	frame->geometry = {pos, size};
	semmety_log(ERR, "init workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);
	semmety_log(ERR, "workspace has root frame: {}", frame->print(*this));
}

void SemmetyWorkspaceWrapper::rebalance() {
	auto emptyFrames = root->getEmptyFrames();

	// sort empty frames by size, largest first
	std::sort(
	    emptyFrames.begin(),
	    emptyFrames.end(),
	    [](const SP<SemmetyLeafFrame>& a, const SP<SemmetyLeafFrame>& b) {
		    return a->geometry.size().x * a->geometry.size().y
		         > b->geometry.size().x * b->geometry.size().y;
	    }
	);

	for (auto& frame: emptyFrames) {
		auto window = getNextMinimizedWindow();
		if (window == nullptr) {
			break;
		}

		frame->setWindow(*this, window);
	}
}

void SemmetyWorkspaceWrapper::addWindow(PHLWINDOWREF window) {
	windows.push_back(window);
	focused_frame->setWindow(*this, window);
	focusWindow(window);
	rebalance();
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
		frameWithWindow->setWindow(*this, nextWindow);
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
	focused_frame->setWindow(*this, window);
	focusWindow(focused_frame->getWindow());
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
