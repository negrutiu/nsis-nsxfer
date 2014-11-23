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
	assert( iThreadCount < MAX_WORKER_THREADS );

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
	pQueue->iThreadCount = __min( iThreadCount, MAX_WORKER_THREADS - 1 );
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

			HANDLE pObj[MAX_WORKER_THREADS];
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

PQUEUE_ITEM QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iReqID )
{
	PQUEUE_ITEM pReq;
	assert( pQueue );
	for (pReq = pQueue->pHead; pReq && pReq->iId != iReqID; pReq = pReq->pNext);
	TRACE2( _T( "  QueueFind(%s, ID:%u) == 0x%p\n" ), pQueue->szName, iReqID, pReq );
	return pReq;
}

PQUEUE_ITEM QueueFindFirstWaiting( _Inout_ PQUEUE pQueue )
{
	// NOTES:
	// New requests are always added to the front of the queue. The first (chronologically) waiting request is the last one
	// We'll select the request with the highest priority (Note: lower value means higher priority)

	PQUEUE_ITEM pReq, pSelectedReq = NULL;
	ULONG iSelectedPrio = ULONG_MAX - 1;
	assert( pQueue );
	for (pReq = pQueue->pHead; pReq; pReq = pReq->pNext)
		if (pReq->iStatus == REQUEST_STATUS_WAITING && !pReq->bAbort && pReq->iPriority <= iSelectedPrio)
			pSelectedReq = pReq, iSelectedPrio = pReq->iPriority;
	TRACE2( _T( "  QueueFindFirstWaiting(%s) == ID:%u, Prio:%u, Ptr:0x%p\n" ), pQueue->szName, pSelectedReq ? pSelectedReq->iId : 0, pSelectedReq ? pSelectedReq->iPriority : 0, pSelectedReq );
	return pSelectedReq;
}

BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_opt_ ULONG iPriority,
	_In_ LPCTSTR pszURL,
	_In_ REQUEST_LOCAL_TYPE iLocalType,
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
	_Outptr_opt_ PQUEUE_ITEM *ppReq
	)
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pszURL && *pszURL && ((iLocalType != REQUEST_LOCAL_FILE) || (pszLocalFile && *pszLocalFile))) {

		PQUEUE_ITEM pReq = (PQUEUE_ITEM)MyAlloc( sizeof( QUEUE_ITEM ) );
		if (pReq) {

			MyZeroMemory( pReq, sizeof( *pReq ) );
			pReq->iId = ++pQueue->iLastId;
			pReq->iPriority = (iPriority == DEFAULT_VALUE) ? DEFAULT_PRIORITY : iPriority;
			pReq->iStatus = REQUEST_STATUS_WAITING;
			pReq->pQueue = pQueue;

			MyStrDup( pReq->pszURL, pszURL );

			pReq->iLocalType = iLocalType;
			if (pReq->iLocalType == REQUEST_LOCAL_FILE)
				MyStrDup( pReq->Local.pszFile, pszLocalFile );

			if (pszProxy && *pszProxy) {
				MyStrDup( pReq->pszProxy, pszProxy );
			}
			if (pszProxyUser && *pszProxyUser) {
				MyStrDup( pReq->pszProxyUser, pszProxyUser );
			}
			if (pszProxyPass && *pszProxyPass) {
				MyStrDup( pReq->pszProxyPass, pszProxyPass );
			}

			if ( pszMethod && *pszMethod ) {
				lstrcpyn( pReq->szMethod, pszMethod, ARRAYSIZE( pReq->szMethod ) );
			} else {
				lstrcpy( pReq->szMethod, _T( "GET" ) );		/// Default
			}
			if (pszHeaders && *pszHeaders) {
				MyStrDup( pReq->pszHeaders, pszHeaders );
			}
			if (pData && (iDataSize > 0)) {
				MyDataDup( pReq->pData, pData, iDataSize );
				pReq->iDataSize = iDataSize;
			}
			pReq->iTimeoutConnect = iTimeoutConnect;
			pReq->iTimeoutReconnect = iTimeoutReconnect;
			pReq->iOptConnectRetries = iOptConnectRetries;
			pReq->iOptConnectTimeout = iOptConnectTimeout;
			pReq->iOptReceiveTimeout = iOptReceiveTimeout;
			if (pszReferrer && *pszReferrer) {
				MyStrDup( pReq->pszReferer, pszReferrer );
			}
			pReq->iHttpInternetFlags = iHttpInternetFlags;
			pReq->iHttpSecurityFlags = iHttpSecurityFlags;

			GetLocalFileTime( &pReq->tmEnqueue );

			pReq->iWin32Error = ERROR_SUCCESS;
			pReq->pszWin32Error = NULL;
			AllocErrorStr( pReq->iWin32Error, &pReq->pszWin32Error );

			pReq->iHttpStatus = 0;
			pReq->pszHttpStatus = NULL;
			MyStrDup( pReq->pszHttpStatus, _T( "N/A" ) );

			// Add to the front
			pReq->pNext = pQueue->pHead;
			pQueue->pHead = pReq;

			// Return
			if (ppReq)
				*ppReq = pReq;

			// Wake up one worker thread
			SetEvent( pQueue->hThreadWakeEvent );

			TRACE(
				_T( "  QueueAdd(%s, ID:%u, %s %s -> %s, Prio:%u)\n" ),
				pQueue->szName,
				pReq->iId,
				pReq->szMethod,
				pReq->pszURL,
				pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY),
				pReq->iPriority
				);

		} else {
			bRet = FALSE;
		}
	} else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_ITEM pReq )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pReq) {

		TRACE(
			_T( "  QueueRemove(%s, ID:%u, Err:%u \"%s\", St:%s, Speed:%s, Time:%ums, %s %s -> %s)\n" ),
			pQueue->szName,
			pReq->iId,
			pReq->iWin32Error != ERROR_SUCCESS ? pReq->iWin32Error : pReq->iHttpStatus,
			pReq->iWin32Error != ERROR_SUCCESS ? pReq->pszWin32Error : pReq->pszHttpStatus,
			pReq->iStatus == REQUEST_STATUS_WAITING ? TEXT_STATUS_WAITING : (pReq->iStatus == REQUEST_STATUS_DOWNLOADING ? TEXT_STATUS_DOWNLOADING : TEXT_STATUS_COMPLETED),
			pReq->Speed.szSpeed,
			MyTimeDiff( &pReq->tmDisconnect, &pReq->tmConnect ),
			pReq->szMethod,
			pReq->pszURL,
			pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY)
			);

		// Free request's content
		MyFree( pReq->pszURL );
		MyFree( pReq->pszProxy );
		MyFree( pReq->pszProxyUser );
		MyFree( pReq->pszProxyPass );
		MyFree( pReq->pszHeaders );
		MyFree( pReq->pData );
		MyFree( pReq->pszSrvHeaders );
		MyFree( pReq->pszSrvIP );
		MyFree( pReq->pszWin32Error );
		MyFree( pReq->pszHttpStatus );
		MyFree( pReq->pszReferer );

		switch (pReq->iLocalType)
		{
		case REQUEST_LOCAL_FILE:
			MyFree( pReq->Local.pszFile );
			if (pReq->Local.hFile != NULL && pReq->Local.hFile != INVALID_HANDLE_VALUE)
				CloseHandle( pReq->Local.hFile );
			break;
		case REQUEST_LOCAL_MEMORY:
			MyFree( pReq->Local.pMemory );
			break;
		}

		// Remove from list
		{
			PQUEUE_ITEM pPrevReq;
			for (pPrevReq = pQueue->pHead; (pPrevReq != NULL) && (pPrevReq->pNext != pReq); pPrevReq = pPrevReq->pNext);
			if (pPrevReq) {
				pPrevReq->pNext = pReq->pNext;
			} else {
				pQueue->pHead = pReq->pNext;
			}
		}

		// Destroy the request
		MyFree( pReq );
	}
	else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL QueueAbort( _In_ PQUEUE pQueue, _In_ PQUEUE_ITEM pReq )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pReq) {

		TRACE(
			_T( "  QueueAbort(%s, ID:%u, Prio:%u, St:%s, %s %s -> %s)\n" ),
			pQueue->szName,
			pReq->iId, pReq->iPriority,
			pReq->iStatus == REQUEST_STATUS_WAITING ? TEXT_STATUS_WAITING : (pReq->iStatus == REQUEST_STATUS_DOWNLOADING ? TEXT_STATUS_DOWNLOADING : TEXT_STATUS_COMPLETED),
			pReq->szMethod,
			pReq->pszURL,
			pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY)
			);

		switch (pReq->iStatus) {
		case REQUEST_STATUS_WAITING:
			pReq->bAbort = TRUE;
			pReq->iStatus = REQUEST_STATUS_DONE;
			/// Error code
			pReq->iWin32Error = ERROR_INTERNET_OPERATION_CANCELLED;
			MyFree( pReq->pszWin32Error );
			AllocErrorStr( pReq->iWin32Error, &pReq->pszWin32Error );
			break;
		case REQUEST_STATUS_DOWNLOADING:
			pReq->bAbort = TRUE;
			/// The worker thread will do the rest...
			break;
		case REQUEST_STATUS_DONE:
			/// Nothing to do
			break;
		default: assert( !"Unknown request status" );
		}

	} else {
		bRet = FALSE;
	}
	return bRet;
}


ULONG QueueSize( _Inout_ PQUEUE pQueue )
{
	PQUEUE_ITEM pReq;
	ULONG iSize;
	assert( pQueue );
	for (pReq = pQueue->pHead, iSize = 0; pReq; pReq = pReq->pNext, iSize++);
	TRACE2( _T( "  QueueSize(%s) == %u\n" ), pQueue->szName, iSize );
	return iSize;
}


BOOL QueueStatistics(
	_In_ PQUEUE pQueue,
	_Out_opt_ PULONG piThreadCount,
	_Out_opt_ PULONG piTotalCount,
	_Out_opt_ PULONG piTotalDone,
	_Out_opt_ PULONG piTotalDownloading,
	_Out_opt_ PULONG piTotalWaiting,
	_Out_opt_ PULONG64 piTotalRecvSize,
	_Out_opt_ PULONG piTotalSpeed
	)
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pQueue) {
		PQUEUE_ITEM pReq;

		if (piThreadCount)
			*piThreadCount = (ULONG)pQueue->iThreadCount;

		if (piTotalCount)
			*piTotalCount = 0;
		if (piTotalDone)
			*piTotalDone = 0;
		if (piTotalDownloading)
			*piTotalDownloading = 0;
		if (piTotalWaiting)
			*piTotalWaiting = 0;
		if (piTotalRecvSize)
			*piTotalRecvSize = 0;
		if (piTotalSpeed)
			*piTotalSpeed = 0;

		for (pReq = pQueue->pHead; pReq; pReq = pReq->pNext) {
			if (piTotalCount)
				(*piTotalCount)++;
			if (piTotalDone && (pReq->iStatus == REQUEST_STATUS_DONE))
				(*piTotalDone)++;
			if (piTotalDownloading && (pReq->iStatus == REQUEST_STATUS_DOWNLOADING))
				(*piTotalDownloading)++;
			if (piTotalWaiting && (pReq->iStatus == REQUEST_STATUS_WAITING))
				(*piTotalWaiting)++;
			if (piTotalRecvSize)
				*piTotalRecvSize += pReq->iRecvSize;
			if (piTotalSpeed && (pReq->iStatus == REQUEST_STATUS_DOWNLOADING))
				*piTotalSpeed += pReq->Speed.iSpeed;
		}

	} else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL RequestMemoryToString( _In_ PQUEUE_ITEM pReq, _Out_ LPTSTR pszString, _In_ ULONG iStringLen )
{
	BOOL bRet = FALSE;
	if (pReq && pszString && iStringLen) {
		pszString[0] = 0;
		if (pReq->iLocalType == REQUEST_LOCAL_MEMORY) {
			if (pReq->iStatus == REQUEST_STATUS_DONE) {
				if (pReq->iWin32Error == ERROR_SUCCESS && (pReq->iHttpStatus >= 200 && pReq->iHttpStatus < 300)) {
					if (pReq->iFileSize > 0 && pReq->iFileSize != INVALID_FILE_SIZE64) {
						ULONG i, n;
						CHAR ch;
						for (i = 0, n = (ULONG)__min( iStringLen - 1, pReq->iFileSize ); i < n; i++) {
							ch = pReq->Local.pMemory[i];
							if ((ch >= 32 /*&& ch < 127*/) || ch == '\r' || ch == '\n') {
								pszString[i] = pReq->Local.pMemory[i];
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
