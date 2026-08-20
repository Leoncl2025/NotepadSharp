foreach(required_variable SOURCE_DIR OUTPUT_ARCHIVE STAGE_PARENT STAGE_NAME GIT_EXECUTABLE UNTRACKED_ALLOWLIST)
	if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
		message(FATAL_ERROR "${required_variable} is required")
	endif()
endforeach()
if(NOT EXISTS "${UNTRACKED_ALLOWLIST}")
	message(FATAL_ERROR "Untracked source allowlist does not exist: ${UNTRACKED_ALLOWLIST}")
endif()

set(stage_dir "${STAGE_PARENT}/${STAGE_NAME}")
file(REMOVE_RECURSE "${STAGE_PARENT}")
file(MAKE_DIRECTORY "${stage_dir}")

execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
	OUTPUT_VARIABLE git_head
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" ls-files --cached
	OUTPUT_VARIABLE tracked_file_output
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" ls-files --others --exclude-standard
	OUTPUT_VARIABLE untracked_file_output
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)

file(STRINGS "${UNTRACKED_ALLOWLIST}" untracked_allowlist ENCODING UTF-8)
set(reviewed_untracked_files)
set(unreviewed_untracked_files)
string(REPLACE "\r\n" "\n" untracked_file_output "${untracked_file_output}")
string(REPLACE "\n" ";" untracked_files "${untracked_file_output}")
foreach(relative_path IN LISTS untracked_files)
	if(relative_path STREQUAL "")
		continue()
	endif()

	list(FIND untracked_allowlist "${relative_path}" allowlist_index)
	if(allowlist_index EQUAL -1)
		list(APPEND unreviewed_untracked_files "${relative_path}")
	else()
		list(APPEND reviewed_untracked_files "${relative_path}")
	endif()
endforeach()

if(unreviewed_untracked_files)
	list(JOIN unreviewed_untracked_files "\n  " unreviewed_file_list)
	message(FATAL_ERROR
		"Source archive blocked by unreviewed untracked files:\n  ${unreviewed_file_list}\n"
		"Add intentional product source to ${UNTRACKED_ALLOWLIST}, or ignore/remove local files.")
endif()

string(REPLACE "\r\n" "\n" tracked_file_output "${tracked_file_output}")
string(REPLACE "\n" ";" tracked_files "${tracked_file_output}")
set(source_files ${tracked_files} ${reviewed_untracked_files})
list(REMOVE_DUPLICATES source_files)
list(SORT source_files)
set(snapshot_files)

foreach(relative_path IN LISTS source_files)
	if(relative_path STREQUAL "")
		continue()
	endif()

	set(source_path "${SOURCE_DIR}/${relative_path}")
	if(NOT EXISTS "${source_path}")
		continue()
	endif()
	if(IS_DIRECTORY "${source_path}")
		continue()
	endif()

	get_filename_component(relative_directory "${relative_path}" DIRECTORY)
	file(MAKE_DIRECTORY "${stage_dir}/${relative_directory}")
	file(COPY_FILE "${source_path}" "${stage_dir}/${relative_path}" ONLY_IF_DIFFERENT)
	list(APPEND snapshot_files "${relative_path}")
endforeach()

list(JOIN snapshot_files "\n" source_manifest)
file(WRITE "${stage_dir}/SOURCE_FILE_MANIFEST.txt" "${source_manifest}\n")

file(WRITE "${stage_dir}/SOURCE_SNAPSHOT_INFO.txt"
	"Notepad # project source snapshot\n"
	"Version: ${PROJECT_VERSION}\n"
	"Repository revision: ${git_head}\n"
	"Includes tracked working-tree files and reviewed allowlisted untracked files at package time.\n"
	"Generated output, local caches, and Git metadata are excluded.\n")

file(REMOVE "${OUTPUT_ARCHIVE}")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT_ARCHIVE}" --format=zip "${STAGE_NAME}"
	WORKING_DIRECTORY "${STAGE_PARENT}"
	COMMAND_ERROR_IS_FATAL ANY
)