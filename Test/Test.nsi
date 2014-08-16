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

!macro STACK_VERIFY_START
	Push 666
!macroend

!macro STACK_VERIFY_END
	Pop $R9
	IntCmp $R9 666 +2 +1 +1
		MessageBox MB_ICONSTOP "Stack is NOT OK"
!macroend

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


Function PrintStatus

	!insertmacro STACK_VERIFY_START
	Push $0
	Push $1
	Push $2
	Push $3
	Push $R0
	Push $R1
	Push $R2
	Push $R3
	Push $R4

	!define ENUM_STATUS "all"

	DetailPrint "NSdown::Enumerate ${ENUM_STATUS}"
	Push "${ENUM_STATUS}"
	CallInstDLL "${NSDOWN}" "Enumerate"
	;NSdown::Enumerate ${ENUM_STATUS}

	Pop $1	; Count
	${For} $0 1 $1

		Pop $2	; Transfer ID

		Push "/END"
		Push "/ERRORTEXT"
		Push "/ERRORCODE"
		Push "/TIMEDOWNLOADING"
		Push "/TIMEWAITING"
		Push "/SPEED"
		Push "/SPEEDBYTES"
		Push "/PERCENT"
		Push "/FILESIZE"
		Push "/RECVSIZE"
		Push "/RECVHEADERS"
		Push "/SENTHEADERS"
		Push "/LOCAL"
		Push "/PROXY"
		Push "/IP"
		Push "/URL"
		Push "/METHOD"
		Push "/WININETSTATUS"
		Push "/STATUS"
		Push $2	; Transfer ID
		CallInstDLL "${NSDOWN}" "Query"
		;NSdown::Query $2 /STATUS ... /END

		StrCpy $R0 "    ID:$2"
		Pop $3 ;STATUS
		StrCpy $R0 "$R0 [$3]"
		Pop $3 ;WININETSTATUS
		;StrCpy $R0 "$R0 [$3]"
		Pop $3 ;METHOD
		StrCpy $R0 "$R0 $3"
		Pop $3 ;URL
		StrCpy $R0 "$R0 $3"
		Pop $3 ;IP
		StrCmp $3 "" +2 +1
			StrCpy $R0 "$R0 [$3]"
		Pop $3 ;PROXY
		StrCmp $3 "" +2 +1
			StrCpy $R0 "$R0 [Proxy:$3]"
		Pop $3 ;LOCAL
		;StrCpy $R0 "$R0 -> $3"
		Pop $3 ;SENTHEADERS
		;StrCpy $R0 "$R0 [$3]"
		Pop $3 ;RECVHEADERS
		;StrCpy $R0 "$R0 [$3]"
		Pop $3 ;RECVSIZE
		StrCpy $R0 "$R0 $3"
		Pop $3 ;FILESIZE
		StrCpy $R0 "$R0/$3"
		Pop $3 ;PERCENT
		StrCpy $R0 "$R0($3%)"
		Pop $3 ;SPEEDBYTES
		;StrCpy $R0 "$R0 [$3]"
		Pop $3 ;SPEED
		StrCpy $R0 "$R0 @ $3"
		Pop $3 ;TIMEWAITING
		StrCpy $R0 "$R0 (wait:$3ms"
		Pop $3 ;TIMEDOWNLOADING
		StrCpy $R0 "$R0, dnld:$3ms)"
		Pop $3 ;ERRORCODE
		StrCpy $R0 "$R0 = $3"
		Pop $3 ;ERRORTEXT
		StrCpy $R0 '$R0 "$3"'

		DetailPrint $R0
	${Next}

	Push "/END"
	Push "/COUNTTHREADS"
	Push "/SPEED"
	Push "/COUNTDOWNLOADING"
	Push "/COUNTCOMPLETED"
	Push "/COUNTTOTAL"
	CallInstDLL "${NSDOWN}" "QueryGlobal"
	;NSdown::QueryGlobal /COUNTTOTAL /COUNTCOMPLETED /COUNTDOWNLOADING /SPEED /COUNTTHREADS /END
	Pop $R0 ; Total
	Pop $R1 ; Completed
	Pop $R2 ; Downloading
	Pop $R3 ; Speed
	Pop $R4 ; Worker threads

	DetailPrint "Transferring $R1+$R2/$R0 items at $R3 using $R4 worker threads"
	
	Pop $R4
	Pop $R3
	Pop $R2
	Pop $R1
	Pop $R0
	Pop $3
	Pop $2
	Pop $1
	Pop $0
	!insertmacro STACK_VERIFY_END

FunctionEnd


!define LINK0 `https://nefertiti`
!define FILE0 "$EXEDIR\_Nefertiti.html"

!define LINK1 `http://download.tuxfamily.org/notepadplus/6.6.7/npp.6.6.7.Installer.exe`
!define FILE1 "$EXEDIR\_npp.6.6.7.Installer.exe"
;!define FILE1 "NONE"

!define LINK2 `http://www.piriform.com/ccleaner/download/slim/downloadfile`
;!define FILE2 "$EXEDIR\CCleanerSetup.exe"
!define FILE2 "MEMORY"

!define LINK3       "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
!define FILE3       "$EXEDIR\_SysinternalsSuiteLive.zip"
!define FILE3_PROXY "$EXEDIR\_SysinternalsSuiteLive_proxy.zip"

!define LINK4 `http://nefertiti.homenet.org:8008/SysinternalsSuite (August 5, 2014).zip`
!define FILE4 "$EXEDIR\_SysinternalsSuite (August 5, 2014).zip"

!define LINK5 `http://httpbin.org/post`
!define FILE5 "$EXEDIR\_Post1.txt"

!define LINK6 `http://nefertiti.homenet.org:8008/Priest.mkv`
!define FILE6 "$EXEDIR\_Priest.mkv"

Section "-Test"
	!insertmacro STACK_VERIFY_START

	DetailPrint 'NSdown::Transfer "${LINK0}" "${FILE0}"'
	Push "/END"
	Push "0x2080|0x100"
	Push "/SECURITYFLAGS"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE0}"
	Push "/LOCAL"
	Push "${LINK0}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK0}" /LOCAL "${FILE0}" /RETRYCOUNT 3 /RETRYDELAY 5000 /END
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK1}" "${FILE1}"'
	Push "/END"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE1}"
	Push "/LOCAL"
	Push "${LINK1}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK1}" /LOCAL "${FILE1}" /RETRYCOUNT 3 /RETRYDELAY 5000 /END
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK2}" "${FILE2}"'
	Push "/END"
	Push "${FILE2}"
	Push "/LOCAL"
	Push "${LINK2}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK2}" /LOCAL "${FILE2}" /END
	Pop $0	; ItemID

	!define PROXY3 "http=http://176.9.52.230:3128"
	DetailPrint 'NSdown::Transfer /proxy ${PROXY3} "${LINK3}" "${FILE3_PROXY}"'
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${PROXY3}"
	Push "/PROXY"
	Push "${FILE3_PROXY}"
	Push "/LOCAL"
	Push "${LINK3}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK3}" /LOCAL "${FILE3} /PROXY "${PROXY3}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK3}" "${FILE3}"'
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE3}"
	Push "/LOCAL"
	Push "${LINK3}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /URL "${LINK3}" /LOCAL "${FILE3}" /END
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK4}" "${FILE4}"'
	Push "/END"
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
	;NSdown::Transfer /URL "${LINK4}" /LOCAL "${FILE4}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
	Pop $0	; ItemID

	DetailPrint 'NSdown::Transfer "${LINK5}" "${FILE5}"'
	Push "/END"
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
	;NSdown::Transfer /METHOD POST /URL "${LINK5}" /LOCAL "${FILE5}" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK5}" /END
	Pop $0	; ItemID

/*
	DetailPrint 'NSdown::Transfer "${LINK6}" "${FILE6}"'
	Push "/END"
	Push "10000"
	Push "/TIMEOUTRECONNECT"
	Push "${FILE6}"
	Push "/LOCAL"
	Push "${LINK6}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	CallInstDLL "${NSDOWN}" "Transfer"
	;NSdown::Transfer /METHOD POST /URL "${LINK6}" /LOCAL "${FILE6}" /TIMEOUTRECONNECT 60000 /END
	Pop $0	; ItemID
*/

	Call PrintStatus

	!insertmacro STACK_VERIFY_END
SectionEnd


Section Wait
	!insertmacro STACK_VERIFY_START
_loop:
	Call PrintStatus

	Push "/END"
	Push "/COUNTCOMPLETED"
	Push "/COUNTTOTAL"
	CallInstDLL "${NSDOWN}" "QueryGlobal"
	;NSdown::QueryGlobal /COUNTTOTAL /COUNTCOMPLETED /END
	Pop $R0 ; Total
	Pop $R1 ; Completed

	; Loop until ItemsTotal == ItemsDone
	IntCmp $R0 $R1 _done +1 +1
		Sleep 5000
		Goto _loop
_done:
	!insertmacro STACK_VERIFY_END
SectionEnd


Section Enum
	Call PrintStatus
SectionEnd