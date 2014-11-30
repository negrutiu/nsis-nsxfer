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
; Call NSxfer functins with CallInstDLL
!define ENABLE_DEBUGGING


# The folder where NSxfer.dll is
!ifdef ENABLE_DEBUGGING
	; Debug
	!ifdef NSIS_UNICODE
		!define NSXFER "$EXEDIR\..\DebugW\NSxfer.dll"
	!else
		!define NSXFER "$EXEDIR\..\DebugA\NSxfer.dll"
	!endif
!else
	; Release
	!ifdef NSIS_UNICODE
		!if /FileExists "..\ReleaseW\NSxfer.dll"
			!AddPluginDir "..\ReleaseW"
		!else
			!error "NSxfer.dll (Unicode) not found. Have you built it?"
		!endif
	!else
		!if /FileExists "..\ReleaseA\NSxfer.dll"
			!AddPluginDir "..\ReleaseA"
		!else
			!error "NSxfer.dll (ANSI) not found. Have you built it?"
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
InstType "All"		; 1
InstType "None"		; 2
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS

# Installation page
!insertmacro MUI_PAGE_INSTFILES

# Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

# Installer details
!ifdef NSIS_UNICODE
	Name "NSxferW"
	OutFile "NSxferW.exe"
!else
	Name "NSxferA"
	OutFile "NSxferA.exe"
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

	; .onInit download demo
/*	!define /redef LINK `http://download.tuxfamily.org/notepadplus/6.6.7/npp.6.6.7.Installer.exe`
	!define /redef FILE "$EXEDIR\_npp.6.6.7.Installer.exe"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
	Push "/END"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSXFER}" "Request"

	Push "/END"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	Push "PAGE"
	Push "/MODE"
	CallInstDLL "${NSXFER}" "Wait"*/

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

	DetailPrint "NSxfer::Enumerate"
!ifdef ENABLE_DEBUGGING
	Push "/END"
	CallInstDLL "${NSXFER}" "Enumerate"
!else
	NSxfer::Enumerate /NOUNLOAD /END
!endif

	Pop $1	; Count
	DetailPrint "    $1 requests"
	${For} $0 1 $1

		Pop $2	; Request ID

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
		Push "/DATA"
		Push "/LOCAL"
		Push "/IP"
		Push "/PROXY"
		Push "/URL"
		Push "/METHOD"
		Push "/WININETSTATUS"
		Push "/STATUS"
		Push "/PRIORITY"
		Push $2	; Request ID
		Push "/ID"
		CallInstDLL "${NSXFER}" "Query"
!else
		NSxfer::Query /NOUNLOAD /ID $2 /PRIORITY /STATUS /WININETSTATUS /METHOD /URL /PROXY /IP /LOCAL /DATA /SENTHEADERS /RECVHEADERS /RECVSIZE /FILESIZE /PERCENT /SPEEDBYTES /SPEED /TIMEWAITING /TIMEDOWNLOADING /ERRORCODE /ERRORTEXT /CONTENT /END
!endif

		StrCpy $R0 "[>] ID:$2"
		Pop $3 ;PRIORITY
		StrCpy $R0 "$R0, Prio:$3"
		Pop $3 ;STATUS
		StrCpy $R0 "$R0, [$3]"
		Pop $3 ;WININETSTATUS
		StrCpy $R0 "$R0, WinINet:$3"
		DetailPrint $R0

		StrCpy $R0 "  [Request]"
		Pop $3 ;METHOD
		StrCpy $R0 "$R0 $3"
		Pop $3 ;URL
		StrCpy $R0 "$R0 $3"
		DetailPrint $R0

		Pop $3 ;PROXY
		StrCmp $3 "" +2 +1
			DetailPrint "  [Proxy] $3"
		Pop $3 ;IP
		StrCmp $3 "" +2 +1
			DetailPrint "  [Server] $3"

		Pop $3 ;LOCAL
		DetailPrint "  [Local] $3"

		Pop $3 ;DATA
		${If} $3 != ""
			${StrRep} $3 "$3" "$\r" "\r"
			${StrRep} $3 "$3" "$\n" "\n"
			DetailPrint "  [Sent Data] $3"
		${EndIf}
		Pop $3 ;SENTHEADERS
		${If} $3 != ""
			${StrRep} $3 "$3" "$\r" "\r"
			${StrRep} $3 "$3" "$\n" "\n"
			DetailPrint "  [Sent Headers] $3"
		${EndIf}
		Pop $3 ;RECVHEADERS
		${If} $3 != ""
			${StrRep} $3 "$3" "$\r" "\r"
			${StrRep} $3 "$3" "$\n" "\n"
			DetailPrint "  [Recv Headers] $3"
		${EndIf}

		StrCpy $R0 "  [Size]"
		Pop $3 ;RECVSIZE
		StrCpy $R0 "$R0 $3"
		Pop $3 ;FILESIZE
		StrCpy $R0 "$R0/$3"
		Pop $3 ;PERCENT
		StrCpy $R0 "$R0 ($3%)"
		Pop $3 ;SPEEDBYTES
		Pop $3 ;SPEED
		StrCmp $3 "" +2 +1
			StrCpy $R0 "$R0 @ $3"
		DetailPrint "$R0"

		StrCpy $R0 "  [Time]"
		Pop $3 ;TIMEWAITING
		StrCpy $R0 "$R0 Waiting $3ms"
		Pop $3 ;TIMEDOWNLOADING
		StrCpy $R0 "$R0, Downloading $3ms"
		DetailPrint "$R0"

		StrCpy $R0 "  [Error]"
		Pop $3 ;ERRORCODE
		StrCpy $R0 "$R0 $3"
		Pop $3 ;ERRORTEXT
		StrCpy $R0 "$R0, $3"
		DetailPrint "$R0"

		Pop $3 ;CONTENT
		${If} $3 != ""
			${StrRep} $3 "$3" "$\r" "\r"
			${StrRep} $3 "$3" "$\n" "\n"
			DetailPrint "  [Content] $3"
		${EndIf}
	${Next}

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "/PLUGINVERSION"
	Push "/PLUGINNAME"
	Push "/TOTALTHREADS"
	Push "/TOTALSPEED"
	Push "/TOTALDOWNLOADING"
	Push "/TOTALCOMPLETED"
	Push "/TOTALCOUNT"
	CallInstDLL "${NSXFER}" "QueryGlobal"
!else
	NSxfer::QueryGlobal /NOUNLOAD /TOTALCOUNT /TOTALCOMPLETED /TOTALDOWNLOADING /TOTALSPEED /TOTALTHREADS /PLUGINNAME /PLUGINVERSION /END
!endif
	Pop $R0 ; Total
	Pop $R1 ; Completed
	Pop $R2 ; Downloading
	Pop $R3 ; Speed
	Pop $R4 ; Worker threads
	Pop $1	; Plugin Name
	Pop $2	; Plugin Version

	DetailPrint "Transferring $R1+$R2/$R0 items at $R3 using $R4 worker threads"
	DetailPrint "$1 $2"
	
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
	CallInstDLL "${NSXFER}" "Test"
!else
	NSxfer::Test /NOUNLOAD
!endif
SectionEnd


Section "Request: Nefertiti.html"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://nefertiti`
	!define /redef FILE "$EXEDIR\_Nefertiti.html"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
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
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /SECURITYFLAGS 0x2080|0x100 /END
!endif
	Pop $0	; ItemID

	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: Notepad++"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://download.tuxfamily.org/notepadplus/6.6.7/npp.6.6.7.Installer.exe`
	!define /redef FILE "$EXEDIR\_npp.6.6.7.Installer.exe"
	;!define /redef FILE "NONE"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: CCleaner"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://www.piriform.com/ccleaner/download/slim/downloadfile`
	;!define /redef FILE "$EXEDIR\CCleanerSetup.exe"
	!define /redef FILE "MEMORY"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /URL "${LINK}" /LOCAL "${FILE}" /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Transfer: SysinternalsSuite (nefertiti)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/SysinternalsSuite (September 11, 2014).zip`
	!define /redef FILE "$EXEDIR\_SysinternalsSuite (September 11, 2014)_Transfer.zip"
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "60000"
	Push "/TIMEOUTRECONNECT"
	Push "15000"
	Push "/TIMEOUTCONNECT"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	Push "Popup"
	Push "/MODE"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "GET"
	Push "/METHOD"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSXFER}" "Transfer"
!else
	NSxfer::Transfer /NOUNLOAD /PRIORITY 10 /METHOD GET /URL "${LINK}" /LOCAL "${FILE}" /MODE P /ABORT "Abort" "Are you sure?" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; Status code
	DetailPrint '    Status: $0'
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: SysinternalsSuite (proxy)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK  "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE  "$EXEDIR\_SysinternalsSuiteLive_proxy.zip"
	!define /redef PROXY "http=http://148.251.90.165:3128"
	DetailPrint 'NSxfer::Request /proxy ${PROXY} "${LINK}" "${FILE}"'
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
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 10 /URL "${LINK}" /LOCAL "${FILE}" /PROXY "${PROXY}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: SysinternalsSuite (direct)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE "$EXEDIR\_SysinternalsSuiteLive.zip"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
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
	Push "GET"
	Push "/METHOD"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 10 /METHOD GET /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: SysinternalsSuite (nefertiti)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/SysinternalsSuite (September 11, 2014).zip`
	!define /redef FILE "$EXEDIR\_SysinternalsSuite (September 11, 2014).zip"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
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
	Push "GET"
	Push "/METHOD"
	Push 10
	Push "/PRIORITY"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 10 /METHOD GET /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: httpbin.org/post"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://httpbin.org/post`
	!define /redef FILE "$EXEDIR\_Post1.txt"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
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
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 2000 /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK}" /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section "Request: httpbin.org/post -> Memory"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://httpbin.org/get?param1=value1&param2=value2`
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
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
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 2000 /METHOD GET /URL "${LINK}" /LOCAL MEMORY /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section /o "Request: Priest.mkv"
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/Priest.mkv`
	!define /redef FILE "$EXEDIR\_Priest.mkv"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "10000"
	Push "/TIMEOUTRECONNECT"
	Push "${FILE}"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "GET"
	Push "/METHOD"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTRECONNECT 10000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section Wait
	SectionIn 1	2 ; All & None
	!insertmacro STACK_VERIFY_START

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	;Push $HWNDPARENT
	;Push "/TITLEHWND"
	Push "PAGE"
	Push "/MODE"
	CallInstDLL "${NSXFER}" "Wait"
!else
	NSxfer::Wait /NOUNLOAD /MODE Page /ABORT "Abort" "Are you sure?" /END
!endif
	Pop $0

	!insertmacro STACK_VERIFY_END
SectionEnd


Section -Enum
	Call PrintStatus
SectionEnd