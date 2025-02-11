#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w) {
  workspace = w;

  auto frame = makeShared<SemmetyEmptyFrame>();
  this->root = frame;
  this->focused_frame = frame;
}

SP<SemmetyFrame> SemmetyWorkspaceWrapper::getFrameForWindow(PHLWINDOWREF window) const {
    std::list<SP<SemmetyFrame>> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->is_window() && current->getWindow() == window) {
            return current;
        }

        if (auto parentFrame = std::dynamic_pointer_cast<SemmetyParentFrame>(current)) {
            for (const auto& child : parentFrame->children) {
                stack.push_back(child);
            }
        }
    }

    return nullptr; // Return null if no matching window frame is found
}

std::list<SP<SemmetyFrame>> SemmetyWorkspaceWrapper::getWindowFrames() const {
    std::list<SP<SemmetyFrame>> windowFrames;
    std::list<SP<SemmetyFrame>> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->is_window()) {
            windowFrames.push_back(current);
        }

        if (auto parentFrame = std::dynamic_pointer_cast<SemmetyParentFrame>(current)) {
            for (const auto& child : parentFrame->children) {
                stack.push_back(child);
            }
        }
    }

    return windowFrames;
}

SemmetyFrame& SemmetyWorkspaceWrapper::getFocusedFrame() {
  if (!this->focused_frame) {
      throw std::runtime_error("No active frame, were outputs added to the desktop?");
  }

  if (!this->focused_frame->is_leaf()) {
      throw std::runtime_error("Active frame is not a leaf");
  }

  return *this->focused_frame;
}

void SemmetyWorkspaceWrapper::putWindowInFocusedFrame(PHLWINDOWREF window) {
  //
}
