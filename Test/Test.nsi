!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"

!define LOGICLIB_STRCMP
!include "LogicLib.nsh"

# The folder where NSbkdld.dll is
!ifdef NSIS_UNICODE
	!if /FileExists "..\ReleaseW\NSbkdld.dll"
		!AddPluginDir "..\ReleaseW"
	!else if /FileExists "..\DebugW\NSbkdld.dll"
		!AddPluginDir "..\DebugW"
	!else
		!error "NSbkdld.dll (Unicode) not found. Have you built it?"
	!endif
!else
	!if /FileExists "..\Release\NSbkdld.dll"
		!AddPluginDir "..\ReleaseA"
	!else if /FileExists "..\DebugA\NSbkdld.dll"
		!AddPluginDir "..\DebugA"
	!else
		!error "NSbkdld.dll (ANSI) not found. Have you built it?"
	!endif
!endif

# GUI settings
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install-nsis.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-nsis.bmp"

# Welcome page
;!define MUI_WELCOMEPAGE_TITLE_3LINES
;!insertmacro MUI_PAGE_WELCOME

# Components page
;!define MUI_COMPONENTSPAGE_NODESC
;!insertmacro MUI_PAGE_COMPONENTS

# Installation page
!insertmacro MUI_PAGE_INSTFILES

# Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

# Installer details
!ifdef NSIS_UNICODE
	Name "NSbkdldW"
	OutFile "NSbkdldW.exe"
!else
	Name "NSbkdldA"
	OutFile "NSbkdldA.exe"
!endif

XPStyle on
RequestExecutionLevel user ; don't require UAC elevation
ShowInstDetails show

#---------------------------------------------------------------#
# .onInit                                                       #
#---------------------------------------------------------------#
Function .onInit

	; Initializations
	InitPluginsDir
	
	; Language selection
	!define MUI_LANGDLL_ALLLANGUAGES
	!insertmacro MUI_LANGDLL_DISPLAY

FunctionEnd

!define LINK1 `http://download.tuxfamily.org/notepadplus/6.5.3/npp.6.5.3.Installer.exe`
!define FILE1 "$EXEDIR\npp.6.5.3.Installer.exe"

!define LINK2 `http://www.piriform.com/ccleaner/download/slim/downloadfile`
!define FILE2 "$EXEDIR\CCleanerSetup.exe"

!define LINK3 `http://live.sysinternals.com/Files/SysinternalsSuite.zip`
!define FILE3 "$EXEDIR\SysinternalsSuite.zip"

Section "-Test"
	DetailPrint 'NSbkdld::Download "${LINK1}" "${FILE1}"'
	Push "/END"
	Push "${FILE1}"
	Push "${LINK1}"
	CallInstDLL "$EXEDIR\..\DebugW\NSbkdld.dll" "Download"
	;NSbkdld::Download "${LINK1}" "${FILE1}" /END

	DetailPrint 'NSbkdld::Download "${LINK2}" "${FILE2}"'
	Push "/END"
	Push "${FILE2}"
	Push "${LINK2}"
	CallInstDLL "$EXEDIR\..\DebugW\NSbkdld.dll" "Download"
;	NSbkdld::Download "${LINK2}" "${FILE2}" /END

	DetailPrint 'NSbkdld::Download "${LINK3}" "${FILE3}"'
	Push "/END"
	Push "${FILE3}"
	Push "${LINK3}"
	CallInstDLL "$EXEDIR\..\DebugW\NSbkdld.dll" "Download"
;	NSbkdld::Download "${LINK3}" "${FILE3}" /END


/*	; Wait a few seconds...
	StrCpy $6 4000		; Steps
	${For} $5 1 $6
		; $0 = HTTP status code, 0=Completed
		; $1 = Completed files
		; $2 = Remaining files
		; $3 = Number of downloaded bytes for the current file
		; $4 = Size of current file (Empty string if the size is unknown)
		CallInstDLL "$EXEDIR\..\DebugW\NSbkdld.dll" "GetStats"
		;NSbkdld::GetStats
		DetailPrint "Waiting($5/$6): HTTP status == $0, Completed files == $1, Remaining files == $2, Recv bytes (last file) == $3, Total bytes (last file) == $4"
		${If} $2 = 0		; No remaining requests
		;${OrIf} $0 >= 300	; HTTP error codes
			${Break}
		${EndIf}
		Sleep 500
	${Next} */

SectionEnd
