!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"
!define LOGICLIB_STRCMP
!include "LogicLib.nsh"
!include "Sections.nsh"

!include "StrFunc.nsh"
${StrRep}			; Declare function in advance

!define /ifndef NULL 0
!define SYSINTERNALS_NAME "SysinternalsSuite (January 19, 2015)"

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
		!if /FileExists "..\ReleaseW-mingw\NSxfer.dll"
			!AddPluginDir "..\ReleaseW-mingw"
		!else
			!error "NSxfer.dll (Unicode) not found. Have you built it?"
		!endif
	!else
		!if /FileExists "..\ReleaseA-mingw\NSxfer.dll"
			!AddPluginDir "..\ReleaseA-mingw"
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
;!define MUI_COMPONENTSPAGE_CHECKBITMAP "${NSISDIR}\Contrib\Graphics\Checks\simple.bmp"
!define MUI_COMPONENTSPAGE_NODESC
!define MUI_PAGE_CUSTOMFUNCTION_SHOW .onComponentsPage.Show
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
ManifestDPIAware true

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


# Input:
#   [Param] (HWND) _hDlg
#   [Param] (HWND) _hCtrl
#   [Param] Output variable to receive the Left coordinate
#   [Param] Output variable to receive the Top coordinate
#   [Param] Output variable to receive the Right coordinate
#   [Param] Output variable to receive the Bottom coordinate
# Output:
#   Output variables will receive the control rectangle (in dialog coordinates)
!define GetDlgItemRect `!insertmacro GetDlgItemRect`
!macro GetDlgItemRect _hDlg _hCtrl _OutLeft _OutTop _OutRight _OutBottom
    Push $R2
    System::Call "*(i, i, i, i) p.r12"		; $R2 = malloc( struct RECT)
    System::Call 'User32::GetWindowRect( p ${_hCtrl}, p r12 ) i'
    System::Call 'User32::ScreenToClient( p ${_hDlg}, p r12 ) i'
    Push $R1
		IntOp $R1 $R2 + 8 ;sizeof(POINT)*2 = 8
		System::Call 'User32::ScreenToClient( p ${_hDlg}, p r11 ) i'
	Pop $R1
    System::Call '*$R2( i .s, i .s, i .s, i .s )'
    System::Free $R2
    Exch 4		; Exchange stack index 4 ($R2) with the top of the stack (rect.left)
    Pop $R2
    Pop ${_OutTop}
    Pop ${_OutRight}
    Pop ${_OutBottom}
    Pop ${_OutLeft}
!macroend


Function .onComponentsPage.Show
	; Hide all controls except for $mui.ComponentsPage.Components
	ShowWindow $mui.ComponentsPage.Text ${SW_HIDE}
	ShowWindow $mui.ComponentsPage.InstTypesText ${SW_HIDE}
	ShowWindow $mui.ComponentsPage.ComponentsText ${SW_HIDE}

	;ShowWindow $mui.ComponentsPage.Components ${SW_SHOW}

	ShowWindow $mui.ComponentsPage.DescriptionTitle ${SW_HIDE}
	ShowWindow $mui.ComponentsPage.DescriptionText.Info ${SW_HIDE}
	ShowWindow $mui.ComponentsPage.DescriptionText ${SW_HIDE}

	ShowWindow $mui.ComponentsPage.SpaceRequired ${SW_HIDE}

	Push $1
	Push $2
	Push $3
	Push $4
	Push $5
	Push $6

	${GetDlgItemRect} $mui.ComponentsPage $mui.ComponentsPage.InstTypes $1  $2  $3  $4
	IntOp $5 $4 - $2	; InstTypes combo box height
	IntOp $6 $5 / 4		; InstTypes height / 4

	; Reposition $mui.ComponentsPage.InstTypes
	${GetDlgItemRect} $mui.ComponentsPage $mui.ComponentsPage $1  $2  $3  $4
	System::Call 'user32::SetWindowPos( p $mui.ComponentsPage.InstTypes, p ${NULL}, i $1, i $2, i 150, i $4, i 0 ) i.r0'

	; Resize $mui.ComponentsPage.Components to fill the entire page
	${GetDlgItemRect} $mui.ComponentsPage $mui.ComponentsPage $1  $2  $3  $4
	IntOp $2 $2 + $5
	IntOp $2 $2 + $6
	IntOp $4 $4 - $5
	IntOp $4 $4 - $6
	System::Call 'user32::SetWindowPos( p $mui.ComponentsPage.Components, p ${NULL}, i $1, i $2, i $3, i $4, i 0 ) i.r0'

	Pop $6
	Pop $5
	Pop $4
	Pop $3
	Pop $2
	Pop $1
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
		Push "/CONNECTIONDROPS"
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
		Push "/DEPEND"
		Push "/PRIORITY"
		Push $2	; Request ID
		Push "/ID"
		CallInstDLL "${NSXFER}" "Query"
!else
		NSxfer::Query /NOUNLOAD /ID $2 /PRIORITY /DEPEND /STATUS /WININETSTATUS /METHOD /URL /PROXY /IP /LOCAL /DATA /SENTHEADERS /RECVHEADERS /RECVSIZE /FILESIZE /PERCENT /SPEEDBYTES /SPEED /TIMEWAITING /TIMEDOWNLOADING /ERRORCODE /ERRORTEXT /CONNECTIONDROPS /CONTENT /END
!endif

		StrCpy $R0 "[>] ID:$2"
		Pop $3 ;PRIORITY
		StrCpy $R0 "$R0, Prio:$3"
		Pop $3 ;DEPEND
		IntCmp $3 0 +2 +1 +1
			StrCpy $R0 "$R0, DependsOn:$3"
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
		Pop $3 ;CONNECTIONDROPS
		IntCmp $3 0 +2 +1 +1
			StrCpy $R0 "$R0, Drops:$3"
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
	Push "/USERAGENT"
	Push "/PLUGINVERSION"
	Push "/PLUGINNAME"
	Push "/TOTALTHREADS"
	Push "/TOTALSPEED"
	Push "/TOTALDOWNLOADING"
	Push "/TOTALCOMPLETED"
	Push "/TOTALCOUNT"
	CallInstDLL "${NSXFER}" "QueryGlobal"
!else
	NSxfer::QueryGlobal /NOUNLOAD /TOTALCOUNT /TOTALCOMPLETED /TOTALDOWNLOADING /TOTALSPEED /TOTALTHREADS /PLUGINNAME /PLUGINVERSION /USERAGENT /END
!endif
	Pop $R0 ; Total
	Pop $R1 ; Completed
	Pop $R2 ; Downloading
	Pop $R3 ; Speed
	Pop $R4 ; Worker threads
	Pop $1	; Plugin Name
	Pop $2	; Plugin Version
	Pop $3	; Useragent

	DetailPrint "Transferring $R1+$R2/$R0 items at $R3 using $R4 worker threads"
	DetailPrint "[>] $1 $2"
	DetailPrint "[>] User agent: $3"
	
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

/*
Section Test
	SectionIn 1	; All
!ifdef ENABLE_DEBUGGING
	CallInstDLL "${NSXFER}" "Test"
!else
	NSxfer::Test /NOUNLOAD
!endif
SectionEnd
*/

Section /o "Request: Test webpages"
	SectionIn 1	; All

	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://negrutiu.com`
	!define /redef FILE "$EXEDIR\_webpage_negrutiu.com.html"
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


	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://nefertiti`
	!define /redef FILE "$EXEDIR\_webpage_nefertiti.html"
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


	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://patrunjel`
	!define /redef FILE "$EXEDIR\_webpage_patrunjel.html"
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


Section /o "Request: Notepad++"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://notepad-plus-plus.org/repository/6.x/6.8.3/npp.6.8.3.Installer.exe`
	!define /redef FILE "$EXEDIR\_npp.6.8.3.Installer.exe"
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


Section /o "Request: CCleaner"
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


Section /o "Transfer: SysinternalsSuite (negrutiu.com, Popup mode)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://negrutiu.com/${SYSINTERNALS_NAME}.zip`
	!define /redef FILE "$EXEDIR\_${SYSINTERNALS_NAME}_Transfer.zip"
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


Section /o "Request: SysinternalsSuite (proxy)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK  "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE  "$EXEDIR\_SysinternalsSuiteLive_proxy.zip"
	;!define /redef PROXY "http=http://47.88.15.198:80"	; Ottawa, Canada
	!define /redef PROXY "https=https://212.47.235.81:3129"	; France
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


Section /o "Request: SysinternalsSuite (live)"
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
	;Push "5000"
	;Push "/OPTSENDTIMEOUT"
	;Push "5000"
	;Push "/OPTRECEIVETIMEOUT"
	;Push "9999"
	;Push "/OPTCONNECTTIMEOUT"
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


Section /o "Request: SysinternalsSuite (negrutiu.com)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `https://negrutiu.com/${SYSINTERNALS_NAME}.zip`
	!define /redef FILE "$EXEDIR\_${SYSINTERNALS_NAME}.zip"
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


Section /o "Request: CuckooBox (https://github)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	; NOTE: github.com doesn't support Range headers
	!define /redef LINK `https://github.com/cuckoobox/cuckoo/archive/master.zip`
	!define /redef FILE "$EXEDIR\_CuckooBox_master.zip"
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


Section /o "Request: OSMC installer (http://osmc.tv)"
	SectionIn 1	; All
	!insertmacro STACK_VERIFY_START
	; NOTE: github.com doesn't support Range headers
	!define /redef LINK `http://download.osmc.tv/installers/osmc-installer.exe`
	!define /redef FILE "$EXEDIR\_osmc_installer.exe"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
!ifdef ENABLE_DEBUGGING
	Push "/END"
	;Push 10000
	;Push "/OPTCONNECTTIMEOUT"
	Push 60000
	Push "/TIMEOUTRECONNECT"
	Push 10800000
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
	NSxfer::Request /NOUNLOAD /PRIORITY 10 /METHOD GET /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 300000 /TIMEOUTRECONNECT 60000 /END
!endif
	Pop $0	; ItemID
	!insertmacro STACK_VERIFY_END
SectionEnd


Section /o "Request: httpbin.org/post"
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


Section /o "Request: httpbin.org/get -> Memory"
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


!macro TEST_DEPENDENCY_REQUEST _Filename _DependsOn
	!define /redef LINK `http://httpbin.org/post`
	DetailPrint 'NSxfer::Request "${LINK}" "${_Filename}.txt"'
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
	Push "$EXEDIR\${_Filename}.txt"
	Push "/LOCAL"
	Push "${LINK}"
	Push "/URL"
	Push "POST"
	Push "/METHOD"
	Push "${_DependsOn}"
	Push "/DEPEND"
	Push 2000
	Push "/PRIORITY"
	CallInstDLL "${NSXFER}" "Request"
!else
	NSxfer::Request /NOUNLOAD /PRIORITY 2000 /DEPEND ${_DependsOn} /METHOD POST /URL "${LINK}" /LOCAL "$EXEDIR\${_Filename}.txt" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK}" /END
!endif
	Pop $0	; Request ID
!macroend


Section /o "Dependencies (depend on first request)"
	;SectionIn 1	; All
	!insertmacro STACK_VERIFY_START

	StrCpy $R0 0	; First request ID
	StrCpy $R1 0	; Last request ID

	; First request
	!insertmacro TEST_DEPENDENCY_REQUEST "_DependOnFirst1" -1
	StrCpy $R0 $0	; Remember the first ID
	StrCpy $R1 $0	; Remember the last request ID

	; Subsequent requests
	${For} $1 2 20
		!insertmacro TEST_DEPENDENCY_REQUEST "_DependOnFirst$1" $R0
		StrCpy $R1 $0	; Remember the last request ID
	${Next}

	; Sleep
	;Sleep 2000

	; Unlock the first request, and consequently all the others...
!ifdef ENABLE_DEBUGGING	
	Push "/END"
	Push 0			; No dependency
	Push "/SETDEPEND"
	Push $R0		; First request ID
	Push "/ID"
	CallInstDLL "${NSXFER}" "Set"
!else
	NSxfer::Set /NOUNLOAD /ID $R0 /SETDEPEND 0 /END
!endif
	Pop $0	; Error code. Ignored

	!insertmacro STACK_VERIFY_END
SectionEnd


Section /o "Dependencies (depend on previous request)"
	;SectionIn 1	; All
	!insertmacro STACK_VERIFY_START

	StrCpy $R0 0	; First request ID
	StrCpy $R1 0	; Last request ID

	; First request
	!insertmacro TEST_DEPENDENCY_REQUEST "_DependOnPrevious1" -1
	StrCpy $R0 $0	; Remember the first ID
	StrCpy $R1 $0	; Remember the last request ID

	; Subsequent requests
	${For} $1 2 20
		!insertmacro TEST_DEPENDENCY_REQUEST "_DependOnPrevious$1" $R1
		StrCpy $R1 $0	; Remember the last request ID
	${Next}

	; Sleep
	;Sleep 2000

	; Unlock the first request, and consequently all the others...
!ifdef ENABLE_DEBUGGING	
	Push "/END"
	Push 0			; No dependency
	Push "/SETDEPEND"
	Push $R0		; First request ID
	Push "/ID"
	CallInstDLL "${NSXFER}" "Set"
!else
	NSxfer::Set /NOUNLOAD /ID $R0 /SETDEPEND 0 /END
!endif
	Pop $0	; Error code. Ignored

	!insertmacro STACK_VERIFY_END
SectionEnd


/*
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
*/

Section "Wait (Page mode)" SECTION_WAIT_PAGE
	SectionIn 1	2 ; All & None
SectionEnd

Section /o "Wait (Popup mode)" SECTION_WAIT_POPUP
SectionEnd

Section /o "Wait (Silent mode)" SECTION_WAIT_SILENT
SectionEnd

Section -Wait

	DetailPrint "Waiting..."

	; Mode
	${If} ${SectionIsSelected} ${SECTION_WAIT_PAGE}
		StrCpy $R0 "Page"
	${ElseIf} ${SectionIsSelected} ${SECTION_WAIT_POPUP}
		StrCpy $R0 "Popup"
	${ElseIf} ${SectionIsSelected} ${SECTION_WAIT_SILENT}
		StrCpy $R0 "Silent"
	${Else}
		Return	; Don't wait
	${EndIf}

	!insertmacro STACK_VERIFY_START

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	;Push $HWNDPARENT
	;Push "/TITLEHWND"
	Push $R0
	Push "/MODE"
	CallInstDLL "${NSXFER}" "Wait"
!else
	NSxfer::Wait /NOUNLOAD /MODE $R0 /ABORT "Abort" "Are you sure?" /END
!endif
	Pop $0

	!insertmacro STACK_VERIFY_END
SectionEnd


Section -Enum
	Call PrintStatus
SectionEnd


Function .onSelChange
	${If} $0 >= 0
		${If} ${SectionIsSelected} $0
			${If} $0 == ${SECTION_WAIT_PAGE}
				!insertmacro UnselectSection ${SECTION_WAIT_POPUP}
				!insertmacro UnselectSection ${SECTION_WAIT_SILENT}
			${ElseIf} $0 == ${SECTION_WAIT_POPUP}
				!insertmacro UnselectSection ${SECTION_WAIT_PAGE}
				!insertmacro UnselectSection ${SECTION_WAIT_SILENT}
			${ElseIf} $0 == ${SECTION_WAIT_SILENT}
				!insertmacro UnselectSection ${SECTION_WAIT_PAGE}
				!insertmacro UnselectSection ${SECTION_WAIT_POPUP}
			${EndIf}
		${Else}
		${EndIf}
	${EndIf}
FunctionEnd