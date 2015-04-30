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
!define MUI_WELCOMEPAGE_TITLE_3LINES
!insertmacro MUI_PAGE_WELCOME

# CustomWait page
Page Custom .onCustomWait.Create .onCustomWait.Leave

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


Function .onCustomWait.Create

	; NOTE: This function is never called by silent installers

	;!define PAGE_FULLWINDOW

	; Initialize variables
	Var /Global mui.CustomWait.Page
	Var /Global mui.CustomWait.Image
	Var /Global mui.CustomWait.Image.Icon
	Var /Global mui.CustomWait.Title
	Var /Global mui.CustomWait.Title.Font
	Var /Global mui.CustomWait.Title.String
	Var /Global mui.CustomWait.Progress
	Var /Global mui.CustomWait.Text
	Var /Global mui.CustomWait.Text.String

	StrCpy $mui.CustomWait.Page 0
	StrCpy $mui.CustomWait.Image 0
	StrCpy $mui.CustomWait.Image.Icon 0
	StrCpy $mui.CustomWait.Title 0
	StrCpy $mui.CustomWait.Title.Font 0
	StrCpy $mui.CustomWait.Title.String "MyTitle"
	StrCpy $mui.CustomWait.Text 0
	StrCpy $mui.CustomWait.Text.String "MyText"

	; Page header
	!ifndef PAGE_FULLWINDOW
		!insertmacro MUI_HEADER_TEXT_PAGE "MyTitle" "MySubtitle"
	!endif

	; Create the page
	!ifdef PAGE_FULLWINDOW
		nsDialogs::Create /NOUNLOAD 1044		; Use Welcome/Finish page dialog resource
	!else
		nsDialogs::Create /NOUNLOAD 1018		; Use regular page dialog resource
	!endif
	Pop $mui.CustomWait.Page
	nsDialogs::SetRTL /NOUNLOAD $(^RTL)
	;SetCtlColors $mui.CustomWait.Page "" "0xF00000"

	; The image control
	${NSD_CreateIcon} 10u 10u 48 48 ""
	Pop $mui.CustomWait.Image
	System::Call 'kernel32::GetModuleHandle( p ${NULL} ) p.r1' 
	System::Call 'user32::LoadImage( p r1, p 103, i ${IMAGE_ICON}, i 48, i 48, i 0 ) p.s'
	Pop $mui.CustomWait.Image.Icon
	SendMessage $mui.CustomWait.Image ${STM_SETIMAGE} ${IMAGE_ICON} $mui.CustomWait.Image.Icon

	; Title
	!define MUI_CUSTOMWAIT_TITLE_HEIGHT 14
	;!ifndef MUI_CUSTOMWAIT_TITLE_3LINES
	;	!define MUI_CUSTOMWAIT_TITLE_HEIGHT 28
	;!else
	;	!define MUI_CUSTOMWAIT_TITLE_HEIGHT 38
	;!endif

	${NSD_CreateLabel} 60u 10u -70u ${MUI_CUSTOMWAIT_TITLE_HEIGHT}u `$mui.CustomWait.Title.String`
	Pop $mui.CustomWait.Title
	CreateFont $mui.CustomWait.Title.Font "$(^Font)" "12" "700"
	SendMessage $mui.CustomWait.Title ${WM_SETFONT} $mui.CustomWait.Title.Font 0
	;SetCtlColors $mui.CustomWait.Title "" "0xF00000"

	; Progress
	${GetDlgItemRect} $mui.CustomWait.Page $mui.CustomWait.Title $0 $0 $0 $1	; $1 == bottom coordinate	
	IntOp $1 $1 + 20
	${NSD_CreateProgressBar} 60u $1 -70u 12u ""
	Pop $mui.CustomWait.Progress

	; Text
	${GetDlgItemRect} $mui.CustomWait.Page $mui.CustomWait.Progress $0 $0 $0 $1	; $1 == bottom coordinate	
	IntOp $1 $1 + 5
	${NSD_CreateLabel} 61u $1 -70u 65u `$mui.CustomWait.Text.String`
	Pop $mui.CustomWait.Text
	;SetCtlColors $mui.CustomWait.Text "" "0xF00000"

	; Timer
	GetFunctionAddress $0 .onCustomWait.Execute
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
	${NSD_FreeIcon} $mui.CustomWait.Image.Icon

	!insertmacro MUI_UNSET MUI_CUSTOMWAIT_TITLE
	!insertmacro MUI_UNSET MUI_CUSTOMWAIT_TEXT
	!insertmacro MUI_UNSET MUI_CUSTOMWAIT_TITLE_HEIGHT
	!insertmacro MUI_UNSET MUI_CUSTOMWAIT_TEXT_TOP

	!insertmacro MUI_UNSET PAGE_FULLWINDOW

FunctionEnd


Function .onCustomWait.Execute
	;System::Call 'user32::MessageBeep( i -1 ) i'

	GetFunctionAddress $0 .onCustomWait.Execute
	nsDialogs::KillTimer $0

	; NSIS buttons
	EnableWindow $mui.Button.Back ${FALSE}
	EnableWindow $mui.Button.Next ${FALSE}
	;EnableWindow $mui.Button.Cancel ${FALSE}

	; Download files
	Call CustomWait.Request
	Call CustomWait.Wait

	; Cleanup (optional, in case the user hits Back and the download starts again...)
	Call CustomWait.Clear

	; NSIS buttons
	EnableWindow $mui.Button.Back ${TRUE}
	EnableWindow $mui.Button.Next ${TRUE}
	;EnableWindow $mui.Button.Cancel ${TRUE}
FunctionEnd


Function .onCustomWait.Leave
FunctionEnd


# Input:
#   None
# Output:
#   None
Function CustomWait.Request

	; SysinternalsSuite (live)
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


	; SysinternalsSuite (nefertiti)
	!insertmacro STACK_VERIFY_START
	!define /redef LINK `http://nefertiti.homenet.org:8008/${SYSINTERNALS_NAME}.zip`
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
Function CustomWait.Wait
	!insertmacro STACK_VERIFY_START

!ifdef ENABLE_DEBUGGING
	Push "/END"
	Push "Are you sure?"
	Push "Abort"
	Push "/ABORT"
	;Push $HWNDPARENT
	;Push "/TITLEHWND"
	Push $mui.CustomWait.Progress
	Push "/PROGRESSHWND"
	Push $mui.CustomWait.Text
	Push "/STATUSHWND"
	Push Page
	Push "/MODE"
	CallInstDLL "${NSXFER}" "Wait"
!else
	NSxfer::Wait /NOUNLOAD /MODE Page /STATUSHWND $mui.CustomWait.Text /PROGRESSHWND $mui.CustomWait.Progress /ABORT "Abort" "Are you sure?" /END
!endif
	Pop $0

	!insertmacro STACK_VERIFY_END
FunctionEnd


# Input:
#   None
# Output:
#   None
Function CustomWait.Clear
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
