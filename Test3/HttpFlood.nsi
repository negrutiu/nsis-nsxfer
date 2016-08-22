
!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define /ifndef TRUE			1
!define /ifndef FALSE			0
!define /ifndef NULL			0

!define DEFAULT_URL				http://httpbin.org
!define DEFAULT_COUNT			5

# Enable debugging
;!define ENABLE_DEBUGGING

# Engine
;!define USE_NSXFER				; Implemented using WinINet. Supports HTTPS
!define USE_NSISDL				; Custom HTTP implementation. No HTTPS support
;!define USE_NSINETC			; Implemented using WinINet. Supports HTTPS

# Plugin folders
!ifdef ENABLE_DEBUGGING
	; Debug
	!ifdef USE_NSXFER
		!ifdef NSIS_UNICODE
			!define DLL "$EXEDIR\..\DebugW\NSxfer.dll"
		!else
			!define DLL "$EXEDIR\..\DebugA\NSxfer.dll"
		!endif
	!else ifdef USE_NSISDL
	!else ifdef USE_NSINETC
	!else
		!error "USE_XXX not declared!"
	!endif
!else
	; Release
	!ifdef USE_NSXFER
		!ifdef NSIS_UNICODE
			!AddPluginDir "..\ReleaseW-mingw"
		!else
			!AddPluginDir "..\ReleaseA-mingw"
		!endif
	!else ifdef USE_NSISDL
	!else ifdef USE_NSINETC
	!else
		!error "USE_XXX not declared!"
	!endif
!endif

# GUI settings
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install-nsis.ico"

# Welcome page
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-nsis.bmp"
!define MUI_WELCOMEPAGE_TITLE "HTTP Flood"
!define MUI_WELCOMEPAGE_TEXT "Syntax:$\n    $EXEFILE /url='your_url' /count=conn_count$\n$\nExample:$\n    $EXEFILE /url='${DEFAULT_URL}' /count=${DEFAULT_COUNT}"
!define MUI_PAGE_CUSTOMFUNCTION_PRE .onWelcomePage.Pre		; pre-callback for the Welcome page
!insertmacro MUI_PAGE_WELCOME

# InstFiles page
!insertmacro MUI_PAGE_INSTFILES

# Language
!insertmacro MUI_LANGUAGE "English"

# Installer details
!ifdef NSIS_UNICODE
	Name "HttpFlood"
	OutFile "HttpFlood.exe"
!else
	Name "HttpFloodA"
	OutFile "HttpFloodA.exe"
!endif

XPStyle on
RequestExecutionLevel user ; don't require UAC elevation
ShowInstDetails show
ManifestDPIAware true


Function .onWelcomePage.Pre

	${GetParameters} $0
	${GetOptions} "$0" "/url=" $1
	${IfNot} ${Errors}
		Abort	; Skip this page
	${EndIf}

	; [Install] -> [Next]
	GetDlgItem $0 $HWNDPARENT 1
	SendMessage $0 ${WM_SETTEXT} 0 'STR:$(^NextBtn)'

FunctionEnd


Section Flood

	DetailPrint "Flooding in progress..."
	SetDetailsPrint listonly

	; Command line
	${GetParameters} $0

	${GetOptions} "$0" "/url=" $R1
	${If} ${Errors}
		StrCpy $R1 ${DEFAULT_URL}
	${EndIf}

	${GetOptions} "$0" "/count=" $R2
	${If} ${Errors}
		StrCpy $R2 ${DEFAULT_COUNT}
	${EndIf}

	; Info
	!ifdef USE_NSXFER

		!ifdef ENABLE_DEBUGGING
			Push "/END"
			Push "/USERAGENT"
			Push "/PLUGINVERSION"
			Push "/PLUGINNAME"
			CallInstDLL "${DLL}" "QueryGlobal"
		!else
			NSxfer::QueryGlobal /NOUNLOAD /PLUGINNAME /PLUGINVERSION /USERAGENT /END
		!endif
		Pop $1
		Pop $2
		Pop $3
		DetailPrint "Plugin: $1 $2"
		DetailPrint "UserAgent: $3"

	!else ifdef USE_NSISDL
		DetailPrint "Plugin: NSISdl"
	!else ifdef USE_NSINETC
		DetailPrint "Plugin: NSinetc"
	!endif

	; Temp file
	GetTempFileName $R3

	; Flood
	${For} $R0 1 $R2

!ifdef USE_NSXFER

		; Transfer
	!ifdef ENABLE_DEBUGGING
		Push "/END"
		Push "None"
		Push "/LOCAL"
		Push "$R1"
		Push "/URL"
		Push "Silent"
		Push "/MODE"
		Push "GET"
		Push "/METHOD"
		CallInstDLL "${DLL}" "Transfer"
	!else
		NSxfer::Transfer /NOUNLOAD /METHOD GET /MODE Silent /URL "$R1" /LOCAL None /END
	!endif
		Pop $0		; "OK"

		; NSxfer doesn't remove the request after completion, to allow the caller to query detailed information
		; In this example we don't need to query anything, so we clear the queue after each transfer
	!ifdef ENABLE_DEBUGGING
		Push "/END"
		Push "/REMOVE"
		CallInstDLL "${DLL}" "Set"
	!else
		NSxfer::Set /NOUNLOAD /REMOVE /END
	!endif
		Pop $1		; Ignored

		;NSxfer::QueryGlobal /NOUNLOAD /TOTALCOUNT /END
		;Pop $1
		;${If} $1 <> 0
		;	MessageBox MB_OK "TotalCount is $1. Must be 0!"
		;${EndIf}

!else ifdef USE_NSISDL

		; download_quiet
		NSISdl::download_quiet "$R1" "$R3"
		Pop $0

		SetDetailsPrint none
		Delete "$R3"	; Temp file
		SetDetailsPrint lastused

!else ifdef USE_NSINETC
!endif

		DetailPrint "[$R0 / $R2] $R1 = $0"

	${Next}

	SetDetailsPrint none
	Delete "$R3"	; Temp file
	SetDetailsPrint lastused

	SetDetailsPrint both

SectionEnd


/*
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

*/
