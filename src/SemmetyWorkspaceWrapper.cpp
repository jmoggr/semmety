#include "SemmetyWorkspaceWrapper.hpp"
#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>

SemmetyWorkspaceWrapper::SemmetyWorkspaceWrapper(PHLWORKSPACEREF w) {
  workspace = w;

  auto frame = makeShared<SemmetyEmptyFrame>();
  this->root = frame;
  this->focused_frame = frame;
}

SemmetyFrame& SemmetyWorkspaceWrapper::getActiveFrame() {
  // assert that the frame is not a parent?
}

void SemmetyWorkspaceWrapper::putWindowInActiveFrame(PHLWINDOWREF window) {
  //
}
