qt_add_executable(JsonToolsTests
    JsonToolsTests.cpp
)
target_include_directories(JsonToolsTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(JsonToolsTests PRIVATE Qt6::Core Qt6::Test notepadsharp_json notepadsharp_jsoncons)
add_test(NAME JsonToolsTests COMMAND JsonToolsTests)
set_tests_properties(JsonToolsTests PROPERTIES TIMEOUT 30)

qt_add_executable(JsonWorkbenchTests
    JsonWorkbenchTests.cpp
    ${CMAKE_SOURCE_DIR}/src/resources.qrc
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/RangeAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaCommenter.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.h
    ${CMAKE_SOURCE_DIR}/src/SelectionTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/UndoAction.cpp
)
target_link_libraries(JsonWorkbenchTests PRIVATE
    notepadsharp_json_widgets Qt6::Test Qt6::Core5Compat uchardet
)
add_test(NAME JsonWorkbenchTests COMMAND JsonWorkbenchTests)
set_tests_properties(JsonWorkbenchTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)

qt_add_executable(JsonFormatterTests
    JsonFormatterTests.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.h
)

target_include_directories(JsonFormatterTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(JsonFormatterTests PRIVATE Qt6::Core Qt6::Test)

add_test(NAME JsonFormatterTests COMMAND JsonFormatterTests)
set_tests_properties(JsonFormatterTests PROPERTIES TIMEOUT 30)

qt_add_executable(JsonEditorTests
    JsonEditorTests.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/RangeAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaCommenter.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.h
    ${CMAKE_SOURCE_DIR}/src/SelectionTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/UndoAction.cpp
)

target_include_directories(JsonEditorTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(JsonEditorTests PRIVATE
    Qt6::Core Qt6::Core5Compat Qt6::Gui Qt6::Test Qt6::Widgets scintilla uchardet
)

add_test(NAME JsonEditorTests COMMAND JsonEditorTests)
set_tests_properties(JsonEditorTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(AppearanceManagerTests
    AppearanceManagerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/AppearanceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/AppearanceManager.h
    ${CMAKE_SOURCE_DIR}/src/ApplicationSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/ApplicationSettings.h
    ${CMAKE_SOURCE_DIR}/src/WindowsAppearance.cpp
    ${CMAKE_SOURCE_DIR}/src/WindowsAppearance.h
)

target_include_directories(AppearanceManagerTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(AppearanceManagerTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(AppearanceManagerTests PRIVATE dwmapi)
endif()

add_test(NAME AppearanceManagerTests COMMAND AppearanceManagerTests)
set_tests_properties(AppearanceManagerTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(EditorAppearanceTests
    EditorAppearanceTests.cpp
    ${CMAKE_SOURCE_DIR}/src/AppearanceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/AppearanceManager.h
    ${CMAKE_SOURCE_DIR}/src/ApplicationSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/ApplicationSettings.h
    ${CMAKE_SOURCE_DIR}/src/EditorAppearance.cpp
    ${CMAKE_SOURCE_DIR}/src/EditorAppearance.h
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.h
    ${CMAKE_SOURCE_DIR}/src/WindowsAppearance.cpp
    ${CMAKE_SOURCE_DIR}/src/WindowsAppearance.h
)

target_include_directories(EditorAppearanceTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(EditorAppearanceTests PRIVATE
    Qt6::Core
    Qt6::Core5Compat
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    scintilla
)

if(WIN32)
    target_link_libraries(EditorAppearanceTests PRIVATE dwmapi)
endif()

add_test(NAME EditorAppearanceTests COMMAND EditorAppearanceTests)
set_tests_properties(EditorAppearanceTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

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
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonFormatter.h
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