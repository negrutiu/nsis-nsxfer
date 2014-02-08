
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
	PQUEUE_ITEM pItem;

	EXDLL_INIT();

	// Validate NSIS version
	if ( !IsCompatibleApiVersion())
		return;

	// Request unload notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

	TRACE( _T("Download: TODO\n"));

	QueueLock();
	QueueAdd( _T( "MyURL" ), ITEM_LOCAL_NONE, _T( "adf" ), &pItem );
	QueueAdd( _T( "MyURL" ), ITEM_LOCAL_FILE, _T( "fdsa" ), &pItem );
	QueueAdd( _T( "MyURL" ), ITEM_LOCAL_MEMORY, _T( "fdsa" ), &pItem );
	QueueSize();
	QueueFindFirstWaiting();
	QueueReset();
	QueueUnlock();
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
