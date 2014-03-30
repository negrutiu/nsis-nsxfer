#pragma once

//+ Init functions
VOID UtilsInitialize();
VOID UtilsDestroy();


//+ TRACE
#ifdef _DEBUG
	#define TRACE TraceImpl
	VOID TraceImpl( __in LPCTSTR pszFormat, ... );
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
	#define verify assert
#else
	#define assert(...)
	#define verify(expr) ((VOID)(expr))
#endif

//+ Utils
BOOL GetLocalFileTime( _Out_ LPFILETIME lpFT );

//+ Memory
#define MyAlloc(_size)		GlobalLock(GlobalAlloc(GMEM_FIXED, _size)); g_MemStats.AllocBytes += (_size); g_MemStats.AllocCalls++
#define MyAllocStr(_len)	(LPTSTR)MyAlloc( ((_len) + 1) * sizeof(TCHAR))
#define MyFree(_ptr)		if ( _ptr ) { g_MemStats.FreeBytes += GlobalSize((HGLOBAL)(_ptr)); g_MemStats.FreeCalls++; GlobalFree((HGLOBAL)(_ptr)); (_ptr) = NULL; }

#define MyStrDup(_dst, _src) { \
	(_dst) = MyAllocStr( lstrlen( _src )); \
	if (_dst) \
		lstrcpy( _dst, _src ); \
}

#define MyZeroMemory(_ptr, _cnt) { \
	LPBYTE p; \
	for ( p = (LPBYTE)(_ptr) + (_cnt) - 1; p >= (LPBYTE)(_ptr); p-- ) *p = 0; \
}

typedef struct {
	UINT64 AllocBytes;
	UINT AllocCalls;
	UINT64 FreeBytes;
	UINT FreeCalls;
} MEMORY_STATS;
extern MEMORY_STATS g_MemStats;
