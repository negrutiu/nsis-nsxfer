#include "main.h"
#include "queue.h"
#include "utils.h"


VOID QueueInitialize(
	_Inout_ PQUEUE pQueue,
	_In_ LPCTSTR szName,
	_In_ int iThreadCount
	)
{
	int i;

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
	for ( i = 0; i < pQueue->iThreadCount; i++ ) {
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

	TRACE( _T( "  QueueInitialize(%s, ThCnt:%d)\n" ), szName, pQueue->iThreadCount );
}

VOID QueueDestroy( _Inout_ PQUEUE pQueue, _In_ BOOLEAN bForced )
{
	assert( pQueue );

	// Worker threads
	if ( pQueue->iThreadCount > 0 ) {

		/// Signal thread termination
		SetEvent( pQueue->hThreadTermEvent );

		if (bForced) {

			/// During DLL_PROCESS_DETACH only one thread is running, the others are suspended or something (check out CreateThread on MSDN)
			/// Waiting for them to close gracefully would be pointless
			int i;
			for (i = 0; i < pQueue->iThreadCount; i++) {
				if (pQueue->pThreads[i].hThread) {
					DWORD err = TerminateThread( pQueue->pThreads[i].hThread, 666 ) ? ERROR_SUCCESS : GetLastError();
					TRACE( _T( "  TerminateThread(%s) == 0x%x\n" ), pQueue->pThreads[i].szName, err );
				}
			}

		} else {

			HANDLE pObj[QUEUE_MAX_THREADS];
			int i, iObjCnt = 0;
			const ULONG QUEUE_WAIT_FOR_THREADS = 12000;

			/// Make a list of thread handles
			for (i = 0; i < pQueue->iThreadCount; i++)
				if (pQueue->pThreads[i].hThread)
					pObj[iObjCnt++] = pQueue->pThreads[i].hThread;

			TRACE( _T( "  Waiting for %d threads (max. %ums)...\n" ), iObjCnt, QUEUE_WAIT_FOR_THREADS );

			/// Wait for all threads to terminate
			if (iObjCnt > 0) {
				DWORD dwTime = dwTime = GetTickCount();
				DWORD iWait = WaitForMultipleObjects( iObjCnt, pObj, TRUE, QUEUE_WAIT_FOR_THREADS );
				dwTime = GetTickCount() - dwTime;
				if (iWait == WAIT_OBJECT_0 || iWait == WAIT_ABANDONED_0) {
					TRACE( _T( "  Threads closed in %ums\n" ), dwTime );
					MyZeroMemory( pQueue->pThreads, ARRAYSIZE( pQueue->pThreads ) * sizeof( THREAD ) );
				} else if (iWait == WAIT_TIMEOUT) {
					TRACE( _T( "  Threads failed to stop after %ums. Will terminate them forcedly\n" ), dwTime );
					for (i = 0; i < iObjCnt; i++)
						TerminateThread( pObj[i], 666 );
				} else {
					DWORD err = GetLastError();
					TRACE( _T( "  [!] WaitForMultipleObjects( ObjCnt:%d ) == 0x%x, GLE == 0x%x\n" ), iObjCnt, iWait, err );
					for (i = 0; i < iObjCnt; i++)
						TerminateThread( pObj[i], 666 );
				}
			}
		}
	}
	CloseHandle( pQueue->hThreadTermEvent );
	CloseHandle( pQueue->hThreadWakeEvent );
	pQueue->hThreadTermEvent = NULL;
	pQueue->hThreadWakeEvent = NULL;
	pQueue->iThreadCount = 0;

	// Queue
	///QueueLock( pQueue );
	QueueReset( pQueue );
	DeleteCriticalSection( &pQueue->csLock );

	// Name
	TRACE( _T( "  QueueDestroy(%s, Forced:%u)\n" ), pQueue->szName, (ULONG)bForced );
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
	// NOTES:
	// New items are always added to the front of the queue. The first (chronologically) waiting item is the last one
	// We'll select the item with the highest priority (Note: lower value means higher priority)

	PQUEUE_ITEM pItem, pSelectedItem = NULL;
	ULONG iSelectedPrio = ULONG_MAX - 1;
	assert( pQueue );
	for (pItem = pQueue->pHead; pItem; pItem = pItem->pNext)
		if (pItem->iStatus == ITEM_STATUS_WAITING && pItem->iPriority <= iSelectedPrio)
			pSelectedItem = pItem, iSelectedPrio = pItem->iPriority;
	TRACE2( _T( "  QueueFindFirstWaiting(%s) == ID:%u, Prio:%u, Ptr:0x%p\n" ), pQueue->szName, pSelectedItem ? pSelectedItem->iId : 0, pSelectedItem ? pSelectedItem->iPriority : 0, pSelectedItem );
	return pSelectedItem;
}

BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_opt_ ULONG iPriority,
	_In_ LPCTSTR pszURL,
	_In_ ITEM_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_In_opt_ LPCTSTR pszProxy,
	_In_opt_ LPCTSTR pszProxyUser,
	_In_opt_ LPCTSTR pszProxyPass,
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

			MyZeroMemory( pItem, sizeof( *pItem ) );
			pItem->iId = ++pQueue->iLastId;
			pItem->iPriority = (iPriority == DEFAULT_VALUE) ? ITEM_DEFAULT_PRIORITY : iPriority;
			pItem->iStatus = ITEM_STATUS_WAITING;
			pItem->pQueue = pQueue;

			MyStrDup( pItem->pszURL, pszURL );

			pItem->iLocalType = iLocalType;
			if (pItem->iLocalType == ITEM_LOCAL_FILE)
				MyStrDup( pItem->Local.pszFile, pszLocalFile );

			if (pszProxy && *pszProxy) {
				MyStrDup( pItem->pszProxy, pszProxy );
			}
			if (pszProxyUser && *pszProxyUser) {
				MyStrDup( pItem->pszProxyUser, pszProxyUser );
			}
			if (pszProxyPass && *pszProxyPass) {
				MyStrDup( pItem->pszProxyPass, pszProxyPass );
			}

			if ( pszMethod && *pszMethod ) {
				lstrcpyn( pItem->szMethod, pszMethod, ARRAYSIZE( pItem->szMethod ) );
			} else {
				lstrcpy( pItem->szMethod, _T("GET") );		/// Default
			}
			if (pszHeaders && *pszHeaders) {
				MyStrDup( pItem->pszHeaders, pszHeaders );
			}
			if (pData && (iDataSize > 0)) {
				MyDataDup( pItem->pData, pData, iDataSize );
				pItem->iDataSize = iDataSize;
			}
			pItem->iTimeoutConnect = iTimeoutConnect;
			pItem->iTimeoutReconnect = iTimeoutReconnect;
			pItem->iOptConnectRetries = iOptConnectRetries;
			pItem->iOptConnectTimeout = iOptConnectTimeout;
			pItem->iOptReceiveTimeout = iOptReceiveTimeout;
			if (pszReferrer && *pszReferrer) {
				MyStrDup( pItem->pszReferer, pszReferrer );
			}
			pItem->iHttpInternetFlags = iHttpInternetFlags;
			pItem->iHttpSecurityFlags = iHttpSecurityFlags;

			GetLocalFileTime( &pItem->tmEnqueue );

			pItem->iWin32Error = ERROR_SUCCESS;
			pItem->pszWin32Error = NULL;
			AllocErrorStr( pItem->iWin32Error, &pItem->pszWin32Error );

			pItem->iHttpStatus = 0;
			pItem->pszHttpStatus = NULL;
			MyStrDup( pItem->pszHttpStatus, _T( "N/A" ) );

			// Add to the front
			pItem->pNext = pQueue->pHead;
			pQueue->pHead = pItem;

			// Return
			if ( ppItem )
				*ppItem = pItem;

			// Wake up one worker thread to process this item
			SetEvent( pQueue->hThreadWakeEvent );

			TRACE(
				_T( "  QueueAdd(%s, ID:%u, %s %s -> %s, Prio:%u)\n" ),
				pQueue->szName,
				pItem->iId,
				pItem->szMethod,
				pItem->pszURL,
				pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T("Memory")),
				pItem->iPriority
				);

		} else {
			bRet = FALSE;
		}
	} else {
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
			_T( "  QueueRemove(%s, ID:%u, Err:%u \"%s\", St:%s, Speed:%s, Time:%ums, %s %s -> %s)\n" ),
			pQueue->szName,
			pItem->iId,
			pItem->iWin32Error != ERROR_SUCCESS ? pItem->iWin32Error : pItem->iHttpStatus,
			pItem->iWin32Error != ERROR_SUCCESS ? pItem->pszWin32Error : pItem->pszHttpStatus,
			pItem->iStatus == ITEM_STATUS_WAITING ? _T("Waiting") : (pItem->iStatus == ITEM_STATUS_DOWNLOADING ? _T("Downloading") : _T("Done")),
			pItem->Speed.szSpeed,
			MyTimeDiff(&pItem->tmDisconnect, &pItem->tmConnect),
			pItem->szMethod,
			pItem->pszURL,
			pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
			);

		// Free item's content
		MyFree( pItem->pszURL );
		MyFree( pItem->pszProxy );
		MyFree( pItem->pszProxyUser );
		MyFree( pItem->pszProxyPass );
		MyFree( pItem->pszHeaders );
		MyFree( pItem->pData );
		MyFree( pItem->pszSrvHeaders );
		MyFree( pItem->pszSrvIP );
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


BOOL QueueAbort( _In_ PQUEUE pQueue, _In_ PQUEUE_ITEM pItem )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pItem) {

		TRACE(
			_T( "  QueueAbort(%s, ID:%u, Prio:%u, St:%s, %s %s -> %s)\n" ),
			pQueue->szName,
			pItem->iId, pItem->iPriority,
			pItem->iStatus == ITEM_STATUS_WAITING ? _T( "Waiting" ) : (pItem->iStatus == ITEM_STATUS_DOWNLOADING ? _T( "Downloading" ) : _T( "Done" )),
			pItem->szMethod,
			pItem->pszURL,
			pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
			);

		switch (pItem->iStatus) {
		case ITEM_STATUS_WAITING:
			pItem->bAbort = TRUE;
			pItem->iStatus = ITEM_STATUS_DONE;
			break;
		case ITEM_STATUS_DOWNLOADING:
			pItem->bAbort = TRUE;
			/// The worker thread will abort soon...
			break;
		case ITEM_STATUS_DONE:
			/// Nothing to do
			break;
		default: assert( !"Unknown item status" );
		}

	} else {
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


BOOL QueueStatistics(
	_In_ PQUEUE pQueue,
	_Out_opt_ PULONG piThreadCount,
	_Out_opt_ PULONG piItemsTotal,
	_Out_opt_ PULONG piItemsDone,
	_Out_opt_ PULONG piItemsDownloading,
	_Out_opt_ PULONG piItemsWaiting,
	_Out_opt_ PULONG piItemsSpeed
	)
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pQueue) {
		PQUEUE_ITEM pItem;

		if (piThreadCount)
			*piThreadCount = (ULONG)pQueue->iThreadCount;

		if (piItemsTotal)
			*piItemsTotal = 0;
		if (piItemsDone)
			*piItemsDone = 0;
		if (piItemsDownloading)
			*piItemsDownloading = 0;
		if (piItemsWaiting)
			*piItemsWaiting = 0;
		if (piItemsSpeed)
			*piItemsSpeed = 0;

		for (pItem = pQueue->pHead; pItem; pItem = pItem->pNext) {
			if (piItemsTotal)
				(*piItemsTotal)++;
			if (piItemsDone && (pItem->iStatus == ITEM_STATUS_DONE))
				(*piItemsDone)++;
			if (piItemsDownloading && (pItem->iStatus == ITEM_STATUS_DOWNLOADING))
				(*piItemsDownloading)++;
			if (piItemsWaiting && (pItem->iStatus == ITEM_STATUS_WAITING))
				(*piItemsWaiting)++;
			if (piItemsSpeed && (pItem->iStatus == ITEM_STATUS_DOWNLOADING))
				*piItemsSpeed += pItem->Speed.iSpeed;
		}

	} else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL ItemMemoryContentToString( _In_ PQUEUE_ITEM pItem, _Out_ LPTSTR pszString, _In_ ULONG iStringLen )
{
	BOOL bRet = FALSE;
	if (pItem && pszString && iStringLen) {
		pszString[0] = 0;
		if (pItem->iLocalType == ITEM_LOCAL_MEMORY) {
			if (pItem->iStatus == ITEM_STATUS_DONE) {
				if (pItem->iWin32Error == ERROR_SUCCESS && (pItem->iHttpStatus >= 200 && pItem->iHttpStatus < 300)) {
					if (pItem->iFileSize > 0 && pItem->iFileSize != INVALID_FILE_SIZE64) {
						ULONG i, n;
						CHAR ch;
						for (i = 0, n = (ULONG)__min( iStringLen - 1, pItem->iFileSize ); i < n; i++) {
							ch = pItem->Local.pMemory[i];
							if ((ch >= 32 /*&& ch < 127*/) || ch == '\r' || ch == '\n') {
								pszString[i] = pItem->Local.pMemory[i];
							} else {
								pszString[i] = _T( '.' );
							}
						}
						pszString[i] = 0;	/// NULL terminator
						bRet = TRUE;
					}
				}
			}
		}
	}
	return bRet;
}
