add_library(notepadsharp_json STATIC JsonIndex.cpp JsonOperations.cpp)
target_include_directories(notepadsharp_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(notepadsharp_json PUBLIC Qt6::Core PRIVATE notepadsharp_jsoncons)

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
add_library(notepadsharp_json_widgets STATIC
	JsonSession.cpp JsonSession.h JsonTreeModel.cpp JsonTreeModel.h JsonWorkbench.cpp JsonWorkbench.h
)
target_include_directories(notepadsharp_json_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(notepadsharp_json_widgets PUBLIC notepadsharp_json Qt6::Widgets Qt6::Concurrent scintilla)