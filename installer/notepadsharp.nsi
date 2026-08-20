!ifndef PACKAGE_DIR
    !error "PACKAGE_DIR must point to the deployed Notepad# package directory"
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE must be the installer output path"
!endif
!ifndef PRODUCT_ICON
    !error "PRODUCT_ICON must point to a Windows .ico file"
!endif

!addplugindir /x86-unicode "${__FILEDIR__}\NsisMultiUser\Plugins\x86-unicode"
!addincludedir "${__FILEDIR__}\NsisMultiUser\Include"

Unicode true
SetCompressor /SOLID lzma
CRCCheck on
XPStyle on

!getdllversion "${PACKAGE_DIR}\NotepadSharp.exe" appver_
!ifndef PRODUCT_VERSION
    !define PRODUCT_VERSION "${appver_1}.${appver_2}.${appver_3}"
!endif
!define VERSION "${PRODUCT_VERSION}"
!define PRODUCT_NAME "Notepad#"
!define COMPANY_NAME "Notepad# Contributors"
!define PROGEXE "NotepadSharp.exe"
!define APP_MUTEX "NotepadSharpMutex"
!define UNINSTALL_FILENAME "Uninstall-NotepadSharp.exe"
!define MULTIUSER_INSTALLMODE_UNINSTALL_REGISTRY_KEY "NotepadSharp"
!define MULTIUSER_INSTALLMODE_DISPLAYNAME "${PRODUCT_NAME} ${VERSION}"
!define MULTIUSER_INSTALLMODE_INSTDIR "NotepadSharp"
!define MULTIUSER_INSTALLMODE_64_BIT 1
!define MULTIUSER_INSTALLMODE_ALLOW_BOTH_INSTALLATIONS 0
!define MULTIUSER_INSTALLMODE_ALLOW_ELEVATION 1
!define MULTIUSER_INSTALLMODE_ALLOW_ELEVATION_IF_SILENT 1
!define MULTIUSER_INSTALLMODE_DEFAULT_ALLUSERS 0
!define MULTIUSER_INSTALLMODE_DEFAULT_CURRENTUSER 1
!define URL_INFO_ABOUT "https://github.com/dail8859/NotepadNext"
!define COMMENTS "Unsigned Notepad# Windows x64 internal test build"

!define MUI_ABORTWARNING
!define MUI_ICON "${PRODUCT_ICON}"
!define MUI_UNICON "${PRODUCT_ICON}"
!define MUI_COMPONENTSPAGE_NODESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\NotepadSharp.exe"

!include "NsisMultiUser.nsh"
!include "NsisMultiUserLang.nsh"
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "utils.nsh"

!insertmacro CheckIfRunning ""
!insertmacro CheckIfRunning "un."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PACKAGE_DIR}\LICENSE-GPL-3.0.txt"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!define MUI_PAGE_CUSTOMFUNCTION_SHOW "CheckIfRunning"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MULTIUSER_UNPAGE_INSTALLMODE
!define MUI_PAGE_CUSTOMFUNCTION_SHOW "un.CheckIfRunning"
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MULTIUSER_LANGUAGE_INIT

Name "Notepad# ${VERSION}"
OutFile "${OUTPUT_FILE}"
ShowInstDetails show
ShowUninstDetails show
BrandingText "Unsigned internal test build"

VIProductVersion "${appver_1}.${appver_2}.${appver_3}.${appver_4}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Notepad#"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Notepad# Contributors"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Notepad# ${VERSION} x64 Internal Test Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Notepad# Contributors"

Function .onInit
    ${ifnot} ${UAC_IsInnerInstance}
        !insertmacro CheckSingleInstance "Setup" "Global" "NotepadSharpSetupMutex"
    ${endif}
    !insertmacro MULTIUSER_INIT
FunctionEnd

Function un.onInit
    !insertmacro MULTIUSER_UNINIT
FunctionEnd

Function .onVerifyInstDir
    IfFileExists "$INSTDIR\${UNINSTALL_FILENAME}" valid
    IfFileExists "$INSTDIR\*.*" checkEmpty valid

    checkEmpty:
    Push "$INSTDIR"
    Call isEmptyDir
    Pop $0
    StrCmp $0 1 valid
    Abort

    valid:
FunctionEnd

Function EnsureSafeInstallDirectory
    SetRegView 64
    ReadRegStr $1 SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\NotepadSharp" "InstallLocation"
    StrCmp $1 "$INSTDIR" 0 checkEmpty
    IfFileExists "$INSTDIR\${UNINSTALL_FILENAME}" valid checkEmpty

    checkEmpty:
    IfFileExists "$INSTDIR\*.*" checkExisting valid

    checkExisting:
    Push "$INSTDIR"
    Call isEmptyDir
    Pop $0
    StrCmp $0 1 valid
    IfSilent invalid
    MessageBox MB_ICONSTOP "Choose an empty directory or the existing Notepad# installation directory."

    invalid:
    SetErrorLevel 3
    Quit

    valid:
FunctionEnd

Section "-Remove previous Notepad# installation" SEC_REMOVE_PREVIOUS
    Call EnsureSafeInstallDirectory
    Call CheckIfRunning
    SetRegView 64
    ReadRegStr $R0 SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\NotepadSharp" "QuietUninstallString"
    ${If} $R0 != ""
        DetailPrint "Removing the previous Notepad# installation..."
        ExecWait '$R0' $R1
        ${If} $R1 != 0
            MessageBox MB_ICONSTOP "The previous Notepad# installation could not be removed (exit code $R1)."
            Abort
        ${EndIf}
    ${EndIf}
SectionEnd

Section "Notepad#" SEC_CORE
    SectionIn RO
    SetRegView 64

    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File /oname=vc_redist.x64.exe "${PACKAGE_DIR}\vc_redist.x64.exe"
    DetailPrint "Checking the Microsoft Visual C++ runtime..."
    ExecWait '"$PLUGINSDIR\vc_redist.x64.exe" /install /quiet /norestart' $R0
    ${If} $R0 == 3010
        SetRebootFlag true
    ${ElseIf} $R0 != 0
    ${AndIf} $R0 != 1638
        MessageBox MB_ICONSTOP "The Microsoft Visual C++ runtime could not be installed (exit code $R0)."
        Abort
    ${EndIf}

    SetOutPath "$INSTDIR"
    CreateDirectory "$INSTDIR"
    File /r "${PACKAGE_DIR}\*"

    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\App Paths\NotepadSharp.exe" "" "$INSTDIR\NotepadSharp.exe"
    WriteRegStr SHCTX "Software\Classes\Applications\NotepadSharp.exe\shell\open\command" "" '"$INSTDIR\NotepadSharp.exe" "%1"'
    WriteRegStr SHCTX "Software\Classes\Applications\NotepadSharp.exe\shell\edit\command" "" '"$INSTDIR\NotepadSharp.exe" "%1"'
    WriteRegStr SHCTX "Software\Classes\SystemFileAssociations\text\OpenWithList\NotepadSharp.exe" "" ""

    WriteUninstaller "$INSTDIR\${UNINSTALL_FILENAME}"
    !insertmacro MULTIUSER_RegistryAddInstallInfo

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    ${IfNot} ${Errors}
        WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\NotepadSharp" "EstimatedSize" "$0"
    ${EndIf}
SectionEnd

Section "Start Menu shortcut" SEC_START_MENU
    CreateDirectory "$SMPROGRAMS\Notepad#"
    CreateShortcut "$SMPROGRAMS\Notepad#\Notepad#.lnk" "$INSTDIR\NotepadSharp.exe"
    CreateShortcut "$SMPROGRAMS\Notepad#\Uninstall Notepad#.lnk" "$INSTDIR\${UNINSTALL_FILENAME}"
SectionEnd

Section /o "Desktop shortcut" SEC_DESKTOP
    CreateShortcut "$DESKTOP\Notepad#.lnk" "$INSTDIR\NotepadSharp.exe"
SectionEnd

Section /o "Explorer context menu" SEC_CONTEXT_MENU
    SetRegView 64
    WriteRegStr SHCTX "Software\Classes\*\shell\NotepadSharp" "" "Edit with Notepad#"
    WriteRegStr SHCTX "Software\Classes\*\shell\NotepadSharp" "Icon" "$INSTDIR\NotepadSharp.exe"
    WriteRegStr SHCTX "Software\Classes\*\shell\NotepadSharp\command" "" '"$INSTDIR\NotepadSharp.exe" "%1"'
SectionEnd

Section "Uninstall"
    Call un.CheckIfRunning
    SetRegView 64
    Delete "$DESKTOP\Notepad#.lnk"
    RMDir /r "$SMPROGRAMS\Notepad#"
    DeleteRegKey SHCTX "Software\Classes\*\shell\NotepadSharp"
    DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\App Paths\NotepadSharp.exe"
    DeleteRegKey SHCTX "Software\Classes\Applications\NotepadSharp.exe"
    DeleteRegKey SHCTX "Software\Classes\SystemFileAssociations\text\OpenWithList\NotepadSharp.exe"
    !insertmacro MULTIUSER_RegistryRemoveInstallInfo
    RMDir /r "$INSTDIR"
SectionEnd