#pragma once

//+ TRACE
#ifdef _DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... );
#else
#define TRACE(...)
#endif

//+ Memory
#define MyAlloc(_size)		GlobalLock(GlobalAlloc(GMEM_FIXED, _size))
#define MyFree(_ptr)		if ( _ptr ) { GlobalFree((HGLOBAL)(_ptr)); (_ptr) = NULL; }

#define MyStrDup(_dst, _src) { \
	(_dst) = (LPTSTR)MyAlloc(( lstrlen( _src ) + 1 ) * sizeof(TCHAR)); \
	if (_dst) \
		lstrcpy( _dst, _src ); \
}
