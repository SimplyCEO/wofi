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
## Packages
Debian sid has a package in the official repos https://packages.debian.org/sid/wofi

Ubuntu focal has a package in universe https://packages.ubuntu.com/focal/wofi

Arch has an AUR package for the current tip https://aur.archlinux.org/packages/wofi-hg/ and for the current stable https://aur.archlinux.org/packages/wofi/

NixOS has a packge in unstable https://nixos.org/nixos/packages.html?attr=wofi&channel=nixos-unstable&query=wofi

Void Linux also has a package

Feodra has an official package https://src.fedoraproject.org/rpms/wofi as well as one in COPR https://copr.fedorainfracloud.org/coprs/wef/wofi/
## Documentation
The official documentation is provided by the man pages in this repository, sample styling can be found here https://cloudninja.pw/docs/wofi.html

## Donating
If you feel like supporting development you can either buy me a tea https://www.buymeacoffee.com/Scoopta or support me monthly https://liberapay.com/Scoopta

## Screenshots
[![example 4](https://f.cloudninja.pw/Scaled_4.png)](https://f.cloudninja.pw/Rootbar_Example_4.png)
