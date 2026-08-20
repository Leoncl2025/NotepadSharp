message(STATUS "Configuring macOS packaging")

set(INSTALL_DIR ${CMAKE_BINARY_DIR}/install)

# Custom Info.plist
set_target_properties(NotepadSharp PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/deploy/macos/info.plist
)

# Application icon
set(APP_ICON_MACOS ${CMAKE_SOURCE_DIR}/icon/NotepadSharp.icns)

set_source_files_properties(${APP_ICON_MACOS}
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
)

target_sources(NotepadSharp PRIVATE ${APP_ICON_MACOS})

set_target_properties(NotepadSharp PROPERTIES
    MACOSX_BUNDLE_ICON_FILE NotepadSharp.icns
)

install(TARGETS NotepadSharp
    BUNDLE DESTINATION .
)

install(FILES ${APP_ICON_MACOS}
    DESTINATION NotepadSharp.app/Contents/Resources
)

add_custom_target(install_local
    COMMAND ${CMAKE_COMMAND}
        --install ${CMAKE_BINARY_DIR}
        --prefix ${INSTALL_DIR}
    DEPENDS NotepadSharp
)

find_program(MACDEPLOYQT_EXECUTABLE macdeployqt REQUIRED)

add_custom_target(dmg
    COMMAND ${MACDEPLOYQT_EXECUTABLE}
        ${INSTALL_DIR}/NotepadSharp.app
        -dmg
    COMMAND ${CMAKE_COMMAND} -E rename
        ${INSTALL_DIR}/NotepadSharp.dmg
        ${CMAKE_BINARY_DIR}/NotepadSharp-v${PROJECT_VERSION}.dmg
    DEPENDS install_local
)
