#pragma once

#include "src/SemmetyLayout.hpp"

inline HANDLE PHANDLE = nullptr;
inline SemmetyLayout* g_SemmetyLayout = nullptr;

// False until Hyprland enters its Wayland event loop (set on the first `tick`). Frame creation
// builds animated variables, which is unsafe during synchronous compositor startup, so the
// frame-creating event hooks no-op until this is true.
inline bool g_semmetyReady = false;
