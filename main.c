
#include "main.h"
#include "utils.h"
#include "queue.h"

HINSTANCE g_hInst = NULL;
BOOL g_bInitialized = FALSE;
QUEUE g_Queue = { 0 };


/// Forward declarations
UINT_PTR __cdecl NsisMessageCallback( enum NSPIM iMessage );


//++ PluginInit
BOOL PluginInit()
{
	BOOL bRet = TRUE;
	if ( !g_bInitialized ) {

		TRACE( _T( "PluginInit\n" ) );

		UtilsInitialize();
		QueueInitialize( &g_Queue, _T("MAIN"), 2 );

		if ( TRUE ) {
			g_bInitialized = TRUE;
		} else {
			bRet = FALSE;
		}
	}
	return bRet;
}


//++ PluginUninit
BOOL PluginUninit()
{
	BOOL bRet = FALSE;
	if ( g_bInitialized ) {

		TRACE( _T( "PluginUninit\n" ) );

		QueueDestroy( &g_Queue );
		UtilsDestroy();

		g_bInitialized = FALSE;
		bRet = TRUE;
	}
	return bRet;
}


//++ NsisMessageCallback
UINT_PTR __cdecl NsisMessageCallback( enum NSPIM iMessage )
{
	switch ( iMessage )
	{
	case NSPIM_UNLOAD:
		TRACE( _T( "NSPIM_UNLOAD\n" ) );
		PluginUninit();
		break;

	case NSPIM_GUIUNLOAD:
		TRACE( _T( "NSPIM_GUIUNLOAD\n" ) );
		break;
	}
	return 0;
}


//++ Transfer
EXTERN_C __declspec(dllexport)
void __cdecl Transfer(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz = NULL;
	LPTSTR pszMethod = NULL;
	LPTSTR pszUrl = NULL, pszFile = NULL;
	ITEM_LOCAL_TYPE iLocalType = ITEM_LOCAL_NONE;
	LPTSTR pszHeaders = NULL;
	LPVOID pData = NULL;
	ULONG iDataSize = 0;
	ULONG iTimeoutConnect = DEFAULT_VALUE, iTimeoutReconnect = DEFAULT_VALUE;
	ULONG iOptConnectRetries = DEFAULT_VALUE, iOptConnectTimeout = DEFAULT_VALUE, iOptRecvTimeout = DEFAULT_VALUE;
	LPTSTR pszProxyHost = NULL, pszProxyUser = NULL, pszProxyPass = NULL;
	LPTSTR pszReferer = NULL;
	ULONG iHttpInternetFlags = DEFAULT_VALUE, iHttpSecurityFlags = DEFAULT_VALUE;
	PQUEUE_ITEM pItem = NULL;

	EXDLL_INIT();

	// Validate NSIS version
	if ( !IsCompatibleApiVersion())
		return;

	// Request unload notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

	TRACE( _T("NSdown!Transfer\n"));

	psz = (LPTSTR)MyAlloc( string_size * sizeof(TCHAR) );
	for ( ;; )
	{
		if ( popstring( psz ) != 0 )
			break;

		if ( lstrcmpi( psz, _T( "/METHOD" ) ) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				if ( lstrcmpi( psz, _T( "GET" ) ) == 0 || lstrcmpi( psz, _T( "POST" ) ) == 0 ) {
					MyFree( pszMethod );
					MyStrDup( pszMethod, psz );
				} else {
					/// TODO:
				}
			}
		}
		else if ( lstrcmpi( psz, _T( "/URL" )) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				MyFree( pszUrl );
				MyStrDup( pszUrl, psz );
			}
		}
		else if ( lstrcmpi( psz, _T( "/LOCAL" ) ) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				MyFree( pszFile );
				if ( lstrcmpi( psz, _T( "NONE" ) ) == 0 ) {
					iLocalType = ITEM_LOCAL_NONE;
				}
				else if ( lstrcmpi( psz, _T( "MEMORY" ) ) == 0 ) {
					iLocalType = ITEM_LOCAL_MEMORY;
				}
				else {
					iLocalType = ITEM_LOCAL_FILE;
					MyStrDup( pszFile, psz );
				}
			}
		}
		else if (lstrcmpi( psz, _T( "/HEADERS" ) ) == 0) {
			if (popstring( psz ) == 0) {
				MyFree( pszHeaders );
				MyStrDup( pszHeaders, psz );
			}
		}
		else if (lstrcmpi( psz, _T( "/DATA" ) ) == 0) {
			if (popstring( psz ) == 0) {
				MyFree( pData );
				iDataSize = 0;
#ifdef UNICODE
				pData = MyAlloc( string_size );
				if (pData) {
					WideCharToMultiByte( CP_ACP, 0, psz, -1, (LPSTR)pData, string_size, NULL, NULL );
					iDataSize = lstrlenA( (LPSTR)pData );	/// Don't trust what WideCharToMultiByte returns...
				}
#else
				MyStrDup( pData, psz );
				iDataSize = lstrlen( psz );
#endif
			}
		}
		else if (lstrcmpi( psz, _T( "/DATAFILE" ) ) == 0) {
			if (popstring( psz ) == 0) {
				HANDLE hFile = CreateFile( psz, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
				if (hFile != INVALID_HANDLE_VALUE) {
					ULONG iFileSize = GetFileSize( hFile, NULL );
					if (iFileSize != INVALID_FILE_SIZE || GetLastError() == ERROR_SUCCESS) {
						MyFree( pData );
						iDataSize = 0;
						pData = MyAlloc( iFileSize );
						if (pData) {
							if (!ReadFile( hFile, pData, iFileSize, &iDataSize, NULL )) {
								MyFree( pData );
								iDataSize = 0;
								assert( !"/DATAFILE: Failed to read" );
							}
						} else {
							assert( !"/DATAFILE: Failed to allocate memory" );
						}
					} else {
						assert( !"/DATAFILE: Failed to get size" );
					}
					CloseHandle( hFile );
				} else {
					assert( !"/DATAFILE: Failed to open" );
				}
			}
		} else if (lstrcmpi( psz, _T( "/TIMEOUTCONNECT" ) ) == 0) {
			iTimeoutConnect = popint();
		}
		else if ( lstrcmpi( psz, _T( "/TIMEOUTRECONNECT" ) ) == 0 ) {
			iTimeoutReconnect = popint();
		}
		else if ( lstrcmpi( psz, _T( "/OPTCONNECTRETRIES" ) ) == 0 ) {
			iOptConnectRetries = popint();
		}
		else if ( lstrcmpi( psz, _T( "/OPTCONNECTTIMEOUT" ) ) == 0 ) {
			iOptConnectTimeout = popint();
		}
		else if ( lstrcmpi( psz, _T( "/OPTRECEIVETIMEOUT" ) ) == 0 ) {
			iOptRecvTimeout = popint();
		}
		else if ( lstrcmpi( psz, _T( "/PROXY" ) ) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				MyFree( pszProxyHost );
				MyStrDup( pszProxyHost, psz );
			}
		}
		else if ( lstrcmpi( psz, _T( "/PROXYUSER" ) ) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				MyFree( pszProxyUser );
				MyStrDup( pszProxyUser, psz );
			}
		}
		else if ( lstrcmpi( psz, _T( "/PROXYPASS" ) ) == 0 ) {
			if ( popstring( psz ) == 0 ) {
				MyFree( pszProxyPass );
				MyStrDup( pszProxyPass, psz );
			}
		}
		else if (lstrcmpi( psz, _T( "/REFERER" ) ) == 0) {
			if (popstring( psz ) == 0) {
				MyFree( pszReferer );
				MyStrDup( pszReferer, psz );
			}
		}
		else if (lstrcmpi( psz, _T( "/INTERNETFLAGS" ) ) == 0) {
			ULONG iFlags = (ULONG)popint_or();
			if (iFlags != 0)
				iHttpInternetFlags = iFlags;
		}
		else if (lstrcmpi( psz, _T( "/SECURITYFLAGS" ) ) == 0) {
			ULONG iFlags = (ULONG)popint_or();
			if (iFlags != 0)
				iHttpSecurityFlags = iFlags;
		}
		else {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}

	// Add to the download queue
	QueueLock( &g_Queue );
	QueueAdd(
		&g_Queue,
		pszUrl, iLocalType, pszFile,
		pszProxyHost, pszProxyUser, pszProxyPass,
		pszMethod, pszHeaders, pData, iDataSize,
		iTimeoutConnect, iTimeoutReconnect,
		iOptConnectRetries, iOptConnectTimeout, iOptRecvTimeout,
		pszReferer,
		iHttpInternetFlags, iHttpSecurityFlags,
		&pItem
		);
	pushint( pItem ? pItem->iId : 0 );	/// Return the item's ID
	QueueUnlock( &g_Queue );

	MyFree( psz );
	MyFree( pszMethod );
	MyFree( pszUrl );
	MyFree( pszFile );
	MyFree( pszHeaders );
	MyFree( pData );
	MyFree( pszProxyHost );
	MyFree( pszProxyUser );
	MyFree( pszProxyPass );
	MyFree( pszReferer );
}


//++ QueryGlobal
EXTERN_C __declspec(dllexport)
void __cdecl QueryGlobal(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	ULONG iThreadCount;
	ULONG iItemsTotal, iItemsDone, iItemsDownloading, iItemsWaiting;
	ULONG iItemsSpeed;

	EXDLL_INIT();

	// Validate NSIS version
	if (!IsCompatibleApiVersion())
		return;

	TRACE( _T( "NSdown!QueryGlobal\n" ) );

	QueueLock( &g_Queue );
	QueueStatistics(
		&g_Queue,
		&iThreadCount,
		&iItemsTotal, &iItemsDone, &iItemsDownloading, &iItemsWaiting,
		&iItemsSpeed
		);
	QueueUnlock( &g_Queue );

	pushint( iItemsSpeed );
	pushint( iItemsWaiting );
	pushint( iItemsDownloading );
	pushint( iItemsDone );
	pushint( iItemsTotal );
	pushint( iThreadCount );
}


//++ Enumerate
EXTERN_C __declspec(dllexport)
void __cdecl Enumerate(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	#define ITEM_STATUS_UNKNOWN			((ITEM_STATUS)-1)
	#define ITEM_STATUS_ALL				((ITEM_STATUS)-2)

	LPTSTR psz;
	ITEM_STATUS iStatus = ITEM_STATUS_UNKNOWN;

	EXDLL_INIT();

	// Validate NSIS version
	if (!IsCompatibleApiVersion())
		return;

	TRACE( _T( "NSdown!Enumerate\n" ) );

	// Decide what items to enumerate
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	if (popstring( psz ) == 0) {
		if (lstrcmpi( psz, _T( "all" ) ) == 0) {
			iStatus = ITEM_STATUS_ALL;
		} else if (lstrcmpi( psz, _T( "downloading" ) ) == 0) {
			iStatus = ITEM_STATUS_DOWNLOADING;
		} else if (lstrcmpi( psz, _T( "waiting" ) ) == 0) {
			iStatus = ITEM_STATUS_WAITING;
		} else if (lstrcmpi( psz, _T( "completed" ) ) == 0) {
			iStatus = ITEM_STATUS_DONE;
			//} else if (lstrcmpi( psz, _T( "paused" ) ) == 0) {
			//	iStatus = ITEM_STATUS_PAUSED;
		}
	}
	MyFree( psz );

	// Enumerate
	if (TRUE) {
		PQUEUE_ITEM pItem;
		int iCount = 0;
		QueueLock( &g_Queue );
		for (pItem = g_Queue.pHead; pItem; pItem = pItem->pNext) {
			if (iStatus == ITEM_STATUS_ALL || iStatus == pItem->iStatus) {
				pushint( pItem->iId );
				iCount++;
			}
		}
		pushint( iCount );
		QueueUnlock( &g_Queue );
	}
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
		PluginInit();
	}
	else if ( iReason == DLL_PROCESS_DETACH ) {
		PluginUninit();
	}
	return TRUE;
}
