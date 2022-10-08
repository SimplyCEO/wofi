# Wofi
Wofi is a launcher/menu program for wlroots based wayland compositors such as sway

[![builds.sr.ht status](https://builds.sr.ht/~scoopta/wofi.svg)](https://builds.sr.ht/~scoopta/wofi?)
## Dependencies
	libwayland-dev
	libgtk-3-dev
	pkg-config
	meson
## Building
	hg clone https://hg.sr.ht/~scoopta/wofi
	cd wofi
	meson build
	ninja -C build
## Installing
	sudo ninja -C build install
## Uninstalling
	sudo ninja -C build uninstall
## Bug Reports
Please file bug reports at https://todo.sr.ht/~scoopta/wofi
## Contributing
Please submit patches to https://lists.sr.ht/~scoopta/wofi

You can find documentation here https://man.sr.ht/hg.sr.ht/email.md

## drun and dbus
Some desktop files declare themselves as being launched by dbus, if this is the case wofi can experience issues on systems where a user session bus is not automatically started such as systems using elogind.

To manually launch a user session bus run the following:

	dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus

## Packages
[![Packaging status](https://repology.org/badge/vertical-allrepos/wofi.svg)](https://repology.org/project/wofi/versions)

## Documentation
The official documentation is provided by the man pages in this repository, sample styling can be found here https://cloudninja.pw/docs/wofi.html

## Donating
If you feel like supporting development you can donate at https://ko-fi.com/scoopta

## Screenshots
[![example 4](https://f.cloudninja.pw/Scaled_4.png)](https://f.cloudninja.pw/Rootbar_Example_4.png)
