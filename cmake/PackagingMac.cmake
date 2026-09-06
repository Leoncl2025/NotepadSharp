message(STATUS "Configuring macOS packaging")

set(INSTALL_DIR "${CMAKE_BINARY_DIR}/install")
set(ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(PACKAGE_RESOURCE_DIR "NotepadSharp.app/Contents/Resources")
set(PACKAGE_LICENSE_DIR "${PACKAGE_RESOURCE_DIR}/licenses")

set(MACOS_PACKAGE_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}")
if(NOT MACOS_PACKAGE_ARCHITECTURES)
    set(MACOS_PACKAGE_ARCHITECTURES "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if("arm64" IN_LIST MACOS_PACKAGE_ARCHITECTURES AND "x86_64" IN_LIST MACOS_PACKAGE_ARCHITECTURES)
    set(MACOS_PACKAGE_ARCH "universal")
else()
    list(JOIN MACOS_PACKAGE_ARCHITECTURES "-" MACOS_PACKAGE_ARCH)
endif()

set(PACKAGE_NAME "NotepadSharp-v${PROJECT_VERSION}-macOS-${MACOS_PACKAGE_ARCH}")
set(DMG_FILE "${ARTIFACT_DIR}/${PACKAGE_NAME}-Unsigned.dmg")
set(PORTABLE_ZIP "${ARTIFACT_DIR}/${PACKAGE_NAME}-Unsigned-Portable.zip")
set(SOURCE_ARCHIVE_NAME "${PACKAGE_NAME}-source.zip")
set(SOURCE_ZIP "${ARTIFACT_DIR}/${SOURCE_ARCHIVE_NAME}")

get_filename_component(QT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_ROOT}/bin" REQUIRED)
find_program(CODESIGN_EXECUTABLE codesign REQUIRED)
find_program(DITTO_EXECUTABLE ditto REQUIRED)
find_program(LIPO_EXECUTABLE lipo REQUIRED)
find_package(Git REQUIRED)

set_target_properties(NotepadSharp PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/deploy/macos/info.plist"
)

set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/icon/NotepadSharp.icns")

set_source_files_properties("${APP_ICON_MACOS}"
    TARGET_DIRECTORY NotepadSharp
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
)

target_sources(NotepadSharp PRIVATE "${APP_ICON_MACOS}")

set_target_properties(NotepadSharp PROPERTIES
    MACOSX_BUNDLE_ICON_FILE NotepadSharp.icns
)

install(TARGETS NotepadSharp
    BUNDLE DESTINATION . COMPONENT Runtime
)
install(FILES
    "${CMAKE_SOURCE_DIR}/packaging/macos/README.txt"
    "${CMAKE_SOURCE_DIR}/packaging/macos/THIRD_PARTY_NOTICES.txt"
    "${CMAKE_BINARY_DIR}/packaging/SOURCE_INFO.txt"
    DESTINATION "${PACKAGE_RESOURCE_DIR}" COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION "${PACKAGE_RESOURCE_DIR}" RENAME LICENSE-GPL-3.0.txt COMPONENT Runtime
)
install(FILES "${NOTEPADSHARP_ADS_LICENSE}"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME Qt-Advanced-Docking-System-LGPL-2.1.txt COMPONENT Runtime
)
install(FILES "${NOTEPADSHARP_SINGLEAPPLICATION_LICENSE}"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME SingleApplication-MIT.txt COMPONENT Runtime
)
install(FILES "${NOTEPADSHARP_EDITORCONFIG_LICENSE}"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME editorconfig-core-qt-MIT.txt COMPONENT Runtime
)
install(FILES "${NOTEPADSHARP_JSONCONS_LICENSE}"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME jsoncons-Boost-1.0.txt COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/windows/LICENSE-Lua-5.3.4.txt"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME Lua-5.3.4-MIT.txt COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/thirdparty/scintilla/License.txt"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME Scintilla-License.txt COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/thirdparty/lexilla/License.txt"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME Lexilla-License.txt COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/thirdparty/uchardet/COPYING"
    DESTINATION "${PACKAGE_LICENSE_DIR}" RENAME uchardet-MPL-1.1-and-GPL-2.0.txt COMPONENT Runtime
)

add_custom_target(install_local
    DEPENDS NotepadSharp
    COMMENT "Staging the Notepad # macOS application and license notices"
    VERBATIM
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${INSTALL_DIR}"
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT_FILE=${CMAKE_BINARY_DIR}/packaging/SOURCE_INFO.txt"
        "-DTEMPLATE_FILE=${CMAKE_SOURCE_DIR}/packaging/macos/SOURCE_INFO.txt.in"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DPROJECT_VERSION=${PROJECT_VERSION}"
        "-DQT_VERSION=${Qt6_VERSION}"
        "-DCMAKE_GENERATOR_TEXT=${CMAKE_GENERATOR}"
        "-DCMAKE_BUILD_TYPE_TEXT=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_CXX_COMPILER_ID_TEXT=${CMAKE_CXX_COMPILER_ID}"
        "-DCMAKE_CXX_COMPILER_VERSION_TEXT=${CMAKE_CXX_COMPILER_VERSION}"
        "-DMACOS_PACKAGE_ARCH=${MACOS_PACKAGE_ARCH}"
        "-DMACOS_MINIMUM_VERSION=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "-DSOURCE_ARCHIVE_NAME=${SOURCE_ARCHIVE_NAME}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GeneratePackageMetadata.cmake"
    COMMAND "${CMAKE_COMMAND}"
        --install "${CMAKE_BINARY_DIR}"
        --prefix "${INSTALL_DIR}"
        --component Runtime
)

add_custom_target(dmg
    DEPENDS install_local
    COMMENT "Deploying Qt and creating the ad-hoc signed macOS disk image"
    VERBATIM
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DIR}"
    COMMAND "${MACDEPLOYQT_EXECUTABLE}"
        "${INSTALL_DIR}/NotepadSharp.app" -always-overwrite -codesign=- -dmg
    COMMAND "${CODESIGN_EXECUTABLE}" --verify --deep --strict "${INSTALL_DIR}/NotepadSharp.app"
    COMMAND "${LIPO_EXECUTABLE}" "${INSTALL_DIR}/NotepadSharp.app/Contents/MacOS/NotepadSharp"
        -verify_arch ${MACOS_PACKAGE_ARCHITECTURES}
    COMMAND "${CMAKE_COMMAND}" -E rename "${INSTALL_DIR}/NotepadSharp.dmg" "${DMG_FILE}"
)

add_custom_target(portable
    DEPENDS dmg
    COMMENT "Creating the Notepad # macOS portable archive"
    VERBATIM
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${PORTABLE_ZIP}"
    COMMAND "${DITTO_EXECUTABLE}" -c -k --sequesterRsrc --keepParent
        "${INSTALL_DIR}/NotepadSharp.app" "${PORTABLE_ZIP}"
)

add_custom_target(source_archive
    COMMENT "Creating the Notepad # macOS project-source snapshot"
    VERBATIM
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DIR}"
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT_ARCHIVE=${SOURCE_ZIP}"
        "-DSTAGE_PARENT=${CMAKE_BINARY_DIR}/source-stage"
        "-DSTAGE_NAME=${PACKAGE_NAME}-source"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DPROJECT_VERSION=${PROJECT_VERSION}"
        "-DUNTRACKED_ALLOWLIST=${CMAKE_SOURCE_DIR}/packaging/windows/source-untracked-allowlist.txt"
        -P "${CMAKE_SOURCE_DIR}/cmake/CreateSourceArchive.cmake"
)

add_custom_target(macos_bundle
    DEPENDS portable source_archive
    COMMENT "Writing the Notepad # macOS distribution checksum manifest"
    VERBATIM
    COMMAND "${CMAKE_COMMAND}"
        "-DINSTALLER_FILE=${DMG_FILE}"
        "-DSOURCE_FILE=${SOURCE_ZIP}"
        "-DPORTABLE_FILE=${PORTABLE_ZIP}"
        "-DOUTPUT_FILE=${ARTIFACT_DIR}/SHA256SUMS-macOS-${MACOS_PACKAGE_ARCH}.txt"
        -P "${CMAKE_SOURCE_DIR}/cmake/WriteArtifactChecksums.cmake"
)
