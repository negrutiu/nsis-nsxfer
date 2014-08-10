!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"

!define LOGICLIB_STRCMP
!include "LogicLib.nsh"


; NSdown path
!ifdef NSIS_UNICODE
	!define NSDOWN "$EXEDIR\..\DebugW\NSdown.dll"
!else
	!define NSDOWN "$EXEDIR\..\DebugA\NSdown.dll"
!endif


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
!define FILE0 "$EXEDIR\_Nefertiti.html"

!define LINK1 `http://download.tuxfamily.org/notepadplus/6.6.7/npp.6.6.7.Installer.exe`
!define FILE1 "$EXEDIR\_npp.6.6.7.Installer.exe"
;!define FILE1 "NONE"

!define LINK2 `http://www.piriform.com/ccleaner/download/slim/downloadfile`
;!define FILE2 "$EXEDIR\CCleanerSetup.exe"
!define FILE2 "MEMORY"

!define LINK3 `http://live.sysinternals.com/Files/SysinternalsSuite.zip`
!define FILE3 "$EXEDIR\_SysinternalsSuiteLive.zip"

!define LINK4 `http://nefertiti.homenet.org:8008/SysinternalsSuite (May 13, 2014).zip`
!define FILE4 "$EXEDIR\_SysinternalsSuite (May 13, 2014).zip"

!define LINK5 `http://httpbin.org/post`
!define FILE5 "$EXEDIR\_Post1.txt"

!define LINK6 `http://nefertiti.homenet.org:8008/Priest.mkv`
!define FILE6 "$EXEDIR\_Priest.mkv"

Section "-Test"

	DetailPrint 'NSdown::Transfer "${LINK0}" "${FILE0}"'
	Push "0x2080|0x100"
	Push "/SECURITYFLAGS"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE0}"
	Push "/LOCAL"
	Push "${LINK0}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK0}" /LOCAL "${FILE0}" /RETRYCOUNT 3 /RETRYDELAY 5000
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK1}" "${FILE1}"'
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE1}"
	Push "/LOCAL"
	Push "${LINK1}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK1}" /LOCAL "${FILE1}" /RETRYCOUNT 3 /RETRYDELAY 5000
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK2}" "${FILE2}"'
	Push "${FILE2}"
	Push "/LOCAL"
	Push "${LINK2}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK2}" /LOCAL "${FILE2}"
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK3}" "${FILE3}"'
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE3}"
	Push "/LOCAL"
	Push "${LINK3}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK3}" /LOCAL "${FILE3}"
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK4}" "${FILE4}"'
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
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK4}" /LOCAL "${FILE4}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK5}" "${FILE5}"'
	Push "${LINK5}"
	Push "/REFERER"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "user=My+User+Name&pass=My+Password"
	Push "/DATA"
	;Push "$EXEDIR\buildW.bat"
	;Push "/DATAFILE"
	Push "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST"
	Push "/HEADERS"
	Push "${FILE5}"
	Push "/LOCAL"
	Push "${LINK5}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /METHOD POST /URL "${LINK5}" /LOCAL "${FILE5}" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK5}"
	Pop $0	; ItemID

/*
	DetailPrint 'NSdown::Transfer "${LINK6}" "${FILE6}"'
	Push "10000"
	Push "/TIMEOUTRECONNECT"
	Push "${FILE6}"
	Push "/LOCAL"
	Push "${LINK6}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /METHOD POST /URL "${LINK6}" /LOCAL "${FILE6}" /TIMEOUTRECONNECT 60000
	Pop $0	; ItemID
*/

SectionEnd


Section Wait
_loop:
	CallInstDLL "${NSDOWN}" "QueryGlobal"
	;NSdown::QueryGlobal
	Pop $R0 ; Worker threads
	Pop $R1 ; ItemsTotal
	Pop $R2 ; ItemsDone
	Pop $R3 ; ItemsDownloading
	Pop $R4 ; ItemsWaiting
	Pop $R5 ; Speed bps
!ifdef NSIS_UNICODE
	System::Call 'shlwapi::StrFormatByteSizeW( l r15, t .r16, i ${NSIS_MAX_STRLEN} ) p'
!else
	System::Call 'shlwapi::StrFormatByteSizeA( i r15, t .r16, i ${NSIS_MAX_STRLEN} ) p'
!endif
	DetailPrint "    Transferring $R2+$R3/$R1 items at $R6/s by $R0 worker threads"

	; Loop until ItemsTotal == ItemsDone
	IntCmp $R1 $R2 _done +1 +1
		Sleep 1000
		Goto _loop
_done:
SectionEnd


Section Enum
	!define ENUM_STATUS "all"

	DetailPrint "NSdown::Enumerate ${ENUM_STATUS}"
	Push "${ENUM_STATUS}"
	CallInstDLL "${NSDOWN}" "Enumerate"
	;NSdown::Enumerate ${ENUM_STATUS}

	Pop $1	; Count
	${For} $0 1 $1
		Pop $2	; Transfer ID
		DetailPrint "    $0. Transfer ID = $2"
	${Next}
SectionEnd