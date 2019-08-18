
!ifdef AMD64
	Target amd64-unicode
!else ifdef ANSI
	Target x86-ansi
!else
	Target x86-unicode	; Default
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
;!define USE_NSISDL				; Custom HTTP implementation. No HTTPS support
;!define USE_NSINETC			; Implemented using WinINet. Supports HTTPS
!ifndef USE_NSXFER & USE_NSISDL & USE_NSINETC
	!warning "Defined USE_NSXFER by default!"
	!define USE_NSXFER
!endif

# Plugin folders
!ifdef ENABLE_DEBUGGING
	; Debug
	!ifdef USE_NSXFER
		!ifdef NSIS_AMD64
			!define DLL "$EXEDIR\..\Debug-amd64-unicode\NSxfer.dll"
		!else ifdef NSIS_UNICODE
			!define DLL "$EXEDIR\..\Debug-x86-unicode\NSxfer.dll"
		!else
			!define DLL "$EXEDIR\..\Debug-x86-ansi\NSxfer.dll"
		!endif
	!else ifdef USE_NSISDL
	!else ifdef USE_NSINETC
		!ifdef NSIS_AMD64
			!define DLL "$EXEDIR\..\..\NSinetc\Debug-amd64-unicode\NSinetc.dll"
		!else ifdef NSIS_UNICODE
			!define DLL "$EXEDIR\..\..\NSinetc\Debug-x86-unicode\NSinetc.dll"
		!else
			!define DLL "$EXEDIR\..\..\NSinetc\Debug-x86-ansi\NSinetc.dll"
		!endif
	!else
		!error "USE_XXX not declared!"
	!endif
!else
	; Release
	!ifdef USE_NSXFER
		!AddPluginDir /amd64-unicode	"..\Release-mingw-amd64-unicode"
		!AddPluginDir /x86-unicode		"..\Release-mingw-x86-unicode"
		!AddPluginDir /x86-ansi			"..\Release-mingw-x6-ansi"
	!else ifdef USE_NSISDL
	!else ifdef USE_NSINETC
		!AddPluginDir /amd64-unicode	"..\..\NSinetc\Release-mingw-amd64-unicode"
		!AddPluginDir /x86-unicode		"..\..\NSinetc\Release-mingw-x86-unicode"
		!AddPluginDir /x86-ansi			"..\..\NSinetc\Release-mingw-x86-ansi"
	!else
		!error "USE_XXX not declared!"
	!endif
!endif

# GUI settings
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install-nsis.ico"

# Welcome page
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-nsis.bmp"
!define MUI_WELCOMEPAGE_TITLE "HTTP Flood"
!define MUI_WELCOMEPAGE_TEXT "Syntax:$\n    $EXEFILE /url='your_url' /count=conn_count$\n$\nExamples:$\n    $EXEFILE /url='${DEFAULT_URL}' /count=${DEFAULT_COUNT}$\n    $EXEFILE /url='https://127.0.0.1' /count=${DEFAULT_COUNT}"
!define MUI_PAGE_CUSTOMFUNCTION_PRE .onWelcomePage.Pre		; pre-callback for the Welcome page
!insertmacro MUI_PAGE_WELCOME

# InstFiles page
!insertmacro MUI_PAGE_INSTFILES

# Language
!insertmacro MUI_LANGUAGE "English"

# Installer details
!ifdef NSIS_AMD64
	Name "HttpFlood64"
	OutFile "HttpFlood64.exe"
!else ifdef NSIS_UNICODE
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

		; NSxfer::Transfer
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
		Pop $0			; "OK"

		; NSxfer doesn't remove the request after completion in order to allow the caller to query detailed information
		; In this example we don't need to query anything, so we clear the queue after each transfer
	!ifdef ENABLE_DEBUGGING
		Push "/END"
		Push "/REMOVE"
		CallInstDLL "${DLL}" "Set"
	!else
		NSxfer::Set /NOUNLOAD /REMOVE /END
	!endif
		Pop $1			; Ignored

		;NSxfer::QueryGlobal /NOUNLOAD /TOTALCOUNT /END
		;Pop $1
		;${If} $1 <> 0
		;	MessageBox MB_OK "TotalCount is $1. Must be 0!"
		;${EndIf}

!else ifdef USE_NSISDL

		; NSISdl::download_quiet
		NSISdl::download_quiet "$R1" "$R3"
		Pop $0			; "success"

		SetDetailsPrint none
		Delete "$R3"	; Temp file
		SetDetailsPrint lastused

!else ifdef USE_NSINETC

		; Transfer
	!ifdef ENABLE_DEBUGGING
		Push "/END"
		Push "$R3"
		Push "$R1"
		Push "/SILENT"
		CallInstDLL "${DLL}" "get"
	!else
		NSinetc::get /NOUNLOAD /SILENT "$R1" "$R3" /END
	!endif
		Pop $0			; "OK"

!endif

		DetailPrint "[$R0 / $R2] $R1 = $0"

	${Next}

	SetDetailsPrint none
	Delete "$R3"		; Temp file
	SetDetailsPrint lastused

	SetDetailsPrint both

SectionEnd
