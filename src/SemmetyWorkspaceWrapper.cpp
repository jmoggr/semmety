#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Monitor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w, SemmetyLayout& l) : layout(l) {
    workspace = w;

    	auto& monitor = w->m_pMonitor;
    auto frame = makeShared<SemmetyFrame>();

    frame->position = monitor->vecPosition + monitor->vecReservedTopLeft;
    frame->size = monitor->vecSize - monitor->vecReservedTopLeft - monitor->vecReservedBottomRight;

    this->root = frame;
    this->focused_frame = frame;
}

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

SemmetyFrame& SemmetyWorkspaceWrapper::getFocusedFrame()
{
  if (!this->focused_frame) {
      throw std::runtime_error("No active frame, were outputs added to the desktop?");
  }

  if (!this->focused_frame->data.is_leaf()) {
      throw std::runtime_error("Active frame is not a leaf");
  }

  return *this->focused_frame;
}

void SemmetyWorkspaceWrapper::putWindowInFocusedFrame(PHLWINDOWREF window)
{
    auto& focusedFrame = getFocusedFrame();

    if (focusedFrame.data.is_window()) {
        if (focusedFrame.data.as_window() == window) {
            return;
        }

        minimized_windows.push_back(focusedFrame.data.as_window());
    }

    auto frameWithWindow = getFrameForWindow(window);
    if (frameWithWindow) {
        frameWithWindow->data = SemmetyFrame::Empty{};
    } else {
        minimized_windows.remove(window);
    }

    focusedFrame.data = window;
}

void SemmetyWorkspaceWrapper::apply() {
      root->propagateGeometry();
      root->applyRecursive();
}
