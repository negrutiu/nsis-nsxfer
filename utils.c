
#include "main.h"
#include "utils.h"

//++ TRACE
#ifdef PLUGIN_DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... )
{
	DWORD err = ERROR_SUCCESS;
	if ( pszFormat && *pszFormat ) {

		TCHAR szStr[1024];
		int iLen;
		va_list args;

		va_start( args, pszFormat );
		iLen = wvnsprintf( szStr, (int)ARRAYSIZE( szStr ), pszFormat, args );
		if ( iLen > 0 ) {

			if ( iLen < ARRAYSIZE( szStr ) )
				szStr[iLen] = 0;	/// The string is not guaranteed to be null terminated. We'll add the terminator ourselves

			OutputDebugString( szStr );
		}
		va_end( args );
	}
}
#endif
