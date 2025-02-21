// clang-format off
#include <re2/re2.h>
// clang-format on

#include "dispatchers.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/string/String.hpp>

#include "SemmetyFrame.hpp"
#include "dispatchers.hpp"
#include "globals.hpp"

SemmetyWorkspaceWrapper* workspace_for_action(bool allow_fullscreen = true) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return nullptr;
	}

	if (g_pLayoutManager->getCurrentLayout() != layout) {
		return nullptr;
	}

	auto workspace = g_pCompositor->m_pLastMonitor->activeSpecialWorkspace;
	if (!valid(workspace)) {
		workspace = g_pCompositor->m_pLastMonitor->activeWorkspace;
	}

	if (!valid(workspace)) {
		return nullptr;
	}
	if (!allow_fullscreen && workspace->m_bHasFullscreenWindow) {
		return nullptr;
	}

	semmety_log(ERR, "getting workspace for action {}", workspace->m_iID);

	return &layout->getOrCreateWorkspaceWrapper(workspace);
}

SDispatchResult dispatch_set_window_order(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto args = CVarList(arg);
	std::vector<PHLWINDOWREF> newOrder;
	std::set<PHLWINDOWREF> foundWindows;

	for (const auto& regex: args) {
		const auto window = g_pCompositor->getWindowByRegex(regex);
		if (window) {
			newOrder.push_back(window);
			foundWindows.insert(window);
		}
	}

	for (const auto& window: workspace_wrapper->windows) {
		if (foundWindows.find(window) == foundWindows.end()) {
			newOrder.push_back(window);
		}
	}

	workspace_wrapper->windows = newOrder;
	workspace_wrapper->apply();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

std::optional<Direction> parseDirectionArg(std::string arg) {
	if (arg == "l" || arg == "left") return Direction::Left;
	else if (arg == "r" || arg == "right") return Direction::Right;
	else if (arg == "u" || arg == "up") return Direction::Up;
	else if (arg == "d" || arg == "down") return Direction::Down;
	else return {};
}
SDispatchResult dispatch_debug_v2(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr)
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};

	if (!valid(workspace_wrapper->workspace.lock())) {
		semmety_log(ERR, "workspace is not valid");
	}

	auto p = workspace_wrapper->workspace.lock().get();
	if (p == nullptr) {
		semmety_log(ERR, "workspace is null");
	}

	workspace_wrapper->printDebug();

	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult split(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto focused_frame = workspace_wrapper->getFocusedFrame();
	if (!focused_frame->is_leaf()) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}
	semmety_log(ERR, "split before \n{}", focused_frame->print(0, workspace_wrapper));

	const auto childA = makeShared<SemmetyFrame>(focused_frame);
	const auto childB = makeShared<SemmetyFrame>(SemmetyFrame());
	focused_frame->makeParent(childA, childB);
	for (auto& child: focused_frame->as_parent().children) {
		child->parent = focused_frame;
	}

	const auto frameSize = focused_frame->geometry.size();
	if (frameSize.x > frameSize.y) {
		focused_frame->split_direction = SemmetySplitDirection::SplitV;
	} else {
		focused_frame->split_direction = SemmetySplitDirection::SplitH;
	}

	semmety_log(ERR, "split after \n{}", focused_frame->print(0, workspace_wrapper));
	workspace_wrapper->setFocusedFrame(focused_frame);
	workspace_wrapper->apply();

	g_pAnimationManager->scheduleTick();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_remove(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}
	auto focused_frame = workspace_wrapper->getFocusedFrame();

	if (!focused_frame->is_leaf()) {
		semmety_log(ERR, "Can only remove leaf frames");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto parent = focused_frame->parent.lock();
	if (!parent) {
		semmety_log(ERR, "Frame has no parent, cannot remove the root frame!");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto& siblings = parent->as_parent().children;
	auto remaining_sibling = std::find_if(
	    siblings.begin(),
	    siblings.end(),
	    [&focused_frame](const SP<SemmetyFrame>& sibling) { return sibling != focused_frame; }
	);

	if (remaining_sibling != siblings.end()) {
		parent->makeOther(*remaining_sibling);

		if (parent->is_parent()) {
			for (auto& child: parent->as_parent().children) {
				child->parent = parent;
			}
		}
	}

	workspace_wrapper->setFocusedFrame(parent);
	workspace_wrapper->apply();
	g_pAnimationManager->scheduleTick();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult cycle_hidden(std::string arg) {
	auto* workspace_wrapper = workspace_for_action(true);
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}
	auto focused_frame = workspace_wrapper->getFocusedFrame();

	if (!focused_frame->is_leaf()) {
		semmety_log(ERR, "Can only cycle leaf frames");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	const auto window = workspace_wrapper->getNextMinimizedWindow();
	if (!window) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	focused_frame->makeWindow(window);
	workspace_wrapper->apply();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

std::string direction_to_string(Direction dir) {
	switch (dir) {
	case Direction::Up: return "Up";
	case Direction::Down: return "Down";
	case Direction::Left: return "Left";
	case Direction::Right: return "Right";
	default: return "Unknown";
	}
}

SDispatchResult dispatch_focus(std::string value) {
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto args = CVarList(value);

	const auto direction = parseDirectionArg(args[0]);
	if (!direction.has_value()) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	const auto neighbor = workspace_wrapper->getNeighborByDirection(
	    workspace_wrapper->focused_frame,
	    direction.value()
	);

	if (neighbor == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	semmety_log(ERR, "focus {}", direction_to_string(direction.value()));
	workspace_wrapper->setFocusedFrame(neighbor);
	workspace_wrapper->apply();
	g_pAnimationManager->scheduleTick();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_swap(std::string value) {
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto args = CVarList(value);

	const auto direction = parseDirectionArg(args[0]);
	if (!direction.has_value()) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	const auto neighbor = workspace_wrapper->getNeighborByDirection(
	    workspace_wrapper->focused_frame,
	    direction.value()
	);

	if (neighbor == nullptr) {
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	auto focused_frame = workspace_wrapper->getFocusedFrame();

	neighbor->swapData(focused_frame);

	workspace_wrapper->setFocusedFrame(neighbor);
	workspace_wrapper->apply();
	g_pAnimationManager->scheduleTick();
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_move_to_workspace(std::string value) {
	auto args = CVarList(value);

	auto workspace = args[0];
	if (workspace == "") {
		semmety_log(ERR, "no argument provided");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	g_SemmetyLayout->moveWindowToWorkspace(workspace);
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_activate(std::string value) {
	auto args = CVarList(value);

	auto regexp = args[0];
	if (regexp == "") {
		semmety_log(ERR, "no argument provided");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	const auto window = g_pCompositor->getWindowByRegex(regexp);

	if (!window) {
		semmety_log(ERR, "no window found to activate");
		return {};
	}

	g_SemmetyLayout->activateWindow(window);
	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_set_focus_shortcut(std::string value) {
	semmety_log(ERR, "setFocusShortcut {}", value);
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		semmety_log(ERR, "no workspace");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}
	auto args = CVarList(value);

	auto shortcut_key = args[0];
	if (shortcut_key == "") {
		semmety_log(ERR, "no argument provided");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	workspace_wrapper->setFocusShortcut(shortcut_key);

	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

SDispatchResult dispatch_activate_focus_shortcut(std::string value) {
	semmety_log(ERR, "activteFocusShortcut {}", value);
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		semmety_log(ERR, "no workspace");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}
	auto args = CVarList(value);

	auto shortcut_key = args[0];
	if (shortcut_key == "") {
		semmety_log(ERR, "no argument provided");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	workspace_wrapper->activateFocusShortcut(shortcut_key);
	workspace_wrapper->apply();
	g_pAnimationManager->scheduleTick();

	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}
SDispatchResult dispatch_update_bar(std::string value) {
	auto workspace_wrapper = workspace_for_action();
	if (workspace_wrapper == nullptr) {
		semmety_log(ERR, "no workspace");
		return SDispatchResult {.passEvent = false, .success = true, .error = ""};
	}

	workspace_wrapper->updateBar();

	return SDispatchResult {.passEvent = false, .success = true, .error = ""};
}

void registerDispatchers() {
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:debug", dispatch_debug_v2);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:cycle_hidden", cycle_hidden);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:split", split);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:remove", dispatch_remove);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:focus", dispatch_focus);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:swap", dispatch_swap);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:movetoworkspace", dispatch_move_to_workspace);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:activate", dispatch_activate);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:setfocusshortcut", dispatch_set_focus_shortcut);
	HyprlandAPI::addDispatcherV2(
	    PHANDLE,
	    "semmety:activatefocusshortcut",
	    dispatch_activate_focus_shortcut
	);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:updatebar", dispatch_update_bar);
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:setwindoworder", dispatch_set_window_order);
}
