project('semmety', 'cpp',
  default_options: ['buildtype=debugoptimized', 'cpp_std=c++23'],
)

add_project_arguments([
  '-DWLR_USE_UNSTABLE',
], language: 'cpp')

src = files(
  './src/dispatchers.cpp',
  './src/SemmetyFrame.cpp',
  './src/SemmetyFrameUtils.cpp',
  './src/SemmetyLayout.cpp',
  './src/SemmetyLayoutHypr.cpp',
  './src/SemmetyLayoutHooks.cpp',
  './src/SemmetyWorkspaceWrapper.cpp',
  './src/utils.cpp',
  './src/main.cpp',
)

shared_module(meson.project_name(), src,
  dependencies: [
    dependency('hyprland'),
    dependency('pixman-1'),
    dependency('libdrm'),
  ],
  install: true,
)
