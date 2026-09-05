Notepad # macOS Experimental Package
===================================

This package is not Developer ID signed or notarized by Apple. Its ad-hoc
signature supports execution on Apple Silicon but does not identify a trusted
publisher. It is an experimental build, not an approved enterprise artifact.

Installation
------------
1. Keep the DMG or portable ZIP, matching source ZIP, and checksum manifest
   together. Verify the SHA-256 hashes before opening the application.
2. Open the DMG and drag NotepadSharp.app to Applications, or extract the
   portable ZIP and move the complete application bundle to Applications.
3. Start NotepadSharp.app. A downloaded unsigned build may be blocked by
   Gatekeeper. Only after verifying its source and checksum, use macOS System
   Settings > Privacy & Security > Open Anyway for this specific application.
   Do not disable Gatekeeper globally.

Universal packages contain native arm64 and x86_64 code. Minimum macOS version
and build details are recorded in SOURCE_INFO.txt inside the application's
Contents/Resources directory. Qt frameworks and plugins are bundled; installing
Qt separately is not required.

Updates and removal
-------------------
Quit the application before replacing it with a newer verified build. Online
updates are disabled. To uninstall, move the application to Trash. User settings
and session data are retained. Do not store documents inside the app bundle.

Licensing and source
--------------------
Notepad # is a modified GPLv3-or-later application based on Notepad Next.
License texts, dependency notices, and source locations are in
Contents/Resources. The matching project-source ZIP accompanies the binaries;
external dependency source trees are not embedded in that ZIP. See
SOURCE_INFO.txt and THIRD_PARTY_NOTICES.txt. Final corresponding-source and
legal approval remain subject to the project's release review.