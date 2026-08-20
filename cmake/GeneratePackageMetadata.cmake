foreach(required_variable SOURCE_DIR OUTPUT_FILE TEMPLATE_FILE GIT_EXECUTABLE PROJECT_VERSION QT_VERSION CMAKE_GENERATOR_TEXT CMAKE_BUILD_TYPE_TEXT CMAKE_CXX_COMPILER_ID_TEXT CMAKE_CXX_COMPILER_VERSION_TEXT)
	if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
		message(FATAL_ERROR "${required_variable} is required")
	endif()
endforeach()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
	OUTPUT_VARIABLE GIT_HEAD
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status --porcelain
	OUTPUT_VARIABLE git_status
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)

if(git_status STREQUAL "")
	set(GIT_WORKTREE_STATE "clean")
else()
	set(GIT_WORKTREE_STATE "modified; the project source ZIP captures tracked and reviewed allowlisted untracked files")
endif()

set(Qt6_VERSION "${QT_VERSION}")
set(CMAKE_GENERATOR "${CMAKE_GENERATOR_TEXT}")
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_TEXT}")
set(CMAKE_CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID_TEXT}")
set(CMAKE_CXX_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION_TEXT}")
get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}" @ONLY)