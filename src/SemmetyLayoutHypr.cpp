#include <optional>
#include <type_traits>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/managers/EventManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>

#include "SemmetyFrame.hpp"
#include "SemmetyFrameUtils.hpp"
#include "SemmetyLayout.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "utils.hpp"

CHyprSignalListener renderListener;
CHyprSignalListener tickListener;
CHyprSignalListener workspaceListener;
CHyprSignalListener urgentListener;
CHyprSignalListener windowTitleListener;
CHyprSignalListener focusListener;

static void renderHook(eRenderStage render_stage) {
	if (!g_semmetyReady) { return; }

	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");
	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");
	static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

	auto monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
	if (monitor == nullptr) { return; }

	if (monitor->m_activeWorkspace == nullptr) { return; }

	auto layout = g_SemmetyLayout;
	if (layout == nullptr) { return; }
	auto ww = layout->getOrCreateWorkspaceWrapper(monitor->m_activeWorkspace);
	auto emptyFrames = ww.getRoot()->getEmptyFrames();

	switch (render_stage) {
	case RENDER_PRE_WINDOWS:
		for (const auto& frame: emptyFrames) {
			CBorderPassElement::SBorderData borderData;
			borderData.box = frame->getEmptyFrameBox(*monitor);
			borderData.borderSize = *PBORDERSIZE;
			borderData.roundingPower = *PROUNDINGPOWER;
			borderData.round = *PROUNDING;

			auto grad = frame->m_cRealBorderColor;

			const bool ANIMATED = frame->m_fBorderFadeAnimationProgress->isBeingAnimated();
			if (ANIMATED) {
				borderData.hasGrad2 = true;
				borderData.grad1 = frame->m_cRealBorderColorPrevious;
				borderData.grad2 = grad;
				borderData.lerp = frame->m_fBorderFadeAnimationProgress->value();
			} else {
				borderData.grad1 = grad;
			}

			g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(borderData));
		}

		break;
	default: break;
	}
}

static void tickHook() {
	// The tick event only fires once Hyprland is running its Wayland event loop, so by now it is
	// safe to build animated variables (and thus frames).
	g_semmetyReady = true;

	auto layout = g_SemmetyLayout;
	if (layout == nullptr) { return; }

	if (layout->updateBarOnNextTick) {
		updateBar();
		layout->updateBarOnNextTick = false;
	}

	// Safety net: re-apply our layout on the tick after a window was added, in case the window
	// wasn't mapped yet at newTarget (so applyRecursive couldn't position it then). applyRecursive
	// now positions via the layout target, so this just re-asserts the geometry without warping -
	// it won't disturb an in-progress window-open animation.
	if (SemmetyLayout::s_reflowPending) {
		SemmetyLayout::s_reflowPending = false;
		for (auto& ww: SemmetyLayout::workspaceWrappers) {
			if (ww.getRoot()) { ww.getRoot()->applyRecursive(ww, std::nullopt, std::nullopt); }
		}
	}

	for (const auto& monitor: g_pCompositor->m_monitors) {
		if (monitor->m_activeWorkspace == nullptr) { continue; }

		const auto activeWorkspace = monitor->m_activeWorkspace;
		if (activeWorkspace == nullptr) { continue; }

		const auto ww = layout->getOrCreateWorkspaceWrapper(monitor->m_activeWorkspace);
		auto emptyFrames = ww.getRoot()->getEmptyFrames();

		for (const auto& frame: emptyFrames) { frame->damageEmptyFrameBox(*monitor); }
	}
}

SemmetyLayout::SemmetyLayout() {
	s_instances.push_back(this);
	if (g_SemmetyLayout == nullptr) { g_SemmetyLayout = this; }
}

SemmetyLayout::~SemmetyLayout() {
	std::erase(s_instances, this);

	// Keep the global pointer valid: repoint it at another live instance, or null when the last
	// one is destroyed. All shared state is static, so any live instance is interchangeable.
	if (g_SemmetyLayout == this) {
		g_SemmetyLayout = s_instances.empty() ? nullptr : s_instances.back();
	}
}

void SemmetyLayout::onEnabled() {
	// The global tree, event listeners and adoption of pre-existing windows are process-wide, not
	// per-space, so set them up exactly once regardless of how many spaces get created.
	if (s_globalsInitialized) { return; }
	s_globalsInitialized = true;

	for (auto& window: g_pCompositor->m_windows) {
		if (window->isHidden() || !window->m_isMapped || window->m_fadingOut || window->m_isFloating)
			continue;

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_workspace);
		workspace_wrapper.addWindow(window);
	}

	renderListener = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
		renderHook(stage);
	});

	tickListener = Event::bus()->m_events.tick.listen([]() {
		tickHook();
	});

	workspaceListener = Event::bus()->m_events.workspace.active.listen([](PHLWORKSPACE) {
		semmety_log(Log::ERR, "WORKSPACE_HOOK");
		updateBar();
	});

	urgentListener = Event::bus()->m_events.window.urgent.listen([](PHLWINDOW) {
		auto layout = g_SemmetyLayout;
		if (layout == nullptr) { return; }

		layout->updateBarOnNextTick = true;
		g_pAnimationManager->scheduleTick();
	});

	windowTitleListener = Event::bus()->m_events.window.title.listen([](PHLWINDOW) {
		updateBar();
	});

	// Replaces the old IHyprLayout::onWindowFocusChange override (removed in the 0.55 algorithm
	// API): when the focused window changes, mark its frame active and refresh the bar.
	focusListener =
	    Event::bus()->m_events.window.active.listen([](PHLWINDOW window, Desktop::eFocusReason) {
		    if (!g_semmetyReady) { return; }

		    auto layout = g_SemmetyLayout;
		    if (layout == nullptr) { return; }
		    if (entryCount > 0) { return; }

		    layout->entryWrapper("onWindowFocusChange", [&]() -> std::optional<std::string> {
			    if (window == nullptr) { return "window is null"; }
			    if (window->m_workspace == nullptr) { return "window workspace is null"; }
			    if (window->m_isFloating) { return "window is floating"; }

			    auto& workspace_wrapper = layout->getOrCreateWorkspaceWrapper(window->m_workspace);
			    workspace_wrapper.activateWindow(window);

			    shouldUpdateBar();

			    return std::nullopt;
		    });
	    });
}

void SemmetyLayout::onDisabled() {
	renderListener.reset();
	tickListener.reset();
	workspaceListener.reset();
	urgentListener.reset();
	windowTitleListener.reset();
	focusListener.reset();
	s_globalsInitialized = false;
}

void SemmetyLayout::newTarget(SP<Layout::ITarget> target) {
	auto window = target->window();
	entryWrapper("newTarget", [&]() -> std::optional<std::string> {
		if (window->m_workspace == nullptr) {
			return "called with a window that has an invalid workspace";
		}

		semmety_log(
		    Log::INFO,
		    "newTarget called with window {:x} (floating: {}, monitor: {}, workspace: {})",
		    (uintptr_t) window.get(),
		    window->m_isFloating,
		    window->monitorID(),
		    window->m_workspace->m_id
		);

		if (window->m_isFloating) { return "window is floating"; }

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_workspace);

		// addWindow() is not idempotent (it unconditionally appends), so guard against re-adding a
		// window that is already tracked. movedTarget() routes here, and may fire for a target that
		// is already part of this space.
		if (workspace_wrapper.findWindowIt(window) != workspace_wrapper.windows.end()) {
			return "window already tracked in workspace";
		}

		workspace_wrapper.addWindow(window);

		// Re-apply layout on the next tick: the window isn't mapped yet here (so applyRecursive
		// can't size it), and Hyprland will reset it to the engine box once it maps.
		s_reflowPending = true;

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D>) {
	newTarget(target);
}

void SemmetyLayout::removeTarget(SP<Layout::ITarget> target) {
	auto window = target->window();
	entryWrapper("removeTarget", [&]() -> std::optional<std::string> {
		if (window->m_workspace == nullptr) { return "workspace is null"; }

		semmety_log(
		    Log::INFO,
		    "removeTarget window {:x} (floating: {}, monitor: {}, workspace: {}, title: {})",
		    (uintptr_t) window.get(),
		    window->m_isFloating,
		    window->monitorID(),
		    window->m_workspace->m_id,
		    window->fetchTitle()
		);

		window->updateWindowData();
		if (window->isFullscreen()) { g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE); }

		auto& workspace_wrapper = getOrCreateWorkspaceWrapper(window->m_workspace);
		workspace_wrapper.removeWindow(window);

		shouldUpdateBar();
		g_pAnimationManager->scheduleTick();

		return std::nullopt;
	});
}

void SemmetyLayout::recalculateWorkspace(const PHLWORKSPACE& workspace) {
	entryWrapper("recalculateWorkspace", [&]() -> std::optional<std::string> {
		if (workspace == nullptr) { return "workspace is null"; }

		const auto monitor = workspace->m_monitor;
		if (!monitor) { return "monitor is null"; }

		auto& ww = getOrCreateWorkspaceWrapper(workspace);

		// Mirror the pre-0.55 geometry: monitor box minus reserved area. (Per-window gaps are still
		// applied separately via SemmetyLeafFrame::getStandardWindowArea, so gaps_out is not removed
		// here, matching the original behaviour and the workspace-wrapper constructor.)
		auto reserved = monitor->m_reservedArea;
		auto pos = monitor->m_position + Vector2D(reserved.left(), reserved.top());
		auto size = monitor->m_size
		    - Vector2D(reserved.left() + reserved.right(), reserved.top() + reserved.bottom());
		ww.setRootGeometry(CBox(pos, size));

		return std::nullopt;
	});
}

void SemmetyLayout::recalculate(Layout::eRecalculateReason) {
	entryWrapper("recalculate", [&]() -> std::optional<std::string> {
		auto parent = m_parent.lock();
		if (!parent) { return "no parent algorithm"; }

		auto space = parent->space();
		if (!space) { return "no space"; }

		auto workspace = space->workspace();
		if (!workspace) { return "no workspace"; }

		// Only lay out a workspace we are already managing. Don't lazily build the frame tree here:
		// recalculate() can fire very early (e.g. during CMonitor::onConnect in unsafe state) before
		// the config/animation subsystems are ready to construct a frame, and the built-in algorithms
		// likewise only reposition existing nodes. The wrapper (and its root frame) is created on
		// first real use - window add or render - when construction is safe.
		SemmetyWorkspaceWrapper* ww = nullptr;
		for (auto& candidate: workspaceWrappers) {
			if (candidate.workspace.get() == &*workspace) {
				ww = &candidate;
				break;
			}
		}
		if (ww == nullptr) { return "workspace not managed yet"; }

		recalculateWorkspace(workspace);

		// In the 0.55 algorithm API recalculate() is the single entry point for (re)positioning
		// tiled windows (it replaces IHyprLayout::recalculateMonitor), so reflow the frame tree
		// after updating the root geometry. A fullscreen window is positioned by the engine, so
		// skip the tree in that case (matching the built-in algorithms).
		if (!workspace->m_hasFullscreenWindow) {
			ww->getRoot()->applyRecursive(*ww, std::nullopt, std::nullopt);
		}

		return std::nullopt;
	});
}

void SemmetyLayout::resizeTarget(
    const Vector2D& delta,
    SP<Layout::ITarget> target,
    Layout::eRectCorner corner
) {
	auto window = target->window();
	if (!valid(window)) { return; }

	if (window->m_isFloating) { return; }

	auto workspace = workspace_for_window(window);
	if (!workspace) { return; }

	auto frame = workspace->getFrameForWindow(window);
	if (!frame) { return; }

	auto resizeDelta = delta;
	SP<SemmetySplitFrame> horizontalParent, verticalParent;

	switch (corner) {
	case Layout::CORNER_TOPLEFT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up);
		resizeDelta.x *= -1;
		resizeDelta.y *= -1;
		break;
	case Layout::CORNER_TOPRIGHT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up);
		resizeDelta.y *= -1;
		break;
	case Layout::CORNER_BOTTOMRIGHT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Down);
		break;
	case Layout::CORNER_BOTTOMLEFT:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Down);
		resizeDelta.x *= -1;
		break;
	case Layout::CORNER_NONE:
		horizontalParent = getResizeTarget(*workspace, frame, Direction::Left, Direction::Right);
		verticalParent = getResizeTarget(*workspace, frame, Direction::Up, Direction::Down);
		break;
	}

	if (!horizontalParent && !verticalParent) { return; }

	if (horizontalParent) {
		if (horizontalParent->getChildren().second->isSameOrDescendant(frame)) { resizeDelta.x *= -1; }

		horizontalParent->resize(resizeDelta.x);
	}

	if (verticalParent) {
		if (verticalParent->getChildren().second->isSameOrDescendant(frame)) { resizeDelta.y *= -1; }

		verticalParent->resize(resizeDelta.y);
	}

	SP<SemmetyFrame> commonParent;
	if (!horizontalParent) {
		commonParent = verticalParent;
	} else if (!verticalParent) {
		commonParent = horizontalParent;
	} else {
		commonParent = getCommonParent(*workspace, horizontalParent, verticalParent);
	}

	commonParent->applyRecursive(*workspace, std::nullopt, true);
}

Layout::eFullscreenRequestResult SemmetyLayout::requestFullscreen(const Layout::SFullscreenRequest& request) {
	auto target = request.target;
	auto window = target->window();
	const auto CURRENT_EFFECTIVE_MODE = request.currentEffectiveMode;
	const auto EFFECTIVE_MODE = request.effectiveMode;

	entryWrapper("requestFullscreen", [&]() -> std::optional<std::string> {
		auto workspace = workspace_for_window(window);
		if (!workspace) { return "Failed to get workspace for window"; }

		semmety_log(Log::ERR, "current: {}, effective: {}", CURRENT_EFFECTIVE_MODE, EFFECTIVE_MODE);

		if (EFFECTIVE_MODE == FSMODE_NONE) {
			auto frame = workspace->getFrameForWindow(window);
			if (frame) {
				frame->applyRecursive(*workspace, std::nullopt, false);
			} else {
				auto lastSize = target->lastFloatingSize();
				*window->m_realPosition = window->m_realPosition->goal();
				*window->m_realSize = lastSize;

				window->updateWindowData();
			}
		} else {
			if (window->m_isFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
				target->rememberFloatingSize(window->m_realSize->goal());
				window->m_position = window->m_realPosition->goal();
				window->m_size = window->m_realSize->goal();
			}

			window->updateWindowData();
			window->updateWindowDecos();

			const auto& monitor = window->m_monitor;

			*window->m_realPosition = monitor->m_position;
			*window->m_realSize = monitor->m_size;
			g_pCompositor->changeWindowZOrder(window, true);
		}

		return std::nullopt;
	});

	return Layout::FULLSCREEN_REQUEST_DEFAULT;
}

Config::ErrorResult SemmetyLayout::layoutMsg(const std::string_view& sv) {
	semmety_log(Log::INFO, "STUB layoutMsg");
	return {};
}

void SemmetyLayout::swapTargets(SP<Layout::ITarget>, SP<Layout::ITarget>) {
	semmety_log(Log::INFO, "STUB swapTargets");
}

void SemmetyLayout::moveTargetInDirection(SP<Layout::ITarget>, Math::eDirection, bool) {
	semmety_log(Log::INFO, "STUB moveTargetInDirection");
}

std::optional<Vector2D> SemmetyLayout::predictSizeForNewTarget() {
	return entryWrapper("predictSizeForNewTarget", [&]() -> std::optional<Vector2D> {
		auto ws = workspace_for_action();

		if (!ws) { return std::nullopt; }

		return ws->getFocusedFrame()->geometry.size();
	});
}

SP<Layout::ITarget> SemmetyLayout::getNextCandidate(SP<Layout::ITarget> old) {
	auto window = old->window();
	if (!window) { return {}; }

	if (window->m_workspace && window->m_workspace->m_hasFullscreenWindow) {
		auto fsWindow = window->m_workspace->getFullscreenWindow();
		if (fsWindow) {
			auto space = old->space();
			if (space) {
				for (const auto& wt: space->targets()) {
					auto t = wt.lock();
					if (t && t->window() == fsWindow) { return t; }
				}
			}
		}
	}

	if (!window->m_isFloating) { return {}; }

	auto ws = workspace_for_window(window);
	if (!ws) { return {}; }

	const auto index = ws->getLastFocusedWindowIndex();
	if (index >= ws->windows.size()) { return {}; }

	auto candidateWindow = ws->windows[index].lock();
	if (!candidateWindow) { return {}; }

	auto space = old->space();
	if (!space) { return {}; }

	for (const auto& wt: space->targets()) {
		auto t = wt.lock();
		if (t && t->window() == candidateWindow) { return t; }
	}

	return {};
}
