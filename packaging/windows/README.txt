Notepad # Windows x64 Internal Test Package
==========================================

This package is an unsigned internal test build. It is intended for validating
Notepad # on another Windows x64 machine. It is not an approved enterprise
release, a signed publisher artifact, or an MSIT security approval claim.

Installation
------------
1. Keep the installer, its SHA256SUMS.txt file, and the matching source ZIP
   together.
2. Verify the installer hash against SHA256SUMS.txt.
3. Run the installer. Per-user application installation is the default. The
   bundled Microsoft Visual C++ runtime prerequisite or an all-users install may
   request elevation.
4. Windows SmartScreen may warn because this internal test build is unsigned.
   Verify the hash before choosing to run it.

For the portable ZIP, extract the whole archive. Run vc_redist.x64.exe once if
the machine does not already have the Microsoft Visual C++ 2015-2022 x64
runtime, then start NotepadSharp.exe.

Upgrade and rollback
--------------------
Running a newer or older installer replaces the existing Notepad # installation
in the selected scope. User settings are retained under the NotepadSharp
application identity so that an uninstall/reinstall does not silently erase
preferences. Test rollback before relying on it for operational use.
Do not store documents in the program installation directory; that directory is
owned by the installer and is recursively replaced or removed.

Removal
-------
Use Windows Settings > Apps > Installed apps > Notepad # > Uninstall. The
uninstaller removes program files, shortcuts, application registration, and the
optional context-menu entry. It intentionally retains user settings.

Updates and data
----------------
Automatic and manual online updates are disabled for this package. Upgrade by
running another verified installer. Notepad # adds no telemetry or crash-report
upload in this package. Core editor settings and session data remain local to
the Windows user profile.

Licensing and source
--------------------
Notepad # is a modified GNU GPL version 3 application based on Notepad Next.
The Notepad # modifications began in 2026.
LICENSE-GPL-3.0.txt and available dependency license files are installed with
the program. The Notepad # project source snapshot used for this package is
distributed as the matching NotepadSharp-v<version>-source.zip beside the
installer. External dependency source trees are not embedded in that ZIP; see
SOURCE_INFO.txt and THIRD_PARTY_NOTICES.txt for provenance and source locations.
Final corresponding-source and legal approval remain in T-0022.

Do not pass this installer to another person without also passing the matching
source ZIP, license files, and checksum manifest. Legal and security review are
still authoritative for any broader distribution.