# Project
project('medialint', 'c',
  version: '0.1',
  default_options: ['warning_level=2', 'c_std=c17']
)

# Deps
glib_dep = dependency('glib-2.0', required: true)
libavcodec_dep = dependency('libavcodec', required: true)
libavformat_dep = dependency('libavformat', required: true)
libavutil_dep = dependency('libavutil', required: true)

# Sources
sources = files('src/main.c', 'src/report.c')

# Executable
executable('medialint',
  sources,
  dependencies: [glib_dep, libavcodec_dep, libavformat_dep, libavutil_dep]
)
