foreach(required_file INSTALLER_FILE SOURCE_FILE PORTABLE_FILE)
	if(NOT EXISTS "${${required_file}}")
		message(FATAL_ERROR "Missing artifact: ${${required_file}}")
	endif()
endforeach()
if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
	message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

file(SHA256 "${INSTALLER_FILE}" installer_hash)
file(SHA256 "${SOURCE_FILE}" source_hash)
file(SHA256 "${PORTABLE_FILE}" portable_hash)
get_filename_component(installer_name "${INSTALLER_FILE}" NAME)
get_filename_component(source_name "${SOURCE_FILE}" NAME)
get_filename_component(portable_name "${PORTABLE_FILE}" NAME)

file(WRITE "${OUTPUT_FILE}"
	"${installer_hash}  ${installer_name}\n"
	"${source_hash}  ${source_name}\n"
	"${portable_hash}  ${portable_name}\n")