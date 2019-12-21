project('wofi', 'c', version : 'v1.0', default_options : ['buildtype=release', 'warning_level=2'])
cc = meson.get_compiler('c')

inc = include_directories('inc')
gtk = dependency('gtk+-3.0')
gio = dependency('gio-unix-2.0')
threads = dependency('threads')
wayland = dependency('wayland-client')
dl = cc.find_library('dl')

add_project_arguments('-D_GNU_SOURCE', '-DVERSION="' + meson.project_version() + '"', language : 'c')
add_project_link_arguments('-rdynamic', language : 'c')

executable('wofi',
'src/config.c',
'src/main.c',
'src/map.c',
'src/property_box.c',
'src/utils.c',
'src/wofi.c',
'modes/dmenu.c',
'modes/drun.c',
'modes/run.c',
'proto/wlr-layer-shell-unstable-v1-protocol.c',
'proto/xdg-shell-protocol.c',
include_directories : inc,
dependencies : [gtk, gio, threads, wayland, dl])
