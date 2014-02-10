#include "main.h"
#include "thread.h"
#include "utils.h"
#include "queue.h"

THREAD g_Thread = { 0 };


DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem );


//++ ThreadInitialize
DWORD ThreadInitialize( _Inout_ PTHREAD pThread )
{
	TRACE( _T( "  ThreadInitialize()\n" ) );
	assert( pThread );

	lstrcpy( pThread->szName, _T( "MAIN" ) );
	pThread->hTermEvent = CreateEvent( NULL, TRUE, FALSE, NULL );		/// manual
	pThread->hWakeEvent = CreateEvent( NULL, FALSE, FALSE, NULL );		/// automatic
	pThread->hThread = CreateThread( NULL, 0, ThreadProc, pThread, 0, &pThread->iTID );
	if ( !pThread->hThread ) {
		DWORD err = GetLastError();
		TRACE( _T( "  [!] CreateThread( ThreadProc ) == 0x%x\n" ), err );
	}

	assert( pThread->hThread );
	assert( pThread->hTermEvent );
	assert( pThread->hWakeEvent );
	return ERROR_SUCCESS;
}


//++ ThreadDestroy
DWORD ThreadDestroy( _Inout_ PTHREAD pThread, _In_opt_ ULONG iTimeout )
{
	DWORD err = ERROR_SUCCESS;
	DWORD iWait, dwTime;

	assert( pThread );
	TRACE( _T( "  ThreadDestroy()\n" ) );

	SetEvent( pThread->hTermEvent );

	dwTime = GetTickCount();
	iWait = WaitForSingleObject( pThread->hThread, (iTimeout > 0) ? iTimeout : INFINITE );
	dwTime = GetTickCount() - dwTime;
	if ( iWait == WAIT_OBJECT_0 || iWait == WAIT_ABANDONED ) {
		TRACE( _T( "  Thread closed in %ums\n" ), dwTime );
	}
	else if ( iWait == WAIT_TIMEOUT ) {
		err = ERROR_TIMEOUT;
		TRACE( _T( "  Thread %04x timed out (%ums). Terminating forcedly...\n" ), pThread->iTID, dwTime );
		TerminateThread( pThread->hThread, 666 );
	}
	else {
		err = GetLastError();
		TRACE( _T( "  [!] WaitForSingleObject( Thread ) == 0x%x, GLE == 0x%x\n" ), iWait, err );
		TerminateThread( pThread->hThread, 777 );
	}

	CloseHandle( pThread->hThread );
	CloseHandle( pThread->hTermEvent );
	CloseHandle( pThread->hWakeEvent );

	pThread->hThread = NULL;
	pThread->hTermEvent = NULL;
	pThread->hWakeEvent = NULL;

	return err;
}


//++ ThreadWake
VOID ThreadWake( _Inout_ PTHREAD pThread )
{
	assert( pThread );
	TRACE( _T( "  ThreadWake()\n" ) );
	SetEvent( pThread->hWakeEvent );
}


//++ ThreadProc
DWORD WINAPI ThreadProc( _In_ PTHREAD pThread )
{
	HANDLE handles[2];
	PQUEUE_ITEM pItem;

	assert( pThread );
	assert( pThread->hTermEvent );
	assert( pThread->hWakeEvent );

	TRACE( _T( "  Thread start\n" ) );

	handles[0] = pThread->hTermEvent;
	handles[1] = pThread->hWakeEvent;

	while ( TRUE )
	{
		// Wait for something to happen
		DWORD iWait = WaitForMultipleObjects( 2, handles, FALSE, INFINITE );
		if ( iWait == WAIT_OBJECT_0 ) {
			// TERM event
			TRACE( _T( "  TERM event\n" ) );
			break;
		}
		else if ( iWait == WAIT_OBJECT_0 + 1 ) {
			// WAKE event
			TRACE( _T( "  WAKE event\n" ) );
		}
		else {
			// Some error...
			DWORD err = GetLastError();
			TRACE( _T( "  [!] WaitForMultipleObjects(...) == %u, GLE == 0x%x" ), iWait, err );
			break;
		}

		// Dequeue the first waiting item
		QueueLock( &g_Queue );
		pItem = QueueFindFirstWaiting( &g_Queue );
		if ( pItem ) {
			SYSTEMTIME st;
			GetLocalTime( &st );
			SystemTimeToFileTime( &st, &pItem->tmDownloadStart );
			pItem->iStatus = ITEM_STATUS_DOWNLOADING;
		}
		QueueUnlock( &g_Queue );

		// Start downloading
		if ( pItem ) {
			ThreadDownload( pThread, pItem );
		}
	}

	TRACE( _T( "  Thread terminate\n" ) );
	return 0;
}


//++ ThreadDownload
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem )
{
	assert( pThread && pItem );
	TRACE(
		_T( "  ThreadDownload(Th:%s, ID:%u, %s -> %s)\n" ),
		pThread->szName,
		pItem->iId,
		pItem->pszURL,
		pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->LocalData.pszFile : _T( "Memory" ))
		);
}