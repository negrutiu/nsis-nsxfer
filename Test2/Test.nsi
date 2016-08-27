!ifdef ANSI
	Unicode false
!else
	Unicode true	; Default
!endif

!include "MUI2.nsh"
!define LOGICLIB_STRCMP
!include "LogicLib.nsh"


!define /ifndef TRUE 1
!define /ifndef FALSE 0
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
!define MUI_WELCOMEPAGE_TITLE_3LINES
!insertmacro MUI_PAGE_WELCOME

# Download page
Page Custom .onDownloadPage.Create .onDownloadPage.Leave

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
	CallInstDLL "${NSXFER}" "Wait" */

FunctionEnd


# One section is always required
Section -Dummy
SectionEnd

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



Function .onDownloadPage.Create

	; NOTE: This function is never called by silent installers

	;!define PAGE_FULLWINDOW

	!define /redef PageName		DownloadPage
	!define /redef PageTitle	"Page Title"
	!define /redef PageSubtitle	"Page Subtitle"
	!define /redef PageText		"Page Text"

	; Initialize variables
	Var /Global mui.${PageName}.Page
	Var /Global mui.${PageName}.Image
	Var /Global mui.${PageName}.Image.Icon
	Var /Global mui.${PageName}.Title
	Var /Global mui.${PageName}.Title.Font
	Var /Global mui.${PageName}.Title.String
	Var /Global mui.${PageName}.Progress
	Var /Global mui.${PageName}.Text
	Var /Global mui.${PageName}.Text.String

	StrCpy $mui.${PageName}.Page 0
	StrCpy $mui.${PageName}.Image 0
	StrCpy $mui.${PageName}.Image.Icon 0
	StrCpy $mui.${PageName}.Title 0
	StrCpy $mui.${PageName}.Title.Font 0
	StrCpy $mui.${PageName}.Title.String "${PageTitle}"
	StrCpy $mui.${PageName}.Text 0
	StrCpy $mui.${PageName}.Text.String "${PageText}"

	; Page header
	!ifndef PAGE_FULLWINDOW
		!insertmacro MUI_HEADER_TEXT_PAGE "${PageTitle}" "${PageSubtitle}"
	!endif

	; Create the page
	!ifdef PAGE_FULLWINDOW
		nsDialogs::Create /NOUNLOAD 1044		; Use Welcome/Finish page dialog resource
	!else
		nsDialogs::Create /NOUNLOAD 1018		; Use regular page dialog resource
	!endif
	Pop $mui.${PageName}.Page
	nsDialogs::SetRTL /NOUNLOAD $(^RTL)
	;SetCtlColors $mui.${PageName}.Page "" "0xF00000"

	; The image control
	${NSD_CreateIcon} 10u 10u 48 48 ""
	Pop $mui.${PageName}.Image
	System::Call 'kernel32::GetModuleHandle( p ${NULL} ) p.r1' 
	System::Call 'user32::LoadImage( p r1, p 103, i ${IMAGE_ICON}, i 48, i 48, i 0 ) p.s'
	Pop $mui.${PageName}.Image.Icon
	SendMessage $mui.${PageName}.Image ${STM_SETIMAGE} ${IMAGE_ICON} $mui.${PageName}.Image.Icon

	; Title
	!define PAGE_TITLE_HEIGHT 14
	;!ifndef MUI_DOWNLOADPAGE_TITLE_3LINES
	;	!define PAGE_TITLE_HEIGHT 28
	;!else
	;	!define PAGE_TITLE_HEIGHT 38
	;!endif

	${NSD_CreateLabel} 60u 10u -70u ${PAGE_TITLE_HEIGHT}u `$mui.${PageName}.Title.String`
	Pop $mui.${PageName}.Title
	CreateFont $mui.${PageName}.Title.Font "$(^Font)" "12" "700"
	SendMessage $mui.${PageName}.Title ${WM_SETFONT} $mui.${PageName}.Title.Font 0
	;SetCtlColors $mui.${PageName}.Title "" "0xF00000"

	; Progress
	${GetDlgItemRect} $mui.${PageName}.Page $mui.${PageName}.Title $0 $0 $0 $1	; $1 == bottom coordinate	
	IntOp $1 $1 + 20
	${NSD_CreateProgressBar} 60u $1 -70u 12u ""
	Pop $mui.${PageName}.Progress

	; Text
	${GetDlgItemRect} $mui.${PageName}.Page $mui.${PageName}.Progress $0 $0 $0 $1	; $1 == bottom coordinate	
	IntOp $1 $1 + 5
	${NSD_CreateLabel} 61u $1 -70u 65u `$mui.${PageName}.Text.String`
	Pop $mui.${PageName}.Text
	;SetCtlColors $mui.${PageName}.Text "" "0xF00000"

	; Timer
	GetFunctionAddress $0 .on${PageName}.Execute
	nsDialogs::CreateTimer $0 50

	; Display the page
	!ifdef PAGE_FULLWINDOW
		Call muiPageLoadFullWindow
		nsDialogs::Show
		Call muiPageUnloadFullWindow
	!else
		nsDialogs::Show
	!endif

	; Delete the image from memory
	${NSD_FreeIcon} $mui.${PageName}.Image.Icon

	!insertmacro MUI_UNSET PAGE_TITLE_HEIGHT
	!insertmacro MUI_UNSET PAGE_FULLWINDOW

FunctionEnd


Function .onDownloadPage.Execute
	!define /redef PageName "DownloadPage"

	;System::Call 'user32::MessageBeep( i -1 ) i'

	GetFunctionAddress $0 .on${PageName}.Execute
	nsDialogs::KillTimer $0

	; NSIS buttons
	EnableWindow $mui.Button.Back ${FALSE}
	EnableWindow $mui.Button.Next ${FALSE}
	;EnableWindow $mui.Button.Cancel ${TRUE}

	; Download files
	Call Download.Request
	Call Download.Wait
	Call Download.Clear		; Clear the queue, in case the user hits [Back]...

	; NSIS buttons
	EnableWindow $mui.Button.Back ${TRUE}
	EnableWindow $mui.Button.Next ${TRUE}
	EnableWindow $mui.Button.Cancel ${FALSE}
FunctionEnd


Function .onDownloadPage.Leave
	!define /redef PageName "DownloadPage"
FunctionEnd


# Input:
#   None
# Output:
#   None
Function Download.Request

	; SysinternalsSuite (microsoft.com)
	!insertmacro STACK_VERIFY_START
	!define /redef LINK "http://live.sysinternals.com/Files/SysinternalsSuite.zip"
	!define /redef FILE "$EXEDIR\_SysinternalsSuite_Live.zip"
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


	; SysinternalsSuite (negrutiu.com)
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


	; CuckooBox
	; NOTE: github.com doesn't support Range headers
	!insertmacro STACK_VERIFY_START
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

FunctionEnd


# Input:
#   None
# Output:
#   None
Function Download.Wait
	!insertmacro STACK_VERIFY_START

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	;Push $HWNDPARENT
	;Push "/TITLEHWND"
	Push $mui.${PageName}.Progress
	Push "/PROGRESSHWND"
	Push $mui.${PageName}.Text
	Push "/STATUSHWND"
	Push Page
	Push "/MODE"
	CallInstDLL "${NSXFER}" "Wait"
!else
	NSxfer::Wait /NOUNLOAD /MODE Page /STATUSHWND $mui.${PageName}.Text /PROGRESSHWND $mui.${PageName}.Progress /ABORT "Abort" "Are you sure?" /END
!endif
	Pop $0

	!insertmacro STACK_VERIFY_END
FunctionEnd


# Input:
#   None
# Output:
#   None
Function Download.Clear
	!insertmacro STACK_VERIFY_START

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "/REMOVE"
	CallInstDLL "${NSXFER}" "Set"
!else
	NSxfer::Set /NOUNLOAD /REMOVE /END
!endif
	Pop $0

	!insertmacro STACK_VERIFY_END
FunctionEnd
