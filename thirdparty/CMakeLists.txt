include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME ads
    GITHUB_REPOSITORY githubuser0xFFFF/Qt-Advanced-Docking-System
    GIT_TAG 5.0.0
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_EXAMPLES OFF"
)

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication" FORCE)
CPMAddPackage(
    NAME SingleApplication
    GITHUB_REPOSITORY itay-grudev/SingleApplication
    GIT_TAG v3.5.6
)

CPMAddPackage(
    NAME editorconfig_core_qt
    GITHUB_REPOSITORY editorconfig/editorconfig-core-qt
    GIT_TAG ab62f0554abf2bbe4d45427b36a8b2f81ca7b4ab
)

set(NOTEPADSHARP_ADS_LICENSE "${ads_SOURCE_DIR}/LICENSE" PARENT_SCOPE)
set(NOTEPADSHARP_SINGLEAPPLICATION_LICENSE "${SingleApplication_SOURCE_DIR}/LICENSE" PARENT_SCOPE)
set(NOTEPADSHARP_EDITORCONFIG_LICENSE "${editorconfig_core_qt_SOURCE_DIR}/LICENSE" PARENT_SCOPE)

add_subdirectory(lua)
add_subdirectory(scintilla)
add_subdirectory(lexilla)
add_subdirectory(uchardet)

target_link_libraries(lexilla PRIVATE scintilla)
