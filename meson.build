project('wofi', 'c', default_options : ['buildtype=release', 'warning_level=2'])
cc = meson.get_compiler('c')

inc = include_directories('inc')
gtk = dependency('gtk+-3.0')
threads = dependency('threads')
wayland = dependency('wayland-client')
dl = cc.find_library('dl')

hg = find_program('hg', native : true, required : false)

version = get_option('version')

if hg.found()
	version = run_command(hg, 'identify').stdout().strip()
endif

add_project_arguments('-D_GNU_SOURCE', '-DVERSION="' + version + '"', language : 'c')
add_project_link_arguments('-rdynamic', language : 'c')

sources = ['src/config.c',
			'src/main.c',
			'src/map.c',
			'src/property_box.c',
			'src/utils.c',
			'src/wofi.c',
			'proto/wlr-layer-shell-unstable-v1-protocol.c',
			'proto/xdg-shell-protocol.c']
			
deps = [gtk, threads, wayland, dl]
			
if get_option('enable_run')
	sources += ['modes/run.c']
endif

if get_option('enable_drun')
	gio = dependency('gio-unix-2.0')
	sources += ['modes/drun.c']
	deps += [gio]
endif

if get_option('enable_dmenu')
	sources += ['modes/dmenu.c']
endif

executable(meson.project_name(), sources, include_directories : inc, dependencies : deps, install : true)
