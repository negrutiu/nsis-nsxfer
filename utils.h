
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2014/02/02

#pragma once

//+ Init functions
VOID UtilsInitialize();
VOID UtilsDestroy();


//+ TRACE
#if DBG || _DEBUG
	#define TRACE_ENABLED
#endif

#if defined (TRACE_ENABLED)
	#define TRACE TraceImpl
	#define TRACE2(...)			/// More verbose tracing
	VOID TraceImpl( __in LPCTSTR pszFormat, ... );

	#define TRACE_CALL TraceCallImpl
	VOID TraceCallImpl( __in stack_t **ppStackTop, __in LPCTSTR pszPrefix );
#else
	#define TRACE_CALL(...)
	#define TRACE(...)
	#define TRACE2(...)
#endif

//+ assert
#if DBG || _DEBUG
	#define assert(expr) \
		if ( !(expr)) { \
			TCHAR szMsg[512]; \
			TRACE( _T("  [!] %s, %s:%u\n"), _T(#expr), _T( __FILE__ ), __LINE__ ); \
			wnsprintf( szMsg, (int)ARRAYSIZE( szMsg ), _T("%s\n%s : %u\n"), _T(#expr), _T( __FILE__ ), __LINE__ ); \
			if (MessageBox( NULL, szMsg, _T("ASSERT"), MB_ICONERROR|MB_OKCANCEL ) != IDOK) \
				ExitProcess( 666 ); \
		}
	#define verify assert
#else
	#define assert(...)
	#define verify(expr) ((VOID)(expr))
#endif

//+ Utils
BOOL GetLocalFileTime( _Out_ LPFILETIME lpFT );
VOID AllocErrorStr( _In_ DWORD dwErrCode, _Out_ TCHAR **ppszErrText );		/// Call MyFree(ppszErrText) when no longer needed

//+ Memory
/// We'll use global memory, to be compatible with NSIS API
#define MyAlloc(_size)		GlobalLock(GlobalAlloc(GMEM_FIXED, _size)); g_MemStats.AllocBytes += (_size); g_MemStats.AllocCalls++
#define MyAllocStr(_len)	(LPTSTR)MyAlloc( ((_len) + 1) * sizeof(TCHAR))
#define MyFree(_ptr)		if ( _ptr ) { g_MemStats.FreeBytes += GlobalSize((HGLOBAL)(_ptr)); g_MemStats.FreeCalls++; GlobalFree((HGLOBAL)(_ptr)); (_ptr) = NULL; }

#define MyDataDup(_dst, _src, _size) { \
	(_dst) = MyAlloc( _size ); \
	if (_dst) { \
		LPBYTE pDst = (LPBYTE)(_dst); \
		LPBYTE pSrc = (LPBYTE)(_src); \
		ULONG i; \
		for ( i = 0; i < (_size); i++, pDst++, pSrc++ ) \
			*pDst = *pSrc; \
	} \
}

#define MyStrDup(_dst, _src) { \
	(_dst) = MyAllocStr( lstrlen( _src )); \
	if (_dst) \
		lstrcpy( (LPTSTR)(_dst), (LPTSTR)(_src) ); \
}

#define MyMakeUpper(_dst) { \
	if (_dst) { \
	    TCHAR *p; \
		for (p = (TCHAR*)(_dst); *p; p++) { \
		    if (*p >= 'a' && *p <= 'z') { \
			    *p -= ('a' - 'A'); \
			} \
		} \
	} \
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


#define VALID_FILE_HANDLE(h) \
	((h) != NULL) && ((h) != INVALID_HANDLE_VALUE)


//+ ULONGLONG <-> double
double MyUlonglongToDouble( __in ULONGLONG ull );
ULONGLONG MyDoubleToUlonglong( __in double d );

//+ MyDiv64
ULONGLONG MyDiv64( __in ULONGLONG iNumerator, __in ULONGLONG iDenominator );

//+ MyMulDiv64
ULONGLONG MyMulDiv64( __in ULONGLONG iNumber, __in ULONGLONG iNumerator, __in ULONGLONG iDenominator );

//+ MyTimeDiff
// Returns milliseconds
ULONG MyTimeDiff( __in PFILETIME pEndTime, __in PFILETIME pStartTime );

//+ ReadVersionInfoString
// Returns Win32 error
DWORD ReadVersionInfoString( __in_opt LPCTSTR szFile, __in LPCTSTR szStringName, __out LPTSTR szStringValue, __in UINT iStringValueLen );

//+ BinaryToString
// Returns number of TCHAR-s written, not including the NULL terminator
ULONG BinaryToString( __in LPVOID pData, __in ULONG iDataSize, __out LPTSTR pszStr, __in ULONG iStrLen );

//+ RegXxxDWORD
// Returns Win32 error
DWORD RegReadDWORD( __in HKEY hRoot, __in LPCTSTR pszKey, __in LPCTSTR pszValue, __out LPDWORD pdwValue );
DWORD RegWriteDWORD( __in HKEY hRoot, __in LPCTSTR pszKey, __in LPCTSTR pszValue, __in DWORD dwValue );

//+ MyStrToInt64
// Replacement for shlwapi!StrToInt64Ex introduced in "Update Rollup 1 for Windows 2000 SP4"
BOOL MyStrToInt64( _In_ LPCTSTR pszStr, _Out_ PUINT64 piNum );

//+ MyCreateDirectory
ULONG MyCreateDirectory( _In_ LPCTSTR pszPath, _In_ BOOLEAN bHasFilename );
