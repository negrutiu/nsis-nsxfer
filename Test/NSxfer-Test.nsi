
# NSxfer demo
# Marius Negrutiu - https://github.com/negrutiu/nsis-nsxfer#nsis-plugin-nsxfer

!ifdef AMD64
	!define _TARGET_ amd64-unicode
!else ifdef ANSI
	!define _TARGET_ x86-ansi
!else
	!define _TARGET_ x86-unicode		; Default
!endif

Target ${_TARGET_}

!include "MUI2.nsh"
!define LOGICLIB_STRCMP
!include "LogicLib.nsh"
!include "Sections.nsh"

!include "StrFunc.nsh"
${StrRep}				; Declare in advance
${StrTok}				; Declare in advance

!define /ifndef NULL 0


# NSxfer.dll development location
!ifdef DEVEL
!if ! /FileExists "..\Release-mingw-${_TARGET_}\NSxfer.dll"
	!error "Missing \Release-mingw-${_TARGET_}\NSxfer.dll"
!endif
!AddPluginDir /amd64-unicode "..\Release-mingw-amd64-unicode"
!AddPluginDir /x86-unicode   "..\Release-mingw-x86-unicode"
!AddPluginDir /x86-ansi      "..\Release-mingw-x86-ansi"
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
;!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_RESERVEFILE_LANGDLL

# Installer details
Name    "NSxfer-Test-${_TARGET_}"
OutFile "NSxfer-Test-${_TARGET_}.exe"
XPStyle on
RequestExecutionLevel user		; Don't require UAC elevation
ShowInstDetails show
ManifestDPIAware true


#---------------------------------------------------------------#
# .onInit                                                       #
#---------------------------------------------------------------#
Function .onInit

	; Initializations
	InitPluginsDir

	; Language selection
	!define MUI_LANGDLL_ALLLANGUAGES
	!insertmacro MUI_LANGDLL_DISPLAY

/*
	; .onInit download demo
	; NOTE: Transfers from .onInit can be either Silent or Popup (no Page!)
	!define /redef LINK 'https://httpbin.org/post?param1=1&param2=2'
	!define /redef FILE '$EXEDIR\_Post_onInit.json'
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /METHOD POST /MODE Popup /URL "${LINK}" /LOCAL "${FILE}" /DATA 'User=My+User&Pass=My+Pass' /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Dummy: Dummy" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "https://wikipedia.org" /END
	Pop $0
*/
FunctionEnd


Section "Cleanup test files"
	SectionIn 1	2 ; All
	FindFirst $0 $1 "$EXEDIR\_*.*"
loop:
	StrCmp $1 "" done
	Delete "$EXEDIR\$1"
	FindNext $0 $1
	Goto loop
done:
	FindClose $0
SectionEnd


Section /o "HTTP GET (Page mode)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK 'https://download.sysinternals.com/files/SysinternalsSuite.zip'
	!define /redef FILE '$EXEDIR\_SysinternalsSuite.zip'
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 30000 /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


Section /o "HTTP GET (Popup mode)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	; NOTE: github.com doesn't support Range headers
	!define /redef LINK `https://github.com/cuckoobox/cuckoo/archive/master.zip`
	!define /redef FILE "$EXEDIR\_CuckooBox_master.zip"
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /URL "${LINK}" /LOCAL "${FILE}" /Mode Popup /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


Section /o "HTTP GET (Silent mode)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK `https://download.mozilla.org/?product=firefox-stub&os=win&lang=en-US`
	!define /redef FILE "$EXEDIR\_Firefox.exe"
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /URL "${LINK}" /LOCAL "${FILE}" /Mode Silent /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


Section /o "HTTP GET (Memory)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK `https://wikipedia.org`
	!define /redef FILE "memory"

	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
    NSxfer::Request /URL "${LINK}" /LOCAL "${FILE}" /END
	Pop $0

	NSxfer::Wait /ID $0 /END
    Pop $1

	NSxfer::Query /ID $0 /STATUS /END
    Pop $1
	DetailPrint "Status: $1"
SectionEnd


!define /redef COUNT 32
Section /o "HTTP GET (Memory * ${COUNT}x)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef FILE "memory"

    ${For} $R0 1 ${COUNT}
        StrCpy $R1 "https://httpbin.org/post?index=$R0&total=${COUNT}"
        DetailPrint 'NSxfer::Request [$R0/${COUNT}] $R1 "${FILE}"'
        NSxfer::Request /URL $R1 /LOCAL "${FILE}" /METHOD POST /END
        Pop $0
    ${Next}

	NSxfer::Wait /ABORT "" "" /END
    Pop $1
SectionEnd


Section /o "HTTP GET (Parallel transfers)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	; Request 1
	!define /redef LINK `https://download.mozilla.org/?product=firefox-stub&os=win&lang=en-US`
	!define /redef FILE "$EXEDIR\_Firefox(2).exe"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
	NSxfer::Request /URL "${LINK}" /LOCAL "${FILE}" /Mode Silent /END
	Pop $1

	; Request 2
	!define /redef LINK `https://download.mozilla.org/?product=firefox-stub&os=win&lang=en-US`
	!define /redef FILE "$EXEDIR\_Firefox(3).exe"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
	NSxfer::Request /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /END
	Pop $2

	; Request 3
	!define /redef LINK `http://download.osmc.tv/installers/osmc-installer.exe`
	!define /redef FILE "$EXEDIR\_osmc_installer.exe"
	DetailPrint 'NSxfer::Request "${LINK}" "${FILE}"'
	NSxfer::Request /URL "${LINK}" /LOCAL "${FILE}" /TIMEOUTCONNECT 15000 /END
	Pop $3

	; Wait for all
	DetailPrint 'Waiting . . .'
	NSxfer::Wait /MODE Page /ABORT "Abort" "Are you sure?" /END
	Pop $0

	; Validate individual transfer status...
	; TODO

	DetailPrint 'Done'
SectionEnd


Section /o "-HTTP GET (proxy)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK  "https://download.sysinternals.com/files/SysinternalsSuite.zip"
	!define /redef FILE  "$EXEDIR\_SysinternalsSuiteLive_proxy.zip"
	!define /redef PROXY "http=54.36.139.108:8118 https=54.36.139.108:8118"			; France
	DetailPrint 'NSxfer::Transfer /proxy ${PROXY} "${LINK}" "${FILE}"'
	NSxfer::Transfer /PRIORITY 10 /URL "${LINK}" /LOCAL "${FILE}" /PROXY "${PROXY}" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /ABORT "Abort" "Are you sure?" /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


Section /o "HTTP POST (application/json)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK 'https://httpbin.org/post?param1=1&param2=2'
	!define /redef FILE '$EXEDIR\_Post_json.json'
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /DATA '{"number_of_the_beast" : 666}' /HEADERS "Content-Type: application/json" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "https://wikipedia.org" /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


Section /o "HTTP POST (application/x-www-form-urlencoded)"
	SectionIn 1	; All

	DetailPrint '-----------------------------------------------'
	DetailPrint '${__SECTION__}'
	DetailPrint '-----------------------------------------------'

	!define /redef LINK 'http://httpbin.org/post?param1=1&param2=2'
	!define /redef FILE '$EXEDIR\_Post_form.json'
	DetailPrint 'NSxfer::Transfer "${LINK}" "${FILE}"'
	NSxfer::Transfer /METHOD POST /URL "${LINK}" /LOCAL "${FILE}" /DATA 'User=My+User&Pass=My+Pass' /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Dummy: Dummy" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "https://wikipedia.org" /END
	Pop $0
	DetailPrint "Status: $0"
SectionEnd


!macro TEST_DEPENDENCY_REQUEST _Filename _DependsOn
	!define /redef LINK `http://httpbin.org/post`
	DetailPrint 'NSxfer::Request "${LINK}" "${_Filename}.txt"'
	NSxfer::Request /PRIORITY 2000 /DEPEND ${_DependsOn} /METHOD POST /URL "${LINK}" /LOCAL "$EXEDIR\${_Filename}.txt" /HEADERS "Content-Type: application/x-www-form-urlencoded$\r$\nContent-Test: TEST" /DATA "user=My+User+Name&pass=My+Password" /TIMEOUTCONNECT 15000 /TIMEOUTRECONNECT 60000 /REFERER "${LINK}" /END
	Pop $0	; Request ID
!macroend


Section /o "Test Dependencies (depend on first request)"
	;SectionIn 1	; All

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
	NSxfer::Set /ID $R0 /SETDEPEND 0 /END
	Pop $0	; Error code. Ignored

	; Wait
	DetailPrint 'Waiting . . .'
	NSxfer::Wait /MODE Page /ABORT "Abort" "Are you sure?" /END
	Pop $0

SectionEnd


Section /o "Test Dependencies (depend on previous request)"
	;SectionIn 1	; All

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
	NSxfer::Set /ID $R0 /SETDEPEND 0 /END
	Pop $0	; Error code. Ignored

	; Wait
	DetailPrint 'Waiting . . .'
	NSxfer::Wait /MODE Page /ABORT "Abort" "Are you sure?" /END
	Pop $0

SectionEnd


; Input: [Stack] Request ID
; Output: None
Function PrintRequest
    Exch $R1    ; Request ID
	Push $0
    Push $1
    Push $2

    NSxfer::Query /ID $R1 /PRIORITY /DEPEND /STATUS /WININETSTATUS /METHOD /URL /PROXY /IP /LOCAL /DATA /SENTHEADERS /RECVHEADERS /RECVSIZE /FILESIZE /PERCENT /SPEEDBYTES /SPEED /TIMEWAITING /TIMEDOWNLOADING /ERRORCODE /ERRORTEXT /CONNECTIONDROPS /CONTENT /END

    StrCpy $0 "[>] ID:$R1"
    Pop $1 ;PRIORITY
    StrCpy $0 "$0, Prio:$1"
    Pop $1 ;DEPEND
    IntCmp $1 0 +2 +1 +1
        StrCpy $0 "$0, DependsOn:$1"
    Pop $1 ;STATUS
    StrCpy $0 "$0, [$1]"
    Pop $1 ;WININETSTATUS
    StrCpy $0 "$0, WinINet:$1"
    DetailPrint $0

    StrCpy $0 "  [Request]"
    Pop $1 ;METHOD
    StrCpy $0 "$0 $1"
    Pop $1 ;URL
    StrCpy $0 "$0 $1"
    DetailPrint $0

    Pop $1 ;PROXY
    StrCmp $1 "" +2 +1
        DetailPrint "  [Proxy] $1"
    Pop $1 ;IP
    StrCmp $1 "" +2 +1
        DetailPrint "  [Server] $1"

    Pop $1 ;LOCAL
    DetailPrint "  [Local] $1"

    Pop $1 ;DATA
    ${If} $1 != ""
        ${StrRep} $1 "$1" "$\r" "\r"
        ${StrRep} $1 "$1" "$\n" "\n"
        DetailPrint "  [Sent Data] $1"
    ${EndIf}
    Pop $1 ;SENTHEADERS
    ${If} $1 != ""
        DetailPrint "  [Sent Headers]"
        ${For} $2 0 100
            ${StrTok} $0 $1 "$\r$\n" $2 1
            ${If} $0 == ""
                ${Break}
            ${EndIf}
            DetailPrint "    $0"
        ${Next}
    ${EndIf}
    Pop $1 ;RECVHEADERS
    ${If} $1 != ""
        DetailPrint "  [Recv Headers]"
        ${For} $2 0 100
            ${StrTok} $0 $1 "$\r$\n" $2 1
            ${If} $0 == ""
                ${Break}
            ${EndIf}
            DetailPrint "    $0"
        ${Next}
    ${EndIf}

    StrCpy $0 "  [Size]"
    Pop $1 ;RECVSIZE
    StrCpy $0 "$0 $1"
    Pop $1 ;FILESIZE
    StrCpy $0 "$0/$1"
    Pop $1 ;PERCENT
    StrCpy $0 "$0 ($1%)"
    Pop $1 ;SPEEDBYTES
    Pop $1 ;SPEED
    StrCmp $1 "" +2 +1
        StrCpy $0 "$0 @ $1"
    DetailPrint "$0"

    StrCpy $0 "  [Time]"
    Pop $1 ;TIMEWAITING
    StrCpy $0 "$0 Waiting $1ms"
    Pop $1 ;TIMEDOWNLOADING
    StrCpy $0 "$0, Downloading $1ms"
    DetailPrint "$0"

    StrCpy $0 "  [Error]"
    Pop $1 ;ERRORCODE
    StrCpy $0 "$0 $1"
    Pop $1 ;ERRORTEXT
    StrCpy $0 "$0, $1"
    Pop $1 ;CONNECTIONDROPS
    IntCmp $1 0 +2 +1 +1
        StrCpy $0 "$0, Drops:$1"
    DetailPrint "$0"

    Pop $1 ;CONTENT
    ${If} $1 != ""
        ; DetailPrint "  [Content]"
        ; ${For} $2 0 100
        ;     ${StrTok} $0 $1 "$\r$\n" $2 1
        ;     ${If} $0 == ""
        ;         ${Break}
        ;     ${EndIf}
        ;     DetailPrint "    $0"
        ; ${Next}
        ${StrRep} $1 "$1" "$\r" "\r"
        ${StrRep} $1 "$1" "$\n" "\n"
        DetailPrint "  [Content] $1"
    ${EndIf}

    Pop $2
    Pop $1
    Pop $0
    Pop $R1
FunctionEnd


Function PrintSummary

	Push $0
	Push $1
	Push $2
	Push $3
	Push $R0
	Push $R1
	Push $R2
	Push $R3
	Push $R4

	; Enumerate all transfers (completed + pending + waiting)
	DetailPrint "NSxfer::Enumerate"
	NSxfer::Enumerate /END

	Pop $1	; Count
	DetailPrint "    $1 requests"
	${For} $0 1 $1
        Call PrintRequest
	${Next}

	NSxfer::QueryGlobal /TOTALCOUNT /TOTALCOMPLETED /TOTALDOWNLOADING /TOTALSPEED /TOTALTHREADS /PLUGINNAME /PLUGINVERSION /USERAGENT /END
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

FunctionEnd


Section -Summary
	DetailPrint '-----------------------------------------------'
	DetailPrint ' ${__SECTION__}'
	DetailPrint '-----------------------------------------------'
	Call PrintSummary
SectionEnd
