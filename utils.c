
#include "main.h"
#include "utils.h"

//++ TRACE
#ifdef _DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... )
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
