!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"

!define LOGICLIB_STRCMP
!include "LogicLib.nsh"

!include "StrFunc.nsh"
${StrRep}			; Declare function in advance

; Enable debugging
; Call NSdown functins with CallInstDLL
!define ENABLE_DEBUGGING


# The folder where NSdown.dll is
!ifdef ENABLE_DEBUGGING
	; Debug
	!ifdef NSIS_UNICODE
		!define NSDOWN "$EXEDIR\..\DebugW\NSdown.dll"
	!else
		!define NSDOWN "$EXEDIR\..\DebugA\NSdown.dll"
	!endif
!else
	; Release
	!ifdef NSIS_UNICODE
		!if /FileExists "..\ReleaseW\NSdown.dll"
			!AddPluginDir "..\ReleaseW"
		!else
			!error "NSdown.dll (Unicode) not found. Have you built it?"
		!endif
	!else
		!if /FileExists "..\ReleaseA\NSdown.dll"
			!AddPluginDir "..\ReleaseA"
		!else
			!error "NSdown.dll (ANSI) not found. Have you built it?"
		!endif
	!endif
!endif

# GUI settings
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install-nsis.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-nsis.bmp"

# Welcome page
;!define MUI_WELCOMEPAGE_TITLE_3LINES
;!insertmacro MUI_PAGE_WELCOME

# Components page
InstType "All"
InstType "None"
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS

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
!ifdef ENABLE_DEBUGGING
	Push "${ENUM_STATUS}"
	CallInstDLL "${NSDOWN}" "Enumerate"
!else
	NSdown::Enumerate /NOUNLOAD ${ENUM_STATUS}
!endif

	Pop $1	; Count
	DetailPrint "    $1 transfers"
	${For} $0 1 $1

		Pop $2	; Transfer ID

!ifdef ENABLE_DEBUGGING
		Push "/END"
		Push "/CONTENT"
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
!else
		NSdown::Query /NOUNLOAD $2 /STATUS /WININETSTATUS /METHOD /URL /IP /PROXY /LOCAL /SENTHEADERS /RECVHEADERS /RECVSIZE /FILESIZE /PERCENT /SPEEDBYTES /SPEED /TIMEWAITING /TIMEDOWNLOADING /ERRORCODE /ERRORTEXT /CONTENT /END
!endif

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
		Pop $3 ;CONTENT
		${If} $3 != ""
			${StrRep} $3 "$3" "$\r" "\r"
			${StrRep} $3 "$3" "$\n" "\n"
			StrCpy $R0 '$R0 Content:$3'
		${EndIf}

		DetailPrint $R0
	${Next}

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "/COUNTTHREADS"
	Push "/SPEED"
	Push "/COUNTDOWNLOADING"
	Push "/COUNTCOMPLETED"
	Push "/COUNTTOTAL"
	CallInstDLL "${NSDOWN}" "QueryGlobal"
!else
	NSdown::QueryGlobal /NOUNLOAD /COUNTTOTAL /COUNTCOMPLETED /COUNTDOWNLOADING /SPEED /COUNTTHREADS /END
!endif
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


Section Test
	SectionIn 1	; All
!ifdef ENABLE_DEBUGGING
	CallInstDLL "${NSDOWN}" "Test"
!else
	NSdown::Test /NOUNLOAD
!endif
SectionEnd


Section "Transfer: Nefertiti.html"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://nefertiti`
	!define /redef FILE "$EXEDIR\_Nefertiti.html"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "0x2080|0x100"
	Push "/SECURITYFLAGS"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /SECURITYFLAGS 0x2080|0x100 /END
!endif
	Pop $0	; ItemID

	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: Notepad++"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://download.tuxfamily.org/notepadplus/6.6.7/npp.6.6.7.Installer.exe`
	!define /redef FILE "$EXEDIR\_npp.6.6.7.Installer.exe"
	;!define /redef FILE "NONE"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: CCleaner"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://www.piriform.com/ccleaner/download/slim/downloadfile`
	;!define /redef FILE "$EXEDIR\CCleanerSetup.exe"
	!define /redef FILE "MEMORY"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: SysinternalsSuite (proxy)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK  "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE  "$EXEDIR\_SysinternalsSuiteLive_proxy.zip"
	!define /redef PROXY "http=http://86.125.59.121:3128"
	DetailPrint 'NSdown::Transfer /proxy ${PROXY} "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${PROXY}"
	Push "/PROXY"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /PRIORITY 10 /URL "${LINK}" /LOCAL "${FILE}" /PROXY "${PROXY}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: SysinternalsSuite (direct)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE "$EXEDIR\_SysinternalsSuiteLive.zip"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /PRIORITY 10 /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: SysinternalsSuite (nefertiti)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/SysinternalsSuite (September 11, 2014).zip`
	!define /redef FILE "$EXEDIR\_SysinternalsSuite (September 11, 2014).zip"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /PRIORITY 10 /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: httpbin.org/post"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://httpbin.org/post`
	!define /redef FILE "$EXEDIR\_Post1.txt"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "${LINK}"
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
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	Push 2000
	Push "/PRIORITY"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /PRIORITY 2000 /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK}" /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: httpbin.org/post -> Memory"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://httpbin.org/get?param1=value1&param2=value2`
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "MEMORY"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "GET"
	Push "/METHOD"
	Push 2000
	Push "/PRIORITY"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /PRIORITY 2000 /METHOD GET /URL "${LINK}" /LOCAL MEMORY /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section /o "Transfer: Priest.mkv"
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/Priest.mkv`
	!define /redef FILE "$EXEDIR\_Priest.mkv"
	DetailPrint 'NSdown::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "10000"
	Push "/TIMEOUTRECONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	CallInstDLL "${NSDOWN}" "Transfer"
!else
	NSdown::Transfer /NOUNLOAD /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTRECONNECT 10000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section Wait
	SectionIn 1	2 ; All & None
	!insertmacro STACK_VERIFY_START
_loop:
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "/COUNTCOMPLETED"
	Push "/COUNTTOTAL"
	CallInstDLL "${NSDOWN}" "QueryGlobal"
!else
	NSdown::QueryGlobal /NOUNLOAD /COUNTTOTAL /COUNTCOMPLETED /END
!endif
	Pop $R0 ; Total
	Pop $R1 ; Completed

	; Loop until ItemsTotal == ItemsDone
	IntCmp $R0 $R1 _done +1 +1
		Call PrintStatus
		Sleep 5000
		Goto _loop
_done:
	!insertmacro STACK_VERIFY_END
SectionEnd


Section -Enum
	Call PrintStatus
SectionEnd