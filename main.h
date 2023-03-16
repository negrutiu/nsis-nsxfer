
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2014/01/19

#pragma once

#ifndef _DEBUG
	#if DBG || _DEBUG
		#define _DEBUG
	#endif
#endif

#define PLUGINNAME					_T( "NSxfer" )

#define COBJMACROS

#define _WIN32_WINNT 0x0500
#define _WIN32_IE    0x0600
#include <windows.h>
#include <wininet.h>
#include <Shlwapi.h>				/// for wvnsprintf, StrToInt64Ex
#include <commctrl.h>
#include <Shobjidl.h>				/// ITaskbarList

// --> NSIS plugin API
#include <nsis/pluginapi.h>

// NOTE:
// VirusTotal "detonates" dlls by running `RunDll32.exe "<dll>",<proc>` with no parameters
// If the function expects valid parameters the dll will likely crash creating WerFault.exe as child process
// This is interpreted as a potential launcher, increasing the chances of the file being labeled as malitious
// Validate input parameters to prevent this
#define EXDLL_VALID_PARAMS() \
    if (!parent || !IsWindow(parent) || string_size == 0 || !variables || !stacktop || !extra) \
        return;

#undef EXDLL_INIT
#define EXDLL_INIT()           {  \
        g_stringsize=string_size; \
        g_stacktop=stacktop;      \
        g_variables=variables;    \
        g_ep=extra;               \
        g_hwndparent=parent; }

#define EXDLL_VALIDATE() \
	if (g_ep && g_ep->exec_flags && (g_ep->exec_flags->plugin_api_version != NSISPIAPIVER_CURR))  \
		return;

extern extra_parameters *g_ep;		/// main.c
extern HWND g_hwndparent;			/// main.c
#define safepushstring(psz)			pushstring( (psz) ? (psz) : _T("") )
UINT_PTR __cdecl					PluginCallback( enum NSPIM iMessage );
// <-- NSIS plugin API

extern HINSTANCE g_hInst;			/// Defined in main.c
