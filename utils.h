#pragma once

//+ TRACE
#ifdef _DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... );
#else
#define TRACE(...)
#endif

//+ assert
#ifdef _DEBUG
#define assert(expr) \
	if ( !(expr)) { \
		TCHAR szMsg[512]; \
		TRACE( _T("  [!] %s, %s:%u\n"), _T(#expr), _T( __FILE__ ), __LINE__ ); \
		wnsprintf( szMsg, (int)ARRAYSIZE( szMsg ), _T("Expression: %s\n%s:%u\n"), _T(#expr), _T( __FILE__ ), __LINE__ ); \
		MessageBox( NULL, szMsg, _T("ASSERT"), MB_ICONERROR ); \
	}

#else
#define assert(...)
#endif

//+ Memory
#define MyAlloc(_size)		GlobalLock(GlobalAlloc(GMEM_FIXED, _size))
#define MyFree(_ptr)		if ( _ptr ) { GlobalFree((HGLOBAL)(_ptr)); (_ptr) = NULL; }

#define MyStrDup(_dst, _src) { \
	(_dst) = (LPTSTR)MyAlloc(( lstrlen( _src ) + 1 ) * sizeof(TCHAR)); \
	if (_dst) \
		lstrcpy( _dst, _src ); \
}
