# Wofi
Wofi is a launcher/menu program for wlroots based wayland compositors such as sway

If you're having issues with -i not showing images refer to https://todo.sr.ht/~scoopta/wofi/33

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
	sudo cp build/wofi /usr/bin
## Uninstalling
	sudo rm /usr/bin/wofi
## Bug Reports
Please file bug reports at https://todo.sr.ht/~scoopta/wofi
## Contributing
Please submit patches to https://lists.sr.ht/~scoopta/wofi

You can find documentation here https://man.sr.ht/hg.sr.ht/email.md
## Packages
If you're on Arch there's an unofficial AUR package https://aur.archlinux.org/packages/wofi-hg/
## Documentation
Documentation on styling and general configuration can be found here https://cloudninja.pw/docs/wofi.html
