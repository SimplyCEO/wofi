Wofi
====

Wofi is a launcher/menu program for wlroots based wayland compositors such as sway.

This repository is a <b>mirror</b> of https://hg.sr.ht/~scoopta/wofi with some [additions](https://github.com/SimplyCEO/wofi/commits/master/?author=SimplyCEO).

Dependencies
------------

- PkgConf
- Wayland
- GLib2
- GObject
- GTK3

`cmake` or `meson` is required to build.
Use <b>CMake</b> if your C compiler is not <b>GCC</b> or <b>Clang</b>.

Building
--------

Clone the repository:

```shell
git clone --depth 1 https://github.com/SimplyCEO/wofi.git
cd wofi
```

Then build with your desired build system:

- CMake:

```shell
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
```

- Meson:

```shell
meson setup build
ninja -C build
```

Installing
----------

- CMake:

```shell
su -c "cmake --install build"
```

- Meson:

```shell
su -c "ninja -C build install"
```

Uninstalling
------------

- CMake:

```shell
su -c "xargs -a build/install_manifest.txt rm"
```

- Meson:

```shell
su -c "ninja -C build uninstall"
```


Bug Reports
-----------

Please file bug reports at https://todo.sr.ht/~scoopta/wofi

Contributing
------------

Please submit patches to https://lists.sr.ht/~scoopta/wofi

You can find documentation here https://man.sr.ht/hg.sr.ht/email.md

drun and dbus
-------------

Some desktop files declare themselves as being launched by dbus,
if this is the case wofi can experience issues
on systems where a user session bus is not automatically started
such as systems using elogind.

To manually launch a user session bus run the following:

```shell
dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus
```

Packages
--------

[![Packaging status](https://repology.org/badge/vertical-allrepos/wofi.svg)](https://repology.org/project/wofi/versions)

Documentation
-------------

The official documentation is provided by the man pages in this repository, sample styling can be found here https://cloudninja.pw/docs/wofi.html

Donating
--------

If you feel like supporting development you can donate at https://ko-fi.com/scoopta

Screenshots
-----------

[![example 4](https://f.cloudninja.pw/Scaled_4.png)](https://f.cloudninja.pw/Rootbar_Example_4.png)

