qt_add_executable(CompareEngineTests
    CompareEngineTests.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareEngine.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareTypes.h
)

target_include_directories(CompareEngineTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/compare
)

target_link_libraries(CompareEngineTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME CompareEngineTests COMMAND CompareEngineTests)
set_tests_properties(CompareEngineTests PROPERTIES TIMEOUT 90)

qt_add_executable(CompareSessionTests
    CompareSessionTests.cpp
    ${CMAKE_SOURCE_DIR}/src/DockedEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/DockedEditor.h
    ${CMAKE_SOURCE_DIR}/src/DockedEditorTitleBar.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareEngine.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareNavigator.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareNavigator.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareOverlay.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareOverlay.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareSession.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareSession.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareToolBar.cpp
    ${CMAKE_SOURCE_DIR}/src/compare/CompareToolBar.h
    ${CMAKE_SOURCE_DIR}/src/compare/CompareTypes.h
    ${CMAKE_SOURCE_DIR}/src/resources.qrc
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.h
    ${CMAKE_SOURCE_DIR}/src/RangeAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/RangeAllocator.h
    ${CMAKE_SOURCE_DIR}/src/ScintillaCommenter.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaCommenter.h
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.h
    ${CMAKE_SOURCE_DIR}/src/SelectionTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/SelectionTracker.h
    ${CMAKE_SOURCE_DIR}/src/UndoAction.cpp
    ${CMAKE_SOURCE_DIR}/src/UndoAction.h
)

target_include_directories(CompareSessionTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/compare
)

target_link_libraries(CompareSessionTests
    PRIVATE
        Qt6::Core
        Qt6::Core5Compat
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        qtadvanceddocking-qt6
        scintilla
        uchardet
)

add_test(NAME CompareSessionTests COMMAND CompareSessionTests)
    set_tests_properties(CompareSessionTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )