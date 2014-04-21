
#include "main.h"
#include "utils.h"


MEMORY_STATS g_MemStats = { 0 };


//++ UtilsInitialize
VOID UtilsInitialize()
{
	TRACE( _T( "  UtilsInitialize()\n" ) );
}

//++ UtilsDestroy
VOID UtilsDestroy()
{
	TRACE( _T( "  MyAlloc: %u calls, %I64u bytes\n" ), g_MemStats.AllocCalls, g_MemStats.AllocBytes );
	TRACE( _T( "  MyFree:  %u calls, %I64u bytes\n" ), g_MemStats.FreeCalls, g_MemStats.FreeBytes );
	TRACE( _T( "  UtilsDestroy()\n" ) );
}

//++ TraceImpl
#if DBG || _DEBUG
VOID TraceImpl( __in LPCTSTR pszFormat, ... )
{
	DWORD err = ERROR_SUCCESS;
	if ( pszFormat && *pszFormat ) {

		TCHAR szStr[1024];
		int iLen1, iLen2;
		va_list args;

		iLen1 = wnsprintf( szStr, (int)ARRAYSIZE( szStr ), _T( "[NSdown.th%04x] " ), GetCurrentThreadId() );

		va_start( args, pszFormat );
		iLen2 = wvnsprintf( szStr + iLen1, (int)ARRAYSIZE( szStr ) - iLen1, pszFormat, args );
		if ( iLen2 > 0 ) {
			if ( iLen1 + iLen2 < ARRAYSIZE( szStr ) )
				szStr[iLen1 + iLen2] = 0;	/// The string is not guaranteed to be null terminated
		} else {
			szStr[ARRAYSIZE( szStr ) - 1] = 0;
		}
		OutputDebugString( szStr );
		va_end( args );
	}
}
#endif

//++ GetLocalFileTime
BOOL GetLocalFileTime( _Out_ LPFILETIME lpFT )
{
	if ( lpFT ) {
		SYSTEMTIME st;
		GetLocalTime( &st );
		return SystemTimeToFileTime( &st, lpFT );
	}
	return FALSE;
}

//++ AllocErrorStr
VOID AllocErrorStr( _In_ DWORD dwErrCode, _Out_ TCHAR **ppszErrText )
{
	if ( ppszErrText ) {

		DWORD dwLen;
		TCHAR szTextError[512];
		HMODULE hModule = NULL;
		DWORD dwExtraFlags = 0;

		if ( dwErrCode >= INTERNET_ERROR_BASE && dwErrCode <= INTERNET_ERROR_LAST ) {
			hModule = GetModuleHandle( _T( "wininet.dll" ) );
			dwExtraFlags = FORMAT_MESSAGE_FROM_HMODULE;
		} else {
			dwExtraFlags = FORMAT_MESSAGE_FROM_SYSTEM;
		}

		szTextError[0] = 0;
		dwLen = FormatMessage( FORMAT_MESSAGE_IGNORE_INSERTS | dwExtraFlags, hModule, dwErrCode, 0, szTextError, ARRAYSIZE( szTextError ), NULL );
		if ( dwLen > 0 ) {
			StrTrim( szTextError, _T( ". \r\n" ) );
			MyStrDup( *ppszErrText, szTextError );
		}
	}
}