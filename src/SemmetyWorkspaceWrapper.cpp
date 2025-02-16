#include "SemmetyWorkspaceWrapper.hpp"
#include <numeric>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <ranges>

#include "globals.hpp"
#include "log.hpp"

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l): layout(l) {
	workspace = w;

	auto& monitor = w->m_pMonitor;
	auto pos = monitor->vecPosition + monitor->vecReservedTopLeft;
	auto size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;
	auto frame = makeShared<SemmetyFrame>(pos, size);

	semmety_log(ERR, "init workspace monitor size {} {}", monitor->vecSize.x, monitor->vecSize.y);
	semmety_log(ERR, "workspace has root frame: {}", frame->print());

	this->root = frame;
	this->focused_frame = frame;
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

		if (current->data.is_leaf()) {
			leafFrames.push_back(current);
		}

		if (current->data.is_parent()) {
			for (const auto& child: current->data.as_parent().children) {
				stack.push_back(child);
			}
		}
	}

	return leafFrames;
}

void SemmetyWorkspaceWrapper::rebalance() {
	auto emptyFrames = getEmptyFrames();
	emptyFrames.sort([](const SP<SemmetyFrame>& a, const SP<SemmetyFrame>& b) {
		return a->geometry.size().x * a->geometry.size().y
		     > b->geometry.size().x * b->geometry.size().y;
	});
	auto frameIt = emptyFrames.begin();

	for (auto windowIt = minimized_windows.begin();
	     windowIt != minimized_windows.end() && frameIt != emptyFrames.end();)
	{

		(*frameIt)->data = *windowIt;
		windowIt = minimized_windows.erase(windowIt);

		++frameIt;
	}
}

std::list<SP<SemmetyFrame>> SemmetyWorkspaceWrapper::getEmptyFrames() const {
	std::list<SP<SemmetyFrame>> emptyFrames;
	std::list<SP<SemmetyFrame>> stack;
	stack.push_back(root);

	while (!stack.empty()) {
		auto current = stack.back();
		stack.pop_back();

		if (current->data.is_empty()) {
			emptyFrames.push_back(current);
		}

		if (current->data.is_parent()) {
			for (const auto& child: current->data.as_parent().children) {
				stack.push_back(child);
			}
		}
	}

	return emptyFrames;
}

// static const auto p_gaps_in = ConfigValue<Hyprlang::CUSTOMTYPE, CCssGapData>("general:gaps_in");
// static const auto p_gaps_out = ConfigValue<Hyprlang::CUSTOMTYPE,
// CCssGapData>("general:gaps_out"); static const auto group_inset =
// ConfigValue<Hyprlang::INT>("plugin:hy3:group_inset"); static const auto tab_bar_height =
// ConfigValue<Hyprlang::INT>("plugin:hy3:tabs:height"); static const auto tab_bar_padding =
// ConfigValue<Hyprlang::INT>("plugin:hy3:tabs:padding");

// auto workspace_rule = g_pConfigManager->getWorkspaceRuleFor(this->workspace);
// auto gaps_in = workspace_rule.gapsIn.value_or(*p_gaps_in);
// auto gaps_out = workspace_rule.gapsOut.value_or(*p_gaps_out);

// auto gap_topleft_offset = Vector2D(
//     (int) -(gaps_in.left - gaps_out.left),
//     (int) -(gaps_in.top - gaps_out.top)
// );

// auto gap_bottomright_offset = Vector2D(
// 		(int) -(gaps_in.right - gaps_out.right),
// 		(int) -(gaps_in.bottom - gaps_out.bottom)
// );
// // clang-format on

void SemmetyWorkspaceWrapper::addWindow(PHLWINDOWREF window) { putWindowInFocusedFrame(window); }

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		frameWithWindow->data = SemmetyFrame::Empty {};
	}
	minimized_windows.remove(window);
}

void SemmetyWorkspaceWrapper::minimizeWindow(PHLWINDOWREF window) {
	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		frameWithWindow->data = SemmetyFrame::Empty {};
		minimized_windows.push_back(window);
	}
}

SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFrameForWindow(PHLWINDOWREF window) const {
	std::list<SP<SemmetyFrame>> stack;
	stack.push_back(root);

	while (!stack.empty()) {
		auto current = stack.back();
		stack.pop_back();

		if (current->data.is_window() && current->data.as_window() == window) {
			return current;
		}

		if (current->data.is_parent()) {
			for (const auto& child: current->data.as_parent().children) {
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

	if (!this->focused_frame->data.is_leaf()) {
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

	if (frame->data.is_leaf()) {
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

	if (focusedFrame->data.is_window()) {
		if (focusedFrame->data.as_window() == window) {
			return;
		}

		minimized_windows.push_back(focusedFrame->data.as_window());
	}

	auto frameWithWindow = getFrameForWindow(window);
	if (frameWithWindow) {
		frameWithWindow->data = SemmetyFrame::Empty {};
	} else {
		minimized_windows.remove(window);
	}

	focusedFrame->data = window;
}

void SemmetyWorkspaceWrapper::apply() {
	root->propagateGeometry();
	rebalance();
	if (this->focused_frame->data.is_window()) {
		const auto window = this->focused_frame->data.as_window().lock();
		if (window != g_pCompositor->m_pLastWindow) {
			semmety_log(ERR, "setting focus window");
			// g_pCompositor->focusWindow(window);
		}
	} else {
		if (g_pCompositor->m_pLastWindow != nullptr) {
			// g_pCompositor->focusWindow(nullptr);
		}
	}

	// g_pAnimationManager->scheduleTick();
	root->applyRecursive(workspace.lock());

	// for (const auto& window : minimized_windows) {
	//     window.lock().get()->setHidden(true);
	// }
}
