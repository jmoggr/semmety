#pragma once

#include <list>
#include <vector>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "SemmetyWorkspaceWrapper.hpp"
#include "json.hpp"
#include "log.hpp"
#include "utils.hpp"

void updateBar();
using json = nlohmann::json;

class SemmetyLayout: public Layout::ITiledAlgorithm {
public:
	SemmetyLayout();
	~SemmetyLayout() override;

	// Hyprland >= 0.55 instantiates one tiled-algorithm object per space (workspace), so several
	// SemmetyLayout instances can exist at once. semmety's data model is a single, global tree of
	// workspaces, so that state is shared between all instances (see the `inline static` members
	// below) and the global setup (event listeners, adopting existing windows) runs only once.
	// s_instances tracks live instances so g_SemmetyLayout never dangles when a space is destroyed.
	inline static std::vector<SemmetyLayout*> s_instances;
	inline static bool s_globalsInitialized = false;

	//
	// Layout::ITiledAlgorithm / Layout::IModeAlgorithm
	//

	void newTarget(SP<Layout::ITarget> target) override;
	void movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) override;
	void removeTarget(SP<Layout::ITarget> target) override;
	void resizeTarget(const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE) override;
	void recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN) override;
	void swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) override;
	void moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) override;
	Config::ErrorResult layoutMsg(const std::string_view& sv) override;
	std::optional<Vector2D> predictSizeForNewTarget() override;
	Layout::eFullscreenRequestResult requestFullscreen(const Layout::SFullscreenRequest& request) override;
	SP<Layout::ITarget> getNextCandidate(SP<Layout::ITarget> old) override;

	std::optional<std::string> layoutName() const override { return "semmety"; }

	//
	// Semmety
	//

	void onEnabled();
	void onDisabled();

	void moveWindowToWorkspace(std::string wsname);
	void recalculateWorkspace(const PHLWORKSPACE& workspace);
	SemmetyWorkspaceWrapper& getOrCreateWorkspaceWrapper(PHLWORKSPACE workspace);

	inline static std::list<SemmetyWorkspaceWrapper> workspaceWrappers;
	inline static bool updateBarOnNextTick = false;

	void activateWindow(PHLWINDOW window);
	void changeWindowOrder(bool prev);
	json getWorkspacesJson();

	std::string getDebugString();
	void testWorkspaceInvariance();

	template <typename Fn>
	auto entryWrapper(std::string name, Fn&& fn) {
		semmety_log(Log::ERR, "ENTER {} {}", name, entryCount);

		if (entryCount == 0) {
			testWorkspaceInvariance();
			debugStringOnEntry = getDebugString();
		}

		entryCount += 1;

		using ReturnType = std::invoke_result_t<Fn>;
		std::optional<ReturnType> result;
		std::optional<std::string> exitMessage;

		if constexpr (std::is_same_v<ReturnType, std::optional<std::string>>) {
			result = fn();
			if (result->has_value()) { exitMessage = result.value(); }
		} else if constexpr (std::is_void_v<ReturnType>) {
			fn();
		} else {
			result = fn();
		}

		if (entryCount == 1) {
			testWorkspaceInvariance();
			if (_shouldUpdateBar) {
				updateBar();
				_shouldUpdateBar = false;
			}
		}

		entryCount -= 1;

		if (exitMessage.has_value()) {
			semmety_log(Log::INFO, "EXIT {} -- {}", name, exitMessage.value());
		} else {
			semmety_log(Log::INFO, "EXIT {}", name);
		}

		if constexpr (!std::is_void_v<ReturnType>) { return *result; }
	}

	inline static bool _shouldUpdateBar = false;

	inline static int entryCount = 0;
	inline static std::string debugStringOnEntry = "";
};
