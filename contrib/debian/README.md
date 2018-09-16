
Debian
====================
This directory contains files used to package domod/domo-qt
for Debian-based Linux systems. If you compile domod/domo-qt yourself, there are some useful files here.

## domo: URI support ##


domo-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install domo-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your domoqt binary to `/usr/bin`
and the `../../share/pixmaps/domo128.png` to `/usr/share/pixmaps`

domo-qt.protocol (KDE)

