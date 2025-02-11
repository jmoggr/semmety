#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w) {
  workspace = w;

  auto frame = makeShared<SemmetyEmptyFrame>();
  this->root = frame;
  this->focused_frame = frame;
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
