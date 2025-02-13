#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
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

#include <hyprland/src/plugins/PluginAPI.hpp>

#include "globals.hpp"
#include "log.hpp"

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l) : layout(l) {
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
            for (const auto& child : current->data.as_parent().children) {
                stack.push_back(child);
            }
        }
    }

    return leafFrames;
}

void SemmetyWorkspaceWrapper::rebalance() {
    auto emptyFrames = getEmptyFrames();
    emptyFrames.sort([](const SP<SemmetyFrame>& a, const SP<SemmetyFrame>& b) {
        return a->geometry.size().x * a->geometry.size().y > b->geometry.size().x * b->geometry.size().y;
    });
    auto frameIt = emptyFrames.begin();

    for (auto windowIt = minimized_windows.begin(); windowIt != minimized_windows.end() && frameIt != emptyFrames.end(); ++windowIt, ++frameIt) {
        (*frameIt)->data = *windowIt;
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
            for (const auto& child : current->data.as_parent().children) {
                stack.push_back(child);
            }
        }
    }

    return emptyFrames;
}

    
	// static const auto p_gaps_in = ConfigValue<Hyprlang::CUSTOMTYPE, CCssGapData>("general:gaps_in");
	// static const auto p_gaps_out = ConfigValue<Hyprlang::CUSTOMTYPE, CCssGapData>("general:gaps_out");
	// static const auto group_inset = ConfigValue<Hyprlang::INT>("plugin:hy3:group_inset");
	// static const auto tab_bar_height = ConfigValue<Hyprlang::INT>("plugin:hy3:tabs:height");
	// static const auto tab_bar_padding = ConfigValue<Hyprlang::INT>("plugin:hy3:tabs:padding");

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

void SemmetyWorkspaceWrapper::addWindow(PHLWINDOWREF window) {
    putWindowInFocusedFrame(window);
}

void SemmetyWorkspaceWrapper::removeWindow(PHLWINDOWREF window) {
    auto frameWithWindow = getFrameForWindow(window);
    if (frameWithWindow) {
        frameWithWindow->data = SemmetyFrame::Empty{};
    }
    minimized_windows.remove(window);
}

void SemmetyWorkspaceWrapper::minimizeWindow(PHLWINDOWREF window) {
    auto frameWithWindow = getFrameForWindow(window);
    if (frameWithWindow) {
        frameWithWindow->data = SemmetyFrame::Empty{};
        minimized_windows.push_back(window);
    }
}

SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFrameForWindow(PHLWINDOWREF window) const
{
    std::list<SP<SemmetyFrame>> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->data.is_window() && current->data.as_window() == window) {
            return current;
        }

        if (current->data.is_parent()) {
            for (const auto& child : current->data.as_parent().children) {
                stack.push_back(child);
            }
        }
    }

    return nullptr; // Return null if no matching window frame is found
}



SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFocusedFrame()
{
  if (!this->focused_frame) {
      semmety_log(ERR, "No active frame, were outputs added to the desktop?");
      semmety_critical_error("No active frame, were outputs added to the desktop?");
  }

  if (!this->focused_frame->data.is_leaf()) {
      semmety_critical_error("Active frame is not a leaf");
  }

  return this->focused_frame;
}

void SemmetyWorkspaceWrapper::setFocusedFrame(SP<SemmetyFrame> frame)
{
    if (!frame) {
        semmety_log(ERR, "Cannot set a null frame as focused");
        semmety_critical_error("Cannot set a null frame as focused");
    }

    if (!frame->data.is_leaf()) {
        semmety_log(ERR, "Focused frame must be a leaf");
        semmety_critical_error("Focused frame must be a leaf");
    }

    this->focused_frame = frame;
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
     frameWithWindow->data = SemmetyFrame::Empty{};
 } else {
     minimized_windows.remove(window);
 }

 focusedFrame->data = window;
}

void SemmetyWorkspaceWrapper::apply() {
    rebalance();


      root->propagateGeometry();
      root->applyRecursive(workspace.lock());

    // for (const auto& window : minimized_windows) {
    //     window.lock().get()->setHidden(true);
    // }
}

