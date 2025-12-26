// clang-format off
#include <hyprutils/string/VarList.hpp>
#include <re2/re2.h>
// clang-format on

#include "dispatchers.hpp"
#include <optional>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
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
#include "SemmetyFrameUtils.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "dispatchers.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "utils.hpp"

std::optional<std::string>
dispatchDebug(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame>, CVarList) {
	if (!valid(workspace.workspace.lock())) {
		return "Workspace is not valid";
	}

	if (workspace.workspace.lock() == nullptr) {
		return "Workspace is null";
	}

	workspace.printDebug();
	return std::nullopt;
}

std::optional<std::string>
dispatchSplit(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame> focussedFrame, CVarList) {
	auto firstChild = focussedFrame;

	auto next = workspace.getNextWindow(nextTiledWindowParams);
	if (next) {
		semmety_log(
		    ERR,
		    "Split found next window {} {}",
		    next->fetchTitle(),
		    workspace.isWindowInFrame(next)
		);
	}
	auto secondChild = SemmetyLeafFrame::create({});
	auto newSplit = SemmetySplitFrame::create(firstChild, secondChild, focussedFrame->geometry);

	replaceNode(focussedFrame, newSplit, workspace);
	workspace.setFocusedFrame(secondChild);

	return std::nullopt;
}

std::optional<std::string> dispatchRemove(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> focussedFrame,
    CVarList args
) {
	auto parent = findParent(focussedFrame, workspace);
	if (!parent) {
		return "Frame has no parent, cannot remove the root frame!";
	}

	if (args[0] == "sibling") {
		replaceNode(parent, focussedFrame, workspace);
	} else {
		auto remainingSibling = parent->getOtherChild(focussedFrame);
		replaceNode(parent, remainingSibling, workspace);
		workspace.setFocusedFrame(remainingSibling);
	}

	return std::nullopt;
}

std::optional<std::string> dispatchCycle(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> focussedFrame,
    CVarList args
) {
	auto params = nextTiledWindowParams;
	PHLWINDOWREF window;
	if (args[0] == "prev") {
		params.backward = true;
		window = workspace.getNextWindow(params);
	} else {
		window = workspace.getNextWindow(params);
	}

	if (!window) {
		return std::nullopt;
	}

	workspace.putWindowInFocussedFrame(window);

	return std::nullopt;
}

std::optional<std::string> dispatchFocus(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> focussedFrame,
    CVarList args
) {
	const auto direction = directionFromString(args[0]);
	if (!direction.has_value()) {
		// TODO: return error
		return std::nullopt;
	}

	const auto neighbor = getNeighborByDirection(workspace, focussedFrame, direction.value());
	if (!neighbor) {
		return std::nullopt;
	}

	workspace.setFocusedFrame(neighbor);
	return std::nullopt;
}

std::optional<std::string> dispatchSwap(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> focussedFrame,
    CVarList args
) {
	const auto direction = directionFromString(args[0]);
	if (!direction.has_value()) {
		return format("Failed to pares direction from argument string '{}'", args[0]);
	}

	const auto neighbor = getNeighborByDirection(workspace, focussedFrame, direction.value());
	if (!neighbor) {
		return std::nullopt;
	}

	focussedFrame->swapContents(workspace, neighbor);
	workspace.setFocusedFrame(neighbor);

	return std::nullopt;
}

std::optional<std::string>
dispatchMoveToWorkspace(SemmetyWorkspaceWrapper&, SP<SemmetyLeafFrame>, CVarList args) {
	if (args.size() == 0 || args[0].empty()) {
		return "No workspace name provided";
	}

	g_SemmetyLayout->moveWindowToWorkspace(args[0]);
	return std::nullopt;
}

std::optional<std::string>
dispatchActivate(SemmetyWorkspaceWrapper&, SP<SemmetyLeafFrame>, CVarList args) {
	if (args.size() == 0 || args[0].empty()) {
		return "No regex provided for activation";
	}

	const auto window = g_pCompositor->getWindowByRegex(args[0]);
	if (!window) {
		return "No window matched the regex";
	}

	auto workspace = workspace_for_window(window);
	workspace->activateWindow(window);

	return std::nullopt;
}

std::optional<std::string>
dispatchChangeWindowOrder(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame>, CVarList args) {
	if (args.size() == 0) {
		return "Expected 'prev' or 'next' as argument";
	}

	const bool prev = args[0] == "prev";
	workspace.changeWindowOrder(prev);

	return std::nullopt;
}

std::optional<std::string>
dispatchUpdateBar(SemmetyWorkspaceWrapper&, SP<SemmetyLeafFrame>, CVarList) {
	return std::nullopt;
}

using DispatchFunc = std::function<
    std::optional<std::string>(SemmetyWorkspaceWrapper&, SP<SemmetyLeafFrame>, CVarList)>;

SDispatchResult dispatchWrapper(const std::string& arg, const DispatchFunc& action) {
	// TODO? Check that the layout pointer is valid?
	auto* workspace = workspace_for_action(true);
	if (!workspace) {
		return {.passEvent = false, .success = false, .error = ""};
	}

	auto args = CVarList(arg);
	auto focused = workspace->getFocusedFrame();
	if (auto err = action(*workspace, focused, args)) {
		return {.passEvent = false, .success = false, .error = *err};
	}

	shouldUpdateBar();
	g_pAnimationManager->scheduleTick();
	return {.passEvent = false, .success = true, .error = ""};
}

void registerSemmetyDispatcher(const std::string& name, const DispatchFunc& func) {
	HyprlandAPI::addDispatcherV2(PHANDLE, "semmety:" + name, [func, name](const std::string& arg) {
		return g_SemmetyLayout->entryWrapper("semmety:" + name, [&]() {
			return dispatchWrapper(arg, func);
		});
	});
}

void registerDispatchers() {
	registerSemmetyDispatcher("split", dispatchSplit);
	registerSemmetyDispatcher("remove", dispatchRemove);
	registerSemmetyDispatcher("cycle", dispatchCycle);
	registerSemmetyDispatcher("focus", dispatchFocus);
	registerSemmetyDispatcher("swap", dispatchSwap);
	registerSemmetyDispatcher("movetoworkspace", dispatchMoveToWorkspace);
	registerSemmetyDispatcher("activate", dispatchActivate);
	registerSemmetyDispatcher("changewindoworder", dispatchChangeWindowOrder);
	registerSemmetyDispatcher("updatebar", dispatchUpdateBar);
	registerSemmetyDispatcher("debug", dispatchDebug);
}
