#include "SemmetyWorkspaceWrapper.hpp"
#include <algorithm>
#include <numeric>

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

#include "json.hpp"
#include "log.hpp"
#include "src/helpers/MiscFunctions.hpp"

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l): layout(l) {
	workspace = w;

	auto& monitor = w->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;
	auto frame = makeShared<SemmetyFrame>();
	frame->geometry = {pos, size};

	semmety_log(ERR, "init workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);
	semmety_log(ERR, "workspace has root frame: {}", frame->print());

	this->root = frame;
	this->focused_frame = frame;
}

uint64_t spawnRawProc(std::string args, PHLWORKSPACE pInitialWorkspace) {
	Debug::log(LOG, "Executing {}", args);

	// const auto HLENV = getHyprlandLaunchEnv(pInitialWorkspace);

	int socket[2];
	if (pipe(socket) != 0) {
		Debug::log(LOG, "Unable to create pipe for fork");
	}

	Hyprutils::OS::CFileDescriptor pipeSock[2] = {
	    Hyprutils::OS::CFileDescriptor {socket[0]},
	    Hyprutils::OS::CFileDescriptor {socket[1]}
	};

	pid_t child, grandchild;
	child = fork();
	if (child < 0) {
		Debug::log(LOG, "Fail to create the first fork");
		return 0;
	}
	if (child == 0) {
		// run in child
		g_pCompositor->restoreNofile();

		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, nullptr);

		grandchild = fork();
		if (grandchild == 0) {
			// run in grandchild
			// for (auto const& e: HLENV) {
			// 	setenv(e.first.c_str(), e.second.c_str(), 1);
			// }
			setenv("WAYLAND_DISPLAY", g_pCompositor->m_szWLDisplaySocket.c_str(), 1);
			execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
			// exit grandchild
			_exit(0);
		}
		write(pipeSock[1].get(), &grandchild, sizeof(grandchild));
		// exit child
		_exit(0);
	}
	// run in parent
	read(pipeSock[0].get(), &grandchild, sizeof(grandchild));
	// clear child and leave grandchild to init
	waitpid(child, nullptr, 0);
	if (grandchild < 0) {
		Debug::log(LOG, "Fail to create the second fork");
		return 0;
	}

	Debug::log(LOG, "Process Created with pid {}", grandchild);

	return grandchild;
}
SDispatchResult spawnRaw(std::string args) {
	const uint64_t PROC = spawnRawProc(args, nullptr);
	return {.success = PROC > 0, .error = std::format("Failed to start process {}", args)};
}

std::list<PHLWINDOWREF> SemmetyWorkspaceWrapper::getMinimizedWindows() const {
	std::list<PHLWINDOWREF> minimizedWindows;
	for (const auto& window: windows) {
		if (!getFrameForWindow(window)) {
			minimizedWindows.push_back(window);
		}
	}
	return minimizedWindows;
}

bool overlap(int start1, int end1, int start2, int end2) {
	const int dx = std::max(0, std::min(end1, end2) - std::max(start1, start2));
	return dx > 0;
}

SP<SemmetyFrame>
SemmetyWorkspaceWrapper::getNeighborByDirection(SP<SemmetyFrame> basis, Direction dir) {
	bool vertical;
	int sign;
	switch (dir) {
	case Direction::Up:
		vertical = true;
		sign = -1;
		break;
	case Direction::Down:
		vertical = true;
		sign = 1;
		break;
	case Direction::Left:
		vertical = false;
		sign = -1;
		break;
	case Direction::Right:
		vertical = false;
		sign = 1;
		break;
	default: return nullptr;
	}

	auto candidates = getLeafFrames();
	candidates.remove_if([&](const SP<SemmetyFrame>& tile) {
		return vertical ? tile->geometry.pos().y * sign <= basis->geometry.pos().y * sign
		                : tile->geometry.pos().x * sign <= basis->geometry.pos().x * sign;
	});

	candidates.remove_if([&](const SP<SemmetyFrame>& tile) {
		return vertical ? !overlap(
		                      basis->geometry.pos().x,
		                      basis->geometry.pos().x + basis->geometry.size().x,
		                      tile->geometry.pos().x,
		                      tile->geometry.pos().x + tile->geometry.size().x
		                  )
		                : !overlap(
		                      basis->geometry.pos().y,
		                      basis->geometry.pos().y + basis->geometry.size().y,
		                      tile->geometry.pos().y,
		                      tile->geometry.pos().y + tile->geometry.size().y
		                  );
	});

	if (candidates.empty()) return nullptr;

	auto min = sign
	         * std::accumulate(
	               candidates.begin(),
	               candidates.end(),
	               std::numeric_limits<int>::max(),
	               [&](int prevMin, const SP<SemmetyFrame>& tile) {
		               return vertical
		                        ? std::min(tile->geometry.pos().y * sign, static_cast<double>(prevMin))
		                        : std::min(tile->geometry.pos().x * sign, static_cast<double>(prevMin));
	               }
	         );

	std::list<SP<SemmetyFrame>> closest;
	std::copy_if(
	    candidates.begin(),
	    candidates.end(),
	    std::back_inserter(closest),
	    [&](const SP<SemmetyFrame>& tile) {
		    return vertical ? tile->geometry.pos().y == min : tile->geometry.pos().x == min;
	    }
	);

	auto maxFocusOrderLeaf = std::max_element(
	    closest.begin(),
	    closest.end(),
	    [](const SP<SemmetyFrame>& a, const SP<SemmetyFrame>& b) {
		    return a->focusOrder < b->focusOrder;
	    }
	);

	return maxFocusOrderLeaf != closest.end() ? *maxFocusOrderLeaf : nullptr;
}

std::list<SP<SemmetyFrame>> SemmetyWorkspaceWrapper::getLeafFrames() const {
	std::list<SP<SemmetyFrame>> leafFrames;
	std::list<SP<SemmetyFrame>> stack;
	stack.push_back(root);

	while (!stack.empty()) {
		auto current = stack.back();
		stack.pop_back();

		if (current->is_leaf()) {
			leafFrames.push_back(current);
		}

		if (current->is_parent()) {
			for (const auto& child: current->as_parent().children) {
				stack.push_back(child);
			}
		}
	}

	return leafFrames;
}

void SemmetyWorkspaceWrapper::rebalance() {
	// TODO: get minimized window and advance
	auto emptyFrames = getEmptyFrames();
	emptyFrames.sort([](const SP<SemmetyFrame>& a, const SP<SemmetyFrame>& b) {
		return a->geometry.size().x * a->geometry.size().y
		     > b->geometry.size().x * b->geometry.size().y;
	});

	for (auto& frame: emptyFrames) {
		auto window = getNextMinimizedWindow();
		if (window == nullptr) {
			semmety_log(ERR, "no more empty windows in balance?");
			break;
		}
		frame->makeWindow(window);
	}
	semmety_log(ERR, "no more frames to balance");
}

std::list<SP<SemmetyFrame>> SemmetyWorkspaceWrapper::getEmptyFrames() const {
	std::list<SP<SemmetyFrame>> emptyFrames;
	std::list<SP<SemmetyFrame>> stack;
	stack.push_back(root);

	while (!stack.empty()) {
		auto current = stack.back();
		stack.pop_back();

		if (current->is_empty()) {
			emptyFrames.push_back(current);
		}

		if (current->is_parent()) {
			for (const auto& child: current->as_parent().children) {
				stack.push_back(child);
			}
		}
	}

	return emptyFrames;
}

void SemmetyWorkspaceWrapper::addWindow(PHLWINDOWREF window) {
	windows.push_back(window);
	putWindowInFocusedFrame(window);
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		frameWithWindow->makeEmpty();
	}

	windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
}

// get the index of the the most recently focused window in this workspace which was not minimized
size_t SemmetyWorkspaceWrapper::getLastFocusedWindowIndex() {
	if (windows.empty()) {
		return 0;
	}

	const auto minimizedWindows = getMinimizedWindows();
	if (minimizedWindows.empty()) {
		return 0;
	}

	auto getFocusHistoryID = [](PHLWINDOW wnd) -> int {
		for (size_t i = 0; i < g_pCompositor->m_vWindowFocusHistory.size(); ++i) {
			if (g_pCompositor->m_vWindowFocusHistory[i].lock() == wnd) return i;
		}
		return -1;
	};

	auto minFocusIndex = std::numeric_limits<int>::max();
	auto minIndex = 0;

	for (size_t index = 0; index < windows.size(); index++) {
		const auto window = windows[index];
		if (std::find(minimizedWindows.begin(), minimizedWindows.end(), window)
		    != minimizedWindows.end())
		{
			continue;
		}

		int focusIndex = getFocusHistoryID(window.lock());
		if (focusIndex != -1 && focusIndex < minFocusIndex) {
			minFocusIndex = focusIndex;
			minIndex = index;
		}
	}

	return minIndex;
}

PHLWINDOWREF SemmetyWorkspaceWrapper::getNextMinimizedWindow() {
	if (windows.empty()) {
		return {};
	}

	const auto minimizedWindows = getMinimizedWindows();
	size_t index = getLastFocusedWindowIndex();

	index = (index + 1) % windows.size();
	for (size_t i = 0; i < windows.size(); i++) {
		const auto minimizedWindow =
		    std::find(minimizedWindows.begin(), minimizedWindows.end(), windows[index]);
		if (minimizedWindow != minimizedWindows.end()) {
			return *minimizedWindow;
		}

		index = (index + 1) % windows.size();
	}

	return {};
}

PHLWINDOWREF SemmetyWorkspaceWrapper::getPrevMinimizedWindow() {
	const auto minimizedWindows = getMinimizedWindows();
	size_t index = getLastFocusedWindowIndex();

	index = (index == 0) ? windows.size() - 1 : index - 1;
	for (size_t i = 0; i < windows.size(); i++) {
		const auto minimizedWindow =
		    std::find(minimizedWindows.begin(), minimizedWindows.end(), windows[index]);
		if (minimizedWindow != minimizedWindows.end()) {
			return *minimizedWindow;
		}

		index = (index == 0) ? windows.size() - 1 : index - 1;
	}

	return {};
}

SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFrameForWindow(PHLWINDOWREF window) const {
	std::list<SP<SemmetyFrame>> stack;
	stack.push_back(root);

	while (!stack.empty()) {
		auto current = stack.back();
		stack.pop_back();

		if (current->is_window() && current->as_window() == window) {
			return current;
		}

		if (current->is_parent()) {
			for (const auto& child: current->as_parent().children) {
				stack.push_back(child);
			}
		}
	}

	return nullptr; // Return null if no matching window frame is found
}

SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFocusedFrame() {
	if (!this->focused_frame) {
		semmety_critical_error("No active frame, were outputs added to the desktop?");
	}

	if (!this->focused_frame->is_leaf()) {
		semmety_critical_error("Active frame is not a leaf");
	}

	return this->focused_frame;
}

void SemmetyWorkspaceWrapper::setFocusedFrame(SP<SemmetyFrame> frame) {
	static int focusOrder = 0;
	if (!frame) {
		semmety_critical_error("Cannot set a null frame as focused");
	}

	frame->focusOrder = ++focusOrder;

	if (frame->is_leaf()) {
		this->focused_frame = frame;
		return;
	}

	const auto descendantLeafs = SemmetyFrame::getLeafDescendants(frame);
	auto maxFocusOrderLeaf = std::max_element(
	    descendantLeafs.begin(),
	    descendantLeafs.end(),
	    [](const SP<SemmetyFrame>& a, const SP<SemmetyFrame>& b) {
		    return a->focusOrder < b->focusOrder;
	    }
	);

	if (maxFocusOrderLeaf == descendantLeafs.end()) {
		semmety_critical_error("No non parent descendant leafs");
	}
	this->focused_frame = *maxFocusOrderLeaf;
}

void SemmetyWorkspaceWrapper::putWindowInFocusedFrame(PHLWINDOWREF window) {
	auto focusedFrame = getFocusedFrame();
	auto frameWithWindow = getFrameForWindow(window);

	if (focused_frame == frameWithWindow) {
		return;
	}

	if (frameWithWindow) {
		frameWithWindow->makeEmpty();
	}

	focusedFrame->makeWindow(window);
}

void SemmetyWorkspaceWrapper::printDebug() {
	semmety_log(ERR, "DEBUG\n{}", root->print(0, this));

	const auto minimizedWindows = getMinimizedWindows();

	for (auto it = windows.begin(); it != windows.end(); ++it) {
		const auto minimizedWindow = std::find(minimizedWindows.begin(), minimizedWindows.end(), *it);

		CWindow& a = *it->get();
		const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(&a));

		const auto focusString =
		    focused_frame->is_window() && focused_frame->as_window() == *it ? "f" : " ";
		const auto minimizedString = minimizedWindow != minimizedWindows.end() ? "m" : " ";

		semmety_log(ERR, "{} {} {} {}", ptrString, focusString, minimizedString, it->lock()->m_szTitle);
	}

	semmety_log(ERR, "workspace id + name '{}' '{}'", workspace->m_szName, workspace->m_iID);
	semmety_log(ERR, "DEBUG END");

	// apply();
}
#include <algorithm>
#include <iostream>
#include <string>
using json = nlohmann::json;

void escapeSingleQuotes(std::string& str) {
	size_t pos = 0;
	while ((pos = str.find('"', pos)) != std::string::npos) {
		str.replace(pos, 1, "'"); // Replace single quote with \'
		pos += 1;                 // Move past the inserted escape sequence
	}
}

std::string DoubleQuotes(std::string value) {
	std::string retval;
	for (auto ch: value) {
		if (ch == ',') {
			retval.push_back('&');
			continue;
		}
		retval.push_back(ch);
	}
	return retval;
}

void SemmetyWorkspaceWrapper::apply() {
	auto& monitor = workspace->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;
	root->geometry = {pos, size};

	semmety_log(ERR, "applying to workspace {}", workspace->m_szName);

	root->propagateGeometry();
	rebalance();
	if (this->focused_frame->is_window()) {
		const auto window = this->focused_frame->as_window().lock();
		if (window != g_pCompositor->m_pLastWindow) {
			semmety_log(ERR, "setting focus window");
			g_pCompositor->focusWindow(window);
		}
	} else {
		if (g_pCompositor->m_pLastWindow != nullptr) {
			g_pCompositor->focusWindow(nullptr);
		}
	}

	root->applyRecursive(workspace.lock());
	const auto minimizedWindows = getMinimizedWindows();

	for (const auto& window: minimizedWindows) {
		auto w = window.lock().get();
		if (w == nullptr) {
			// TODO
			continue;
		}

		if (!w->isHidden()) {
			window->setHidden(true);
		}
	}

	updateBar();

	// g_pAnimationManager->scheduleTick();
	//
}

void SemmetyWorkspaceWrapper::updateBar() {
	const auto minimizedWindows = getMinimizedWindows();
	const auto focused_window =
	    focused_frame->is_window() ? focused_frame->as_window() : CWeakPointer<CWindow>();

	if (windows.empty()) {
		spawnRawProc("qs ipc call taskManager setWindows empty", workspace.lock());
		semmety_log(ERR, "calling qs with no windows");
		return;
	}

	json jsonWindows = json::array();
	for (const auto& window: windows) {
		const auto isFocused = window == focused_window;
		const auto minimized = std::find(minimizedWindows.begin(), minimizedWindows.end(), window)
		                    != minimizedWindows.end();

		const auto address = std::format("{:x}", (uintptr_t) window.get());
		jsonWindows.push_back(
		    {{"address", address},
		     {"title", window->fetchTitle()},
		     {"appid", window->fetchClass()},
		     {"focused", isFocused},
		     {"minimized", minimized}}
		);
	}

	auto jsonString = jsonWindows.dump();
	auto escapedJsonString = std::string("\'") + DoubleQuotes(jsonString) + "\'";
	spawnRawProc("qs ipc call taskManager setWindows " + escapedJsonString, workspace.lock());

	semmety_log(ERR, "calling qs with \n#{}#\n#{}#", escapedJsonString, jsonString);
}

void SemmetyWorkspaceWrapper::setFocusShortcut(std::string shortcutKey) {
	if (!focused_frame->is_window()) {
		semmety_log(ERR, "Can't set focus short cut non-window");
		return;
	}

	focusShortcuts[shortcutKey] = focused_frame->as_window();
}

void SemmetyWorkspaceWrapper::activateFocusShortcut(std::string shortcutKey) {
	auto window = focusShortcuts.find(shortcutKey);

	if (window == focusShortcuts.end()) {
		semmety_log(ERR, "No shortcut found for key {}", shortcutKey);
		return;
	}

	if (!window->second.valid()) {
		semmety_log(ERR, "Shortcut window is no longer valid {}", shortcutKey);
		return;
	}

	if (window->second->m_pWorkspace != workspace) {
		semmety_log(ERR, "Window for shortcut is no longer on current worksapce");
		return;
	}

	semmety_log(
	    ERR,
	    "activating focus shortcut {} for window {}",
	    shortcutKey,
	    window->second->fetchTitle()
	);
	const auto frame = getFrameForWindow(window->second);
	if (frame) {
		setFocusedFrame(frame);
	} else {
		// TODO: check that the window is currently being tracked by the desktop?
		putWindowInFocusedFrame(window->second);
	}
}
