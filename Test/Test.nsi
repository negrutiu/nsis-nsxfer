!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"

!define LOGICLIB_STRCMP
!include "LogicLib.nsh"


; NSdown path
!define NSDOWN "$EXEDIR\..\DebugW\NSdown.dll"


# The folder where NSdown.dll is
!ifdef NSIS_UNICODE
	!if /FileExists "..\ReleaseW\NSdown.dll"
		!AddPluginDir "..\ReleaseW"
	!else if /FileExists "..\DebugW\NSdown.dll"
		!AddPluginDir "..\DebugW"
	!else
		!error "NSdown.dll (Unicode) not found. Have you built it?"
	!endif
!else
	!if /FileExists "..\Release\NSdown.dll"
		!AddPluginDir "..\ReleaseA"
	!else if /FileExists "..\DebugA\NSdown.dll"
		!AddPluginDir "..\DebugA"
	!else
		!error "NSdown.dll (ANSI) not found. Have you built it?"
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
	Name "NSdownW"
	OutFile "NSdownW.exe"
!else
	Name "NSdownA"
	OutFile "NSdownA.exe"
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

!define LINK0 `https://nefertiti`
!define FILE0 "$EXEDIR\Nefertiti.html"

!define LINK1 `http://download.tuxfamily.org/notepadplus/6.5.3/npp.6.5.3.Installer.exe`
;!define FILE1 "$EXEDIR\npp.6.5.3.Installer.exe"
!define FILE1 "NONE"

!define LINK2 `http://www.piriform.com/ccleaner/download/slim/downloadfile`
;!define FILE2 "$EXEDIR\CCleanerSetup.exe"
!define FILE2 "MEMORY"

!define LINK3 `http://live.sysinternals.com/Files/SysinternalsSuite.zip`
!define FILE3 "$EXEDIR\SysinternalsSuiteLive.zip"

!define LINK4 `http://nefertiti.homenet.org:8008/SysinternalsSuite (May 13, 2014).zip`
!define FILE4 "$EXEDIR\SysinternalsSuite (May 13, 2014).zip"

Section "-Test"

	DetailPrint 'NSdown::Download "${LINK0}" "${FILE0}"'
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE0}"
	Push "/LOCAL"
	Push "${LINK0}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Download"
	;NSdown::Download /URL "${LINK0}" /LOCAL "${FILE0}" /RETRYCOUNT 3 /RETRYDELAY 5000
	Pop $0	; ItemID

	DetailPrint 'NSdown::Download "${LINK1}" "${FILE1}"'
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE1}"
	Push "/LOCAL"
	Push "${LINK1}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Download"
	;NSdown::Download /URL "${LINK1}" /LOCAL "${FILE1}" /RETRYCOUNT 3 /RETRYDELAY 5000
	Pop $0	; ItemID

	DetailPrint 'NSdown::Download "${LINK2}" "${FILE2}"'
	Push "${FILE2}"
	Push "/LOCAL"
	Push "${LINK2}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Download"
	;NSdown::Download /URL "${LINK2}" /LOCAL "${FILE2}"
	Pop $0	; ItemID

	DetailPrint 'NSdown::Download "${LINK3}" "${FILE3}"'
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE3}"
	Push "/LOCAL"
	Push "${LINK3}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Download"
	;NSdown::Download /URL "${LINK3}" /LOCAL "${FILE3}"
	Pop $0	; ItemID

	DetailPrint 'NSdown::Download "${LINK4}" "${FILE4}"'
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE4}"
	Push "/LOCAL"
	Push "${LINK4}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	CallInstDLL "${NSDOWN}" "Download"
	;NSdown::Download /URL "${LINK4}" /LOCAL "${FILE4}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000
	Pop $0	; ItemID


/*	; Wait a few seconds...
	StrCpy $6 4000		; Steps
	${For} $5 1 $6
		; $0 = HTTP status code, 0=Completed
		; $1 = Completed files
		; $2 = Remaining files
		; $3 = Number of downloaded bytes for the current file
		; $4 = Size of current file (Empty string if the size is unknown)
		CallInstDLL "${NSDOWN}" "GetStats"
		;NSdown::GetStats
		DetailPrint "Waiting($5/$6): HTTP status == $0, Completed files == $1, Remaining files == $2, Recv bytes (last file) == $3, Total bytes (last file) == $4"
		${If} $2 = 0		; No remaining requests
		;${OrIf} $0 >= 300	; HTTP error codes
			${Break}
		${EndIf}
		Sleep 500
	${Next} */

SectionEnd
