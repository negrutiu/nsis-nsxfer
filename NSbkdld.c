
#if DBG || _DEBUG || DEBUG
	#define PLUGIN_DEBUG
#endif

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <wininet.h>
#ifdef PLUGIN_DEBUG
	#include <Shlwapi.h>			/// for wvnsprintf
#endif
#include "nsiswapi/pluginapi.h"


#define USERAGENT _T("NSIS NSbkdld (WinInet)")
HINSTANCE g_hInst = NULL;

//
// Download queue stuff.
//
typedef enum {
	ITEM_STATUS_WAITING,			/// The item is waiting in queue. Not downloaded yet
	ITEM_STATUS_DOWNLOADING,		/// The item is being downloaded
	ITEM_STATUS_DONE				/// The item has been downloaded (successful or not)
} ITEM_STATUS;

typedef struct _QUEUE_ITEM {

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

	ULONG iId;						/// Unique download ID
	ITEM_STATUS iStatus;

	TCHAR szURL[1024];
	TCHAR szFile[1024];
	ULONG iRetryCount;				/// 0 if not used
	ULONG iRetryDelay;				/// 0 if not used

	BOOL bErrorCodeHTTP;			/// TRUE == HTTP status code, FALSE == Win32 error code
	ULONG iErrorCode;

	struct {
		FILETIME tmEnqueue;			/// Enqueue time
		FILETIME tmDownloadStart;	/// Download startup time
		FILETIME tmDownloadEnd;		/// Download startup time
		ULONG64 iFileSize;			/// File size or -1 if not available
		ULONG64 iRecvSize;			/// Received bytes
	} Stats;

} QUEUE_ITEM;


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


//++ NsisMessageCallback
UINT_PTR __cdecl NsisMessageCallback(enum NSPIM iMessage)
{
	switch (iMessage) 
	{
	case NSPIM_UNLOAD:
		/// TODO: Cancel downloads
		break;
	case NSPIM_GUIUNLOAD:
		break;
	}
	return 0;
}


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

	// Validate NSIS version
	if ( !IsCompatibleApiVersion())
		return;

	// Request unload notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

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
