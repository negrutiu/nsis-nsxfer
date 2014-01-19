
#if DBG || _DEBUG || DEBUG
	#define PLUGIN_DEBUG
#endif

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <wininet.h>
#ifdef PLUGIN_DEBUG
	#include <Shlwapi.h>	/// for wvnsprintf
#endif
#include "nsiswapi/pluginapi.h"


#define USERAGENT _T("NSIS NSbkdld (WinInet)")
HINSTANCE g_hInst = NULL;

//++ TRACE
#ifdef PLUGIN_DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... )
{
	DWORD err = ERROR_SUCCESS;
	if ( pszFormat && *pszFormat ) {

		TCHAR szStr[1024];
		int iLen;
		va_list args;

		va_start(args, pszFormat);
		iLen = wvnsprintf( szStr, (int)ARRAYSIZE(szStr), pszFormat, args );
		if ( iLen > 0 ) {

			if ( iLen < ARRAYSIZE(szStr))
				szStr[iLen] = 0;	/// The string is not guaranteed to be null terminated. We'll add the terminator ourselves

			OutputDebugString( szStr );
		}
		va_end(args);
	}
}
#else
	#define TRACE(...)
#endif


//++ Download
EXTERN_C __declspec(dllexport)
void __cdecl Download(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	EXDLL_INIT();
	if ( !IsCompatibleApiVersion())
		return;

	TRACE( _T("Download: TODO\n"));
}


//++ _DllMainCRTStartup
EXTERN_C
BOOL WINAPI _DllMainCRTStartup(
	HMODULE hInst,
	UINT iReason,
	LPVOID lpReserved
	)
{
	if ( iReason == DLL_PROCESS_ATTACH ) {
		g_hInst = hInst;
		/// TODO: Initializations
	}
	return TRUE;
}
