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

PHLWINDOWREF SemmetyWorkspaceWrapper::getNextMinimizedWindow() {
	return maybeAdvanceWindowIndex(true);
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
	// if index not on minimized window
	//   try to find minimized window
	//   if found, move index to minimized window
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		frameWithWindow->makeEmpty();
	}

	windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
	maybeAdvanceWindowIndex();
	// if index on removed window (ie, the removed window was minimized),
	// try to find minimized window,
	//   if found move index (you may not need to move).
	//   If not found, and still in range, leave it
	//   if not found and not in range, set to last window
}

PHLWINDOWREF SemmetyWorkspaceWrapper::maybeAdvanceWindowIndex(bool advance_past) {
	if (next_window_index >= windows.size()) {
		next_window_index = std::max<std::size_t>(0, windows.size() - 1);
	}

	const auto minimizedWindows = getMinimizedWindows();
	if (minimizedWindows.empty()) {
		return {};
	}

	size_t index = next_window_index;
	for (size_t i = next_window_index; i < windows.size(); i++) {
		const auto minimizedWindow =
		    std::find(minimizedWindows.begin(), minimizedWindows.end(), windows[i]);
		if (minimizedWindow != minimizedWindows.end()) {
			break;
		}

		index = (index + 1) % windows.size();
	}

	if (advance_past) {
		next_window_index = (index + 1) % windows.size();
	} else {
		next_window_index = index;
	}

	return windows[index];
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
	maybeAdvanceWindowIndex();
}

void SemmetyWorkspaceWrapper::printDebug() {
	semmety_log(ERR, "DEBUG\n{}", root->print(0, this));

	const auto minimizedWindows = getMinimizedWindows();

	for (auto it = windows.begin(); it != windows.end(); ++it) {
		const auto minimizedWindow = std::find(minimizedWindows.begin(), minimizedWindows.end(), *it);
		const auto distance = std::distance(windows.begin(), it);

		CWindow& a = *it->get();
		const auto ptrString = std::to_string(reinterpret_cast<uintptr_t>(&a));

		const auto focusString =
		    focused_frame->is_window() && focused_frame->as_window() == *it ? "f" : " ";
		const auto nextString = distance == next_window_index ? "i" : " ";
		const auto minimizedString = minimizedWindow != minimizedWindows.end() ? "m" : " ";

		semmety_log(
		    ERR,
		    "{} {} {} {} {}",
		    ptrString,
		    focusString,
		    nextString,
		    minimizedString,
		    it->lock()->m_szTitle
		);
	}

	semmety_log(ERR, "");

	// apply();
}

using json = nlohmann::json;

void SemmetyWorkspaceWrapper::apply() {
	auto& monitor = workspace->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;
	root->geometry = {pos, size};

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

		// if (!w->m_pXWaylandSurface->minimized) {
		// 	window->m_pXWaylandSurface->setMinimized(true);
		// }
	}

	// semmety_log(ERR, "calling IPC");
	const auto focused_window =
	    focused_frame->is_window() ? focused_frame->as_window() : CWeakPointer<CWindow>();

	json jsonWindows = json::array();
	for (const auto& window: windows) {
		const auto surface = window->m_pXWaylandSurface.lock();
		if (!surface) {
			continue;
		}

		const auto isFocused = window == focused_window;
		const auto minimized = std::find(minimizedWindows.begin(), minimizedWindows.end(), window)
		                    != minimizedWindows.end();

		jsonWindows.push_back(
		    {{"xID", surface->xID},
		     {"wlID", surface->wlID},
		     {"title", surface->state.title},
		     {"appid", surface->state.appid},
		     {"focused", isFocused},
		     {"minimized", minimized}}
		);
	}

	auto jsonString = jsonWindows.dump();
	auto escapedJsonString = std::string("\"") + jsonString + "\"";
	spawnRawProc("qs ipc call taskManager setWindows " + escapedJsonString, workspace.lock());
	// g_pAnimationManager->scheduleTick();
	//

	// SDispatchResult CKeybindManager::spawnRaw(std::string args);
}
