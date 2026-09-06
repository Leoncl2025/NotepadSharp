# Notepad \#

[![Release](https://img.shields.io/github/v/release/Leoncl2025/NotepadSharp?display_name=tag&sort=semver)](https://github.com/Leoncl2025/NotepadSharp/releases)
[![Downloads](https://img.shields.io/github/downloads/Leoncl2025/NotepadSharp/total)](https://github.com/Leoncl2025/NotepadSharp/releases)
[![License](https://img.shields.io/github/license/Leoncl2025/NotepadSharp)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-0078D4)](#platform-support)

Notepad # is a reimplementation of Notepad++ built with compliance and stronger security as core design goals.

## macOS: Dark Mode & Compare

![Notepad # on macOS in dark mode with side-by-side file comparison](/doc/screenshot_macos.png)

## Windows: Dark Mode & Compare

![screenshot](/doc/screenshot.png)

## JSON Pretty View

Use **Edit > JSON** to pretty-print or compact JSON in place.

| Command | Windows | macOS |
| --- | --- | --- |
| Toggle Pretty / Compact | Ctrl+Alt+J | Cmd+Option+J |
| Compact JSON | Ctrl+Alt+Shift+J | Cmd+Option+Shift+J |
| Pretty Print JSON | Edit > JSON menu | Edit > JSON menu |

A single selection limits formatting to that selection; otherwise the whole document is formatted. The editor's indentation and line-ending settings are respected, and the operation can be undone in one step. Formatting a whole plain-text document also enables JSON syntax highlighting. Read-only documents, rectangular selections, and multiple selections are not modified.

Malformed JSON is still laid out as far as its recognizable structure allows. A warning reports the error's line and column in the formatted document. Formatting does not repair syntax or discard comments and incomplete strings. Key order, duplicate keys, number spelling, and escapes inside JSON values are preserved.

Serialized JSON objects and arrays are decoded automatically, including multiple encoding layers and escaped fragments without outer quotes. For example, `"{\"name\":\"demo\",\"items\":[1,2]}"` becomes:

```json
{
	"name": "demo",
	"items": [
		1,
		2
	]
}
```

Decoding removes the serialization layer's `\"` escapes using a JSON parser, while retaining necessary escapes within the resulting values. Nested string values in an ordinary JSON document are not automatically unwrapped. If the decoded JSON is malformed, it is still formatted with a warning.

### JSON Navigation, Queries, and JSON Lines

**Edit > JSON > JSON Tools** opens the Structure, Query, Validation, and Compare panel. **Find JSON Path** uses Ctrl+Alt+P on Windows and Cmd+Option+P on macOS.

The status bar follows the current JSON node and copies its path when clicked. The editor's JSON context menu copies the clicked node's JSONPath, JSON Pointer, key, raw JSON value, or decoded string. The panel supports JSONPath filters and wildcards, key/value search, source selection, subtree formatting and folding, duplicate-key warnings, offline JSON Schema validation, and structural comparison with optional object-key-order checking.

Files ending in `.jsonl` or `.ndjson` use JSON Lines mode automatically; it can also be selected in the panel. Invalid records do not block later records. Pretty-printing a record opens an unsaved preview, while compacting leaves invalid records unchanged. See [the JSON tools guide](doc/JSON.md) for examples, limits, and test fixtures.

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
| JSON Pretty View | Core | Supported |
| XML Viewer | Core | Planned |
| Markdown View/Edit | Plugin | Planned |

Plugin support is planned for a future release. Features such as Markdown viewing and editing will be delivered as plugins rather than built into the core application.
