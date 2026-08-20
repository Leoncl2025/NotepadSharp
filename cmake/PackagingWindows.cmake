set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
set(ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(PACKAGE_LICENSE_DIR "${PACKAGE_DIR}/licenses")

get_filename_component(QT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
find_program(WINDEPLOYQT_EXECUTABLE
	NAMES windeployqt
	HINTS "${QT_ROOT}/bin"
	REQUIRED
)

find_package(Git REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/packaging")
configure_file(
	"${CMAKE_SOURCE_DIR}/packaging/windows/UNSIGNED_BUILD.txt.in"
	"${CMAKE_BINARY_DIR}/packaging/UNSIGNED_BUILD.txt"
	@ONLY
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(WINDEPLOYQT_BUILD_MODE --debug)
else()
	set(WINDEPLOYQT_BUILD_MODE --release)
endif()

add_custom_target(package
	DEPENDS NotepadSharp
	COMMENT "Deploying the unsigned Notepad # Windows x64 internal test package"
	VERBATIM
	COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PACKAGE_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACKAGE_LICENSE_DIR}"
	COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
		"-DOUTPUT_FILE=${CMAKE_BINARY_DIR}/packaging/SOURCE_INFO.txt"
		"-DTEMPLATE_FILE=${CMAKE_SOURCE_DIR}/packaging/windows/SOURCE_INFO.txt.in"
		"-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
		"-DPROJECT_VERSION=${PROJECT_VERSION}"
		"-DQT_VERSION=${Qt6_VERSION}"
		"-DCMAKE_GENERATOR_TEXT=${CMAKE_GENERATOR}"
		"-DCMAKE_BUILD_TYPE_TEXT=${CMAKE_BUILD_TYPE}"
		"-DCMAKE_CXX_COMPILER_ID_TEXT=${CMAKE_CXX_COMPILER_ID}"
		"-DCMAKE_CXX_COMPILER_VERSION_TEXT=${CMAKE_CXX_COMPILER_VERSION}"
		-P "${CMAKE_SOURCE_DIR}/cmake/GeneratePackageMetadata.cmake"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"$<TARGET_FILE:NotepadSharp>" "${PACKAGE_DIR}/NotepadSharp.exe"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/LICENSE" "${PACKAGE_DIR}/LICENSE-GPL-3.0.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/packaging/windows/README.txt" "${PACKAGE_DIR}/README.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_BINARY_DIR}/packaging/SOURCE_INFO.txt" "${PACKAGE_DIR}/SOURCE_INFO.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_BINARY_DIR}/packaging/UNSIGNED_BUILD.txt" "${PACKAGE_DIR}/UNSIGNED_BUILD.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/packaging/windows/THIRD_PARTY_NOTICES.txt" "${PACKAGE_DIR}/THIRD_PARTY_NOTICES.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${NOTEPADSHARP_ADS_LICENSE}" "${PACKAGE_LICENSE_DIR}/Qt-Advanced-Docking-System-LGPL-2.1.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${NOTEPADSHARP_SINGLEAPPLICATION_LICENSE}" "${PACKAGE_LICENSE_DIR}/SingleApplication-MIT.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${NOTEPADSHARP_EDITORCONFIG_LICENSE}" "${PACKAGE_LICENSE_DIR}/editorconfig-core-qt-MIT.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/packaging/windows/LICENSE-Lua-5.3.4.txt" "${PACKAGE_LICENSE_DIR}/Lua-5.3.4-MIT.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/thirdparty/scintilla/License.txt" "${PACKAGE_LICENSE_DIR}/Scintilla-License.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/thirdparty/lexilla/License.txt" "${PACKAGE_LICENSE_DIR}/Lexilla-License.txt"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_SOURCE_DIR}/thirdparty/uchardet/COPYING" "${PACKAGE_LICENSE_DIR}/uchardet-MPL-1.1-and-GPL-2.0.txt"
	COMMAND "${WINDEPLOYQT_EXECUTABLE}"
		${WINDEPLOYQT_BUILD_MODE}
		--no-translations
		--no-system-d3d-compiler
		--no-opengl-sw
		--compiler-runtime
		--dir "${PACKAGE_DIR}"
		"$<TARGET_FILE:NotepadSharp>"
)

set(PORTABLE_ZIP "${ARTIFACT_DIR}/NotepadSharp-v${PROJECT_VERSION}-x64-Unsigned-Portable.zip")
add_custom_target(portable
	DEPENDS package
	COMMENT "Creating the Notepad # unsigned Windows x64 portable archive"
	VERBATIM
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E rm -f "${PORTABLE_ZIP}"
	COMMAND "${CMAKE_COMMAND}" -E tar cf "${PORTABLE_ZIP}" --format=zip .
	WORKING_DIRECTORY "${PACKAGE_DIR}"
)

set(SOURCE_ZIP "${ARTIFACT_DIR}/NotepadSharp-v${PROJECT_VERSION}-source.zip")
add_custom_target(source_archive
	COMMENT "Creating the Notepad # project-source snapshot"
	VERBATIM
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DIR}"
	COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
		"-DOUTPUT_ARCHIVE=${SOURCE_ZIP}"
		"-DSTAGE_PARENT=${CMAKE_BINARY_DIR}/source-stage"
		"-DSTAGE_NAME=NotepadSharp-v${PROJECT_VERSION}-source"
		"-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
		"-DPROJECT_VERSION=${PROJECT_VERSION}"
		"-DUNTRACKED_ALLOWLIST=${CMAKE_SOURCE_DIR}/packaging/windows/source-untracked-allowlist.txt"
		-P "${CMAKE_SOURCE_DIR}/cmake/CreateSourceArchive.cmake"
)

set(NSIS_MAKENSIS "" CACHE FILEPATH "Path to portable or installed NSIS makensis.exe")
if(NOT NSIS_MAKENSIS)
	find_program(NSIS_MAKENSIS_DISCOVERED NAMES makensis)
	if(NSIS_MAKENSIS_DISCOVERED)
		set(NSIS_MAKENSIS "${NSIS_MAKENSIS_DISCOVERED}" CACHE FILEPATH "Path to portable or installed NSIS makensis.exe" FORCE)
	endif()
endif()
set(INSTALLER_FILE "${ARTIFACT_DIR}/NotepadSharp-v${PROJECT_VERSION}-x64-Unsigned-Setup.exe")
if(NSIS_MAKENSIS)
	add_custom_target(installer
		DEPENDS package
		COMMENT "Building the unsigned Notepad # Windows x64 NSIS installer"
		VERBATIM
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E rm -f "${INSTALLER_FILE}"
		COMMAND "${NSIS_MAKENSIS}"
			/V3
			"/DPACKAGE_DIR=${PACKAGE_DIR}"
			"/DOUTPUT_FILE=${INSTALLER_FILE}"
			"/DPRODUCT_ICON=${CMAKE_SOURCE_DIR}/icon/NotepadSharp.ico"
			"/DPRODUCT_VERSION=${PROJECT_VERSION}"
			"${CMAKE_SOURCE_DIR}/installer/notepadsharp.nsi"
	)

	set(CHECKSUM_FILE "${ARTIFACT_DIR}/SHA256SUMS.txt")
	add_custom_target(installer_bundle
		DEPENDS installer source_archive portable
		COMMENT "Writing the Notepad # internal distribution checksum manifest"
		VERBATIM
		COMMAND "${CMAKE_COMMAND}"
			"-DINSTALLER_FILE=${INSTALLER_FILE}"
			"-DSOURCE_FILE=${SOURCE_ZIP}"
			"-DPORTABLE_FILE=${PORTABLE_ZIP}"
			"-DOUTPUT_FILE=${CHECKSUM_FILE}"
			-P "${CMAKE_SOURCE_DIR}/cmake/WriteArtifactChecksums.cmake"
	)
else()
	message(STATUS "NSIS_MAKENSIS is not set; installer and installer_bundle targets are disabled")
endif()
