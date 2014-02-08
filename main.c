
#include "main.h"
#include "utils.h"
#include "queue.h"

#define USERAGENT _T("NSdown (WinInet)")
HINSTANCE g_hInst = NULL;
BOOL g_bInitialized = FALSE;


/// Forward declarations
UINT_PTR __cdecl NsisMessageCallback( enum NSPIM iMessage );


//++ PluginInit
BOOL PluginInit()
{
	BOOL bRet = TRUE;
	if ( !g_bInitialized ) {

		TRACE( _T( "PluginInit\n" ) );

		QueueInitialize();
		// TODO: Init threads

		if ( TRUE ) {
			g_bInitialized = TRUE;
		}
		else {
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

		// TODO: Cancel downloads
		// TODO: Terminate threads
		QueueDestroy();

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
	LPTSTR psz = NULL;
	LPTSTR pszUrl = NULL, pszFile = NULL;
	ITEM_LOCAL_TYPE iLocalType = ITEM_LOCAL_NONE;
	ULONG iRetryCount = DEFAULT_VALUE, iRetryDelay = DEFAULT_VALUE;
	ULONG iConnectRetries = DEFAULT_VALUE, iConnectTimeout = DEFAULT_VALUE, iRecvTimeout = DEFAULT_VALUE;
	LPTSTR pszProxyHost = NULL, pszProxyUser = NULL, pszProxyPass = NULL;
	PQUEUE_ITEM pItem = NULL;

	EXDLL_INIT();

	// Validate NSIS version
	if ( !IsCompatibleApiVersion())
		return;

	// Request unload notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

	TRACE( _T("NSdown!Download\n"));

	psz = (LPTSTR)MyAlloc( string_size * sizeof(TCHAR) );
	for ( ;; )
	{
		if ( popstring( psz ) != 0 )
			break;

		if ( lstrcmpi( psz, _T( "/URL" )) == 0 ) {
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
		else if ( lstrcmpi( psz, _T( "/RETRYCOUNT" ) ) == 0 ) {
			iRetryCount = popint();
		}
		else if ( lstrcmpi( psz, _T( "/RETRYDELAY" ) ) == 0 ) {
			iRetryDelay = popint();
		}
		else if ( lstrcmpi( psz, _T( "/CONNECTRETRIES" ) ) == 0 ) {
			iConnectRetries = popint();
		}
		else if ( lstrcmpi( psz, _T( "/CONNECTTIMEOUT" ) ) == 0 ) {
			iConnectTimeout = popint();
		}
		else if ( lstrcmpi( psz, _T( "/RECEIVETIMEOUT" ) ) == 0 ) {
			iRecvTimeout = popint();
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
		else {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}

	// Add to the download queue
	QueueLock();
	QueueAdd( pszUrl, iLocalType, pszFile, iRetryCount, iRetryDelay, iConnectRetries, iConnectTimeout, iRecvTimeout, &pItem );
	pushint( pItem ? pItem->iId : 0 );	/// Return the item's ID
	QueueUnlock();

	MyFree( psz );
	MyFree( pszUrl );
	MyFree( pszFile );
	MyFree( pszProxyHost );
	MyFree( pszProxyUser );
	MyFree( pszProxyPass );
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
