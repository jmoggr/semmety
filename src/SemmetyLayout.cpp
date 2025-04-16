#include "SemmetyLayout.hpp"
#include <optional>

#include "SemmetyWorkspaceWrapper.hpp"
#include "log.hpp"
#include "src/globals.hpp"
#include "utils.hpp"

SemmetyWorkspaceWrapper& SemmetyLayout::getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace) {
	if (workspace == nullptr) {
		semmety_critical_error("Tring to get or create a workspace wrapper with an invalid workspace");
	}

	for (auto& wrapper: this->workspaceWrappers) {
		if (wrapper.workspace.get() == &*workspace) {
			return wrapper;
		}
	}

	semmety_log(ERR, "Creating new workspace wrapper for workspace {}", workspace->m_iID);
	auto ww = SemmetyWorkspaceWrapper(workspace, *this);

	this->workspaceWrappers.emplace_back(ww);
	return this->workspaceWrappers.back();
}

json SemmetyLayout::getWorkspacesJson() {
	const auto ws = workspace_for_action();

	json jsonWorkspaces = json::array();
	// TODO: get max workspace ID iterate to that if it's large than 8
	for (int workspaceIndex = 0; workspaceIndex < 8; workspaceIndex++) {
		auto it = std::find_if(
		    workspaceWrappers.begin(),
		    workspaceWrappers.end(),
		    [workspaceIndex](const auto& workspaceWrapper) {
			    return workspaceWrapper.workspace != nullptr
			        && workspaceWrapper.workspace->m_iID == workspaceIndex + 1;
		    }
		);

		if (it == workspaceWrappers.end()) {
			jsonWorkspaces.push_back(
			    {{"id", workspaceIndex + 1},
			     {"numWindows", 0},
			     {"name", ""},
			     {"focused", false},
			     {"urgent", false}}
			);

			continue;
		}

		jsonWorkspaces.push_back(
		    {{"id", workspaceIndex + 1},
		     {"numWindows", it->windows.size()},
		     {"name", it->workspace->m_szName},
		     {"urgent", it->workspace->hasUrgentWindow()},
		     {"focused", &(*it) == &(*ws)}}
		);
	}

	return jsonWorkspaces;
}

void SemmetyLayout::activateWindow(PHLWINDOW window) {
	if (entryCount > 0) {
		return;
	}

	entryWrapper("activateWindow", [&]() -> std::optional<std::string> {
		auto layout = g_SemmetyLayout.get();
		auto ww = layout->getOrCreateWorkspaceWrapper(window->m_pWorkspace);

		ww.activateWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::moveWindowToWorkspace(std::string wsname) {
	entryWrapper("moveWindowToWorkspace", [&]() -> std::optional<std::string> {
		// TODO: follow?
		auto focused_window = g_pCompositor->m_pLastWindow.lock();
		if (!focused_window) {
			return format("no focused window {}", wsname);
		}

		const auto sourceWorkspace = focused_window->m_pWorkspace;
		if (!sourceWorkspace) {
			return "no source workspace";
		}

		auto target = getWorkspaceIDNameFromString(wsname);
		if (target.id == WORKSPACE_INVALID) {
			return format("moveNodeToWorkspace called with invalid workspace {}", wsname);
		}

		auto targetWorkspace = g_pCompositor->getWorkspaceByID(target.id);
		if (!targetWorkspace) {
			semmety_log(LOG, "creating target workspace {} for node move", target.id);

			targetWorkspace =
			    g_pCompositor->createNewWorkspace(target.id, sourceWorkspace->monitorID(), target.name);
			if (!targetWorkspace) {
				return format("could not find target workspace '{}', '{}'", wsname, target.id);
			}
		}

		if (sourceWorkspace == targetWorkspace) {
			return "source and target workspaces are the same";
		}

		auto sourceWrapper = getOrCreateWorkspaceWrapper(sourceWorkspace);
		auto targetWrapper = getOrCreateWorkspaceWrapper(targetWorkspace);

		// onWindowCreatedTiling is called when the new window is put in the target workspace

		if (focused_window->m_bIsFloating) {
			g_SemmetyLayout->onWindowRemovedFloating(focused_window);
		}
		g_pCompositor->moveWindowToWorkspaceSafe(focused_window, targetWorkspace);
		sourceWrapper.removeWindow(focused_window);

		if (focused_window->m_bIsFloating) {
			g_SemmetyLayout->onWindowCreatedFloating(focused_window);
		}

		// focused_window->updateToplevel();
		// focused_window->updateDynamicRules();
		// focused_window->uncacheWindowDecos();

		g_pHyprRenderer->damageWindow(focused_window);

		shouldUpdateBar();
		return std::nullopt;
	});
}

void SemmetyLayout::testWorkspaceInvariance() {
	for (auto& ws: workspaceWrappers) {
		auto errors = ws.testInvariants();
		if (errors.empty()) {
			continue;
		}

		semmety_log(ERR, "Found {} errors", errors.size());
		for (auto& error: errors) {
			semmety_log(ERR, "{}", error);
		}

		ws.printDebug();
		semmety_critical_error("invariant failed");
	}
}

std::string SemmetyLayout::getDebugString() {
	std::string out;
	for (auto it = workspaceWrappers.begin(); it != workspaceWrappers.end(); ++it) {
		out += it->getDebugString();
		if (std::next(it) != workspaceWrappers.end()) {
			out += "\n";
		}
	}
	return out;
}
