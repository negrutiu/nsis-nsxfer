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
	_In_opt_ LPCTSTR pszMethod,
	_In_opt_ LPCTSTR pszHeaders,
	_In_opt_ LPVOID pData,
	_In_opt_ ULONG iDataSize,
	_In_opt_ ULONG iTimeoutConnect,
	_In_opt_ ULONG iTimeoutReconnect,
	_In_opt_ ULONG iOptConnectRetries,
	_In_opt_ ULONG iOptConnectTimeout,
	_In_opt_ ULONG iOptReceiveTimeout,
	_In_opt_ LPCTSTR pszReferrer,
	_In_opt_ ULONG iHttpInternetFlags,
	_In_opt_ ULONG iHttpSecurityFlags,
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
			pItem->pQueue = pQueue;
			pItem->pThread = NULL;

			MyStrDup( pItem->pszURL, pszURL );

			pItem->iLocalType = iLocalType;
			switch ( iLocalType )
			{
			case ITEM_LOCAL_NONE:
				pItem->Local.pszFile = NULL;
				///pItem->Local.pMemory = NULL;
				break;
			case ITEM_LOCAL_FILE:
				MyStrDup( pItem->Local.pszFile, pszLocalFile );
				pItem->Local.hFile = NULL;
				break;
			case ITEM_LOCAL_MEMORY:
				pItem->Local.pMemory = NULL;	/// The memory buffer will be allocated later, when we'll know the file size
				break;
			default:
				TRACE( _T( "  [!] Unknown item type %d\n" ), (int)iLocalType );
			}

			if ( pszMethod && *pszMethod ) {
				lstrcpyn( pItem->szMethod, pszMethod, ARRAYSIZE( pItem->szMethod ) );
			} else {
				lstrcpy( pItem->szMethod, _T("GET") );		/// Default
			}
			if (pszHeaders && *pszHeaders) {
				MyStrDup( pItem->pszHeaders, pszHeaders );
			} else {
				pItem->pszHeaders = NULL;
			}
			if (pData && (iDataSize > 0)) {
				MyDataDup( pItem->pData, pData, iDataSize );
				pItem->iDataSize = iDataSize;
			} else {
				pItem->pData = NULL;
				pItem->iDataSize = 0;
			}
			pItem->iTimeoutConnect = iTimeoutConnect;
			pItem->iTimeoutReconnect = iTimeoutReconnect;
			pItem->iOptConnectRetries = iOptConnectRetries;
			pItem->iOptConnectTimeout = iOptConnectTimeout;
			pItem->iOptReceiveTimeout = iOptReceiveTimeout;
			if (pszReferrer && *pszReferrer) {
				MyStrDup( pItem->pszReferer, pszReferrer );
			} else {
				pItem->pszReferer = NULL;
			}
			pItem->iHttpInternetFlags = iHttpInternetFlags;
			pItem->iHttpSecurityFlags = iHttpSecurityFlags;

			GetLocalFileTime( &pItem->tmEnqueue );
			pItem->tmTransferStart.dwLowDateTime = 0;
			pItem->tmTransferStart.dwHighDateTime = 0;
			pItem->tmTransferEnd.dwLowDateTime = 0;
			pItem->tmTransferEnd.dwHighDateTime = 0;
			pItem->iFileSize = 0;
			pItem->iRecvSize = 0;
			pItem->bResumeNeedsValidation = FALSE;

			pItem->hSession = NULL;
			pItem->hConnect = NULL;
			pItem->hRequest = NULL;

			pItem->iWin32Error = ERROR_SUCCESS;
			pItem->pszWin32Error = NULL;
			AllocErrorStr( pItem->iWin32Error, &pItem->pszWin32Error );

			pItem->iHttpStatus = 666;
			pItem->pszHttpStatus = NULL;
			MyStrDup( pItem->pszHttpStatus, _T( "N/A" ) );

			// Add in front
			pItem->pNext = pQueue->pHead;
			pQueue->pHead = pItem;

			// Return
			if ( ppItem )
				*ppItem = pItem;

			// Wake up one worker thread to process this item
			SetEvent( pQueue->hThreadWakeEvent );

			TRACE(
				_T( "  QueueAdd(%s, ID:%u, %s %s -> %s)\n" ),
				pQueue->szName,
				pItem->iId,
				pItem->szMethod,
				pItem->pszURL,
				pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T("Memory"))
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
			_T( "  QueueRemove(%s, ID:%u, Err:%u \"%s\", St:%s, %s %s -> %s)\n" ),
			pQueue->szName,
			pItem->iId,
			pItem->iWin32Error != ERROR_SUCCESS ? pItem->iWin32Error : pItem->iHttpStatus,
			pItem->iWin32Error != ERROR_SUCCESS ? pItem->pszWin32Error : pItem->pszHttpStatus,
			pItem->iStatus == ITEM_STATUS_WAITING ? _T("Waiting") : (pItem->iStatus == ITEM_STATUS_DOWNLOADING ? _T("Downloading") : _T("Done")),
			pItem->szMethod,
			pItem->pszURL,
			pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
			);

		// Free item's content
		MyFree( pItem->pszURL );
		MyFree( pItem->pszHeaders );
		MyFree( pItem->pData );
		MyFree( pItem->pszWin32Error );
		MyFree( pItem->pszHttpStatus );
		MyFree( pItem->pszReferer );

		switch ( pItem->iLocalType )
		{
		case ITEM_LOCAL_FILE:
			MyFree( pItem->Local.pszFile );
			if ( pItem->Local.hFile != NULL && pItem->Local.hFile != INVALID_HANDLE_VALUE )
				CloseHandle( pItem->Local.hFile );
			break;
		case ITEM_LOCAL_MEMORY:
			MyFree( pItem->Local.pMemory );
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
