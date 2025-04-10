#include "SemmetyLayout.hpp"

#include "SemmetyWorkspaceWrapper.hpp"
#include "log.hpp"
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
	semmety_log(ERR, "in getWorkspacesJson");
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
	semmety_log(ERR, "ENTER activateWindow");
	auto layout = g_SemmetyLayout.get();
	auto ww = layout->getOrCreateWorkspaceWrapper(window->m_pWorkspace);

	ww.activateWindow(window);

	updateBar();
	g_pAnimationManager->scheduleTick();
	semmety_log(ERR, "EXIT activateWindow");
}

void SemmetyLayout::moveWindowToWorkspace(std::string wsname) {
	auto focused_window = g_pCompositor->m_pLastWindow.lock();
	if (!focused_window) {
		semmety_log(ERR, "no focused window {}", wsname);
		return;
	}

	const auto sourceWorkspace = focused_window->m_pWorkspace;
	if (!sourceWorkspace) {
		semmety_log(ERR, "no source workspace");
		return;
	}

	auto target = getWorkspaceIDNameFromString(wsname);

	if (target.id == WORKSPACE_INVALID) {
		semmety_log(ERR, "moveNodeToWorkspace called with invalid workspace {}", wsname);
		return;
	}

	auto targetWorkspace = g_pCompositor->getWorkspaceByID(target.id);
	if (!targetWorkspace) {
		semmety_log(LOG, "creating target workspace {} for node move", target.id);

		targetWorkspace =
		    g_pCompositor->createNewWorkspace(target.id, sourceWorkspace->monitorID(), target.name);
		if (!targetWorkspace) {
			semmety_log(ERR, "could not find target workspace '{}', '{}'", wsname, target.id);
			return;
		}
	}

	if (sourceWorkspace == targetWorkspace) {
		semmety_log(ERR, "source and target workspaces are the same");
		return;
	}

	auto sourceWrapper = getOrCreateWorkspaceWrapper(sourceWorkspace);
	auto targetWrapper = getOrCreateWorkspaceWrapper(targetWorkspace);

	sourceWrapper.removeWindow(focused_window);
	// onWindowCreatedTiling is called when the new window is put on the new monitor

	g_pHyprRenderer->damageWindow(focused_window);
	g_pCompositor->moveWindowToWorkspaceSafe(focused_window, targetWorkspace);
	focused_window->updateToplevel();
	focused_window->updateDynamicRules();
	focused_window->uncacheWindowDecos();

	updateBar();
}
