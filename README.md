# Notepad \#

[![Release](https://img.shields.io/github/v/release/Leoncl2025/NotepadSharp?display_name=tag&sort=semver)](https://github.com/Leoncl2025/NotepadSharp/releases)
[![Downloads](https://img.shields.io/github/downloads/Leoncl2025/NotepadSharp/total)](https://github.com/Leoncl2025/NotepadSharp/releases)
[![License](https://img.shields.io/github/license/Leoncl2025/NotepadSharp)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-0078D4)](#platform-support)

Notepad # is a reimplementation of Notepad++ built with compliance and stronger security as core design goals.

![screenshot](/doc/screenshot.png)

## Origin and License

Notepad # is a modified version of [Notepad Next](https://github.com/dail8859/NotepadNext). The Notepad # modifications began in 2026. Original copyright, license, warranty, and attribution notices are retained in the source tree and distribution materials.

The Notepad++ and Notepad Next names are used only to describe compatibility and upstream provenance. Notepad # is distributed as a distinct modified project.

Notepad # is distributed under the GNU General Public License version 3 or later. See [LICENSE](LICENSE) for the complete terms.

## Platform Support

Notepad # supports Windows and provides experimental macOS builds. Linux support is planned for a future release.

| Platform | Status |
| --- | --- |
| Windows | Supported |
| Linux | Planned |
| macOS 11 or later | Experimental; universal Apple Silicon and Intel build |

## macOS Packages

The release workflow builds macOS packages alongside Windows packages for [GitHub Releases](https://github.com/Leoncl2025/NotepadSharp/releases). macOS artifacts include a DMG, a portable application ZIP, a matching project-source ZIP, and `SHA256SUMS-macOS-universal.txt`.

Open the DMG and drag `NotepadSharp.app` to Applications, or extract the portable ZIP. Qt is included in the application bundle and does not need to be installed separately.

These experimental builds are **not Developer ID signed or notarized**. An ad-hoc signature supports Apple Silicon execution but does not establish publisher identity. Verify the download's SHA-256 checksum before opening it. If Gatekeeper blocks a trusted, verified download, use System Settings > Privacy & Security > Open Anyway for that application; do not disable Gatekeeper globally.

License texts, dependency notices, and build provenance are inside the application's `Contents/Resources` directory. Keep the matching source ZIP with the binary package. Online updates are disabled.

### Build on macOS

Prerequisites: Apple Command Line Tools, CMake 3.21 or later, Ninja, and Qt 6.5.3 for macOS with the `qt5compat` module. The official Qt `clang_64` package contains both arm64 and x86_64 libraries. Adjust `CMAKE_PREFIX_PATH` to your Qt installation.

```sh
cmake -S . -B build-macos -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DBUILD_TESTING=ON \
	'-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
	-DCMAKE_PREFIX_PATH="$HOME/Qt/6.5.3/macos"
cmake --build build-macos --parallel
ctest --test-dir build-macos --output-on-failure
cmake --build build-macos --target macos_bundle --parallel
cd build-macos/artifacts
shasum -a 256 -c SHA256SUMS-macOS-universal.txt
```

Packages are written to `build-macos/artifacts`. Tagged builds create a draft GitHub Release only after both Windows and macOS build, test, and package checks pass. Publishing the draft remains a separate release action.

## Roadmap

| Feature | Delivery Model | Status |
| --- | --- | --- |
| Compare | Core | Supported |
| XML/JSON Viewer | Core | Planned |
| Markdown View/Edit | Plugin | Planned |

Plugin support is planned for a future release. Features such as Markdown viewing and editing will be delivered as plugins rather than built into the core application.
