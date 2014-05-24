#include "main.h"
#include "queue.h"
#include "utils.h"


VOID QueueInitialize(
	_Inout_ PQUEUE pQueue,
	_In_ LPCTSTR szName,
	_In_ int iThreadCount
	)
{
	assert( pQueue );
	assert( szName && *szName );
	assert( iThreadCount < QUEUE_MAX_THREADS );

	// Name
	if ( szName && *szName ) {
		lstrcpyn( pQueue->szName, szName, ARRAYSIZE( pQueue->szName ) );
	} else {
		wnsprintf( pQueue->szName, ARRAYSIZE( pQueue->szName ), _T( "%p" ), pQueue );
	}

	// Queue
	InitializeCriticalSectionAndSpinCount( &pQueue->csLock, 3000 );
	pQueue->pHead = NULL;
	pQueue->iLastId = 0;

	// Worker threads
	pQueue->iThreadCount = __min( iThreadCount, QUEUE_MAX_THREADS - 1 );
	pQueue->hThreadTermEvent = CreateEvent( NULL, TRUE, FALSE, NULL );	/// Manual
	pQueue->hThreadWakeEvent = CreateEvent( NULL, FALSE, FALSE, NULL );	/// Automatic
	assert( pQueue->hThreadTermEvent );
	assert( pQueue->hThreadWakeEvent );
	for ( int i = 0; i < pQueue->iThreadCount; i++ ) {
		PTHREAD pThread = pQueue->pThreads + i;
		pThread->pQueue = pQueue;
		pThread->hTermEvent = pQueue->hThreadTermEvent;
		pThread->hWakeEvent = pQueue->hThreadWakeEvent;
		wnsprintf( pThread->szName, ARRAYSIZE( pThread->szName ), _T( "%s%02d" ), pQueue->szName, i );
		pThread->hThread = CreateThread( NULL, 0, ThreadProc, pThread, 0, &pThread->iTID );
		assert( pThread->hThread );
		if ( !pThread->hThread ) {
			DWORD err = GetLastError();
			TRACE( _T( "  [!] CreateThread(%s) == 0x%x\n" ), pThread->szName, err );
			MyZeroMemory( pThread, sizeof(pThread) );
		}
	}

	TRACE( _T( "  QueueInitialize(%s, ThCnt:%d)\n" ), szName, iThreadCount );
}

VOID QueueDestroy( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );

	// Worker threads
	if ( pQueue->iThreadCount > 0 ) {

		HANDLE pObj[QUEUE_MAX_THREADS];
		int iObjCnt = 0;

		/// Signal thread termination
		SetEvent( pQueue->hThreadTermEvent );

		/// Make a list of thread handles
		for ( int i = 0; i < pQueue->iThreadCount; i++ )
			if ( pQueue->pThreads[i].hThread )
				pObj[iObjCnt++] = pQueue->pThreads[i].hThread;
		if ( iObjCnt > 0 ) {

			/// Wait for all threads to terminate
			DWORD dwTime = dwTime = GetTickCount();
			DWORD iWait = WaitForMultipleObjects( iObjCnt, pObj, TRUE, 12000 );
			dwTime = GetTickCount() - dwTime;
			if ( iWait == WAIT_OBJECT_0 || iWait == WAIT_ABANDONED_0 ) {
				TRACE( _T( "  Threads closed in %ums\n" ), dwTime );
				MyZeroMemory( pQueue->pThreads, ARRAYSIZE( pQueue->pThreads ) * sizeof(THREAD) );
			} else if ( iWait == WAIT_TIMEOUT ) {
				TRACE( _T( "  Threads failed to stop after %ums. Will terminate them forcedly\n" ), dwTime );
				for ( int i = 0; i < iObjCnt; i++ )
					TerminateThread( pObj[i], 666 );
			} else {
				DWORD err = GetLastError();
				TRACE( _T( "  [!] WaitForMultipleObjects( ObjCnt:%d ) == 0x%x, GLE == 0x%x\n" ), iObjCnt, iWait, err );
				for ( int i = 0; i < iObjCnt; i++ )
					TerminateThread( pObj[i], 666 );
			}
		}
	}
	CloseHandle( pQueue->hThreadTermEvent );
	CloseHandle( pQueue->hThreadWakeEvent );
	pQueue->hThreadTermEvent = NULL;
	pQueue->hThreadWakeEvent = NULL;
	pQueue->iThreadCount = 0;

	// Queue
	QueueLock( pQueue );
	QueueReset( pQueue );
	DeleteCriticalSection( &pQueue->csLock );

	// Name
	TRACE( _T( "  QueueDestroy(%s)\n" ), pQueue->szName );
	*pQueue->szName = _T( '\0' );
}

VOID QueueLock( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	if ( !TryEnterCriticalSection( &pQueue->csLock ) )
		EnterCriticalSection( &pQueue->csLock );
	TRACE2( _T( "  QueueLock(%s)\n" ), pQueue->szName );
}

VOID QueueUnlock( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	LeaveCriticalSection( &pQueue->csLock );
	TRACE2( _T( "  QueueUnlock(%s)\n" ), pQueue->szName );
}

BOOL QueueReset( _Inout_ PQUEUE pQueue )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	TRACE( _T( "  QueueReset(%s)\n" ), pQueue->szName );
	while ( pQueue->pHead )
		bRet = bRet && QueueRemove( pQueue, pQueue->pHead );
	pQueue->iLastId = 0;
	return bRet;
}

PQUEUE_ITEM QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iItemID )
{
	PQUEUE_ITEM pItem;
	assert( pQueue );
	for ( pItem = pQueue->pHead; pItem && pItem->iId != iItemID; pItem = pItem->pNext );
	TRACE2( _T( "  QueueFind(%s, ID:%u) == 0x%p\n" ), pQueue->szName, iItemID, pItem );
	return pItem;
}

PQUEUE_ITEM QueueFindFirstWaiting( _Inout_ PQUEUE pQueue )
{
	// New items are always added to the front of our queue
	// Therefore, the first (chronologically) waiting item is the last in the queue

	PQUEUE_ITEM pItem, pLastWaitingItem;
	assert( pQueue );
	for ( pItem = pQueue->pHead, pLastWaitingItem = NULL; pItem; pItem = pItem->pNext )
		if ( pItem->iStatus == ITEM_STATUS_WAITING )
			pLastWaitingItem = pItem;
	TRACE2( _T( "  QueueFindFirstWaiting(%s) == ID:%u, Ptr:0x%p\n" ), pQueue->szName, pLastWaitingItem ? pLastWaitingItem->iId : 0, pLastWaitingItem );
	return pLastWaitingItem;
}

BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_ LPCTSTR pszURL,
	_In_ ITEM_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_In_opt_ ULONG iRetryCount,
	_In_opt_ ULONG iRetryDelay,
	_In_opt_ ULONG iConnectRetries,
	_In_opt_ ULONG iConnectTimeout,
	_In_opt_ ULONG iReceiveTimeout,
	_Outptr_opt_ PQUEUE_ITEM *ppItem
	)
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if ( pszURL && *pszURL && ((iLocalType != ITEM_LOCAL_FILE) || (pszLocalFile && *pszLocalFile)) ) {

		PQUEUE_ITEM pItem = (PQUEUE_ITEM)MyAlloc( sizeof(QUEUE_ITEM) );
		if ( pItem ) {

			pItem->iId = ++pQueue->iLastId;
			pItem->iStatus = ITEM_STATUS_WAITING;
			MyStrDup( pItem->pszURL, pszURL );

			pItem->iLocalType = iLocalType;
			switch ( iLocalType )
			{
			case ITEM_LOCAL_NONE:
				pItem->LocalData.pszFile = NULL;
				///pItem->LocalData.pMemory = NULL;
				break;
			case ITEM_LOCAL_FILE:
				MyStrDup( pItem->LocalData.pszFile, pszLocalFile );
				pItem->LocalData.hFile = NULL;
				break;
			case ITEM_LOCAL_MEMORY:
				pItem->LocalData.pMemory = NULL;	/// The memory buffer will be allocated later, when we'll know the file size
				break;
			default:
				TRACE( _T( "  [!] Unknown item type %d\n" ), (int)iLocalType );
			}

			pItem->iRetryCount = iRetryCount;
			pItem->iRetryDelay = iRetryDelay;
			pItem->iConnectRetries = iConnectRetries;
			pItem->iConnectTimeout = iConnectTimeout;
			pItem->iReceiveTimeout = iReceiveTimeout;
			pItem->bResume = TRUE;

			GetLocalFileTime( &pItem->tmEnqueue );
			pItem->tmDownloadStart.dwLowDateTime = 0;
			pItem->tmDownloadStart.dwHighDateTime = 0;
			pItem->tmDownloadEnd.dwLowDateTime = 0;
			pItem->tmDownloadEnd.dwHighDateTime = 0;
			pItem->iFileSize = 0;
			pItem->iRecvSize = 0;

			pItem->hSession = NULL;
			pItem->hConnect = NULL;

			pItem->bErrorCodeIsHTTP = FALSE;
			pItem->iErrorCode = ERROR_SUCCESS;
			pItem->pszErrorText = NULL;

			// Add in front
			pItem->pNext = pQueue->pHead;
			pQueue->pHead = pItem;

			// Return
			if ( ppItem )
				*ppItem = pItem;

			// Wake up one worker thread to process this item
			SetEvent( pQueue->hThreadWakeEvent );

			TRACE(
				_T( "  QueueAdd(%s, ID:%u, %s -> %s)\n" ),
				pQueue->szName,
				pItem->iId,
				pItem->pszURL,
				pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->LocalData.pszFile : _T("Memory"))
				);
		}
		else {
			bRet = FALSE;
		}
	}
	else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_ITEM pItem )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if ( pItem ) {

		TRACE(
			_T( "  QueueRemove(%s, ID:%u, Err:%u \"%s\", St:%s, %s -> %s)\n" ),
			pQueue->szName,
			pItem->iId,
			pItem->iErrorCode,
			pItem->pszErrorText ? pItem->pszErrorText : _T("n/a"),
			pItem->iStatus == ITEM_STATUS_WAITING ? _T("Waiting") : (pItem->iStatus == ITEM_STATUS_DOWNLOADING ? _T("Downloading") : _T("Done")),
			pItem->pszURL,
			pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->LocalData.pszFile : _T( "Memory" ))
			);

		// Free item's content
		MyFree( pItem->pszURL );
		MyFree( pItem->pszErrorText );

		switch ( pItem->iLocalType )
		{
		case ITEM_LOCAL_FILE:
			MyFree( pItem->LocalData.pszFile );
			if ( pItem->LocalData.hFile != NULL && pItem->LocalData.hFile != INVALID_HANDLE_VALUE )
				CloseHandle( pItem->LocalData.hFile );
			break;
		case ITEM_LOCAL_MEMORY:
			MyFree( pItem->LocalData.pMemory );
			break;
		}

		// Remove from list
		{
			PQUEUE_ITEM pPrevItem;
			for ( pPrevItem = pQueue->pHead; (pPrevItem != NULL) && (pPrevItem->pNext != pItem); pPrevItem = pPrevItem->pNext );
			if ( pPrevItem ) {
				pPrevItem->pNext = pItem->pNext;
			} else {
				pQueue->pHead = pItem->pNext;
			}
		}

		// Destroy the item
		MyFree( pItem );
	}
	else {
		bRet = FALSE;
	}
	return bRet;
}

ULONG QueueSize( _Inout_ PQUEUE pQueue )
{
	PQUEUE_ITEM pItem;
	ULONG iSize;
	assert( pQueue );
	for ( pItem = pQueue->pHead, iSize = 0; pItem; pItem = pItem->pNext, iSize++ );
	TRACE2( _T( "  QueueSize(%s) == %u\n" ), pQueue->szName, iSize );
	return iSize;
}
