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

PQUEUE_REQUEST QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iReqID )
{
	PQUEUE_REQUEST pReq;
	assert( pQueue );
	for (pReq = pQueue->pHead; pReq && pReq->iId != iReqID; pReq = pReq->pNext);
	TRACE2( _T( "  QueueFind(%s, ID:%u) == 0x%p\n" ), pQueue->szName, iReqID, pReq );
	return pReq;
}

PQUEUE_REQUEST QueueFindNextWaiting( _Inout_ PQUEUE pQueue )
{
	// NOTES:
	// New requests are always added to the front of the queue. The first (chronologically) waiting request is the last one
	// We'll select the request with the highest priority (Note: lower value means higher priority)

	PQUEUE_REQUEST pReq, pSelectedReq = NULL;
	ULONG iSelectedPrio = ULONG_MAX - 1;
	assert( pQueue );
	for (pReq = pQueue->pHead; pReq; pReq = pReq->pNext) {
		if (pReq->iStatus == REQUEST_STATUS_WAITING && !pReq->bAbort && pReq->iPriority <= iSelectedPrio) {
			if (pReq->iDependId > 0) {
				PQUEUE_REQUEST pReqDepend = QueueFind( pQueue, pReq->iDependId );
				if (!pReqDepend || pReqDepend->iStatus != REQUEST_STATUS_DONE)
					continue;	/// This request depends on another that's not finished
			}
			pSelectedReq = pReq;
			iSelectedPrio = pReq->iPriority;
		}
	}
	TRACE2( _T( "  QueueFindNextWaiting(%s) == ID:%u, Prio:%u, Ptr:0x%p\n" ), pQueue->szName, pSelectedReq ? pSelectedReq->iId : 0, pSelectedReq ? pSelectedReq->iPriority : 0, pSelectedReq );
	return pSelectedReq;
}

BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_ PQUEUE_REQUEST_PARAM pParam,
	_Outptr_opt_ PQUEUE_REQUEST *ppReq
	)
{
	BOOL bRet = TRUE;
	assert( pQueue );
	assert( pParam );
	if (pParam->pszURL && *pParam->pszURL && ((pParam->iLocalType != REQUEST_LOCAL_FILE) || (pParam->pszLocalFile && *pParam->pszLocalFile))) {

		PQUEUE_REQUEST pReq = (PQUEUE_REQUEST)MyAlloc( sizeof( QUEUE_REQUEST ) );
		if (pReq) {

			MyZeroMemory( pReq, sizeof( *pReq ) );
			pReq->iId = ++pQueue->iLastId;
			pReq->iPriority = (pParam->iPriority == DEFAULT_VALUE) ? DEFAULT_PRIORITY : pParam->iPriority;
			pReq->iDependId = pParam->iDependId;
			pReq->iStatus = REQUEST_STATUS_WAITING;
			pReq->pQueue = pQueue;

			MyStrDup( pReq->pszURL, pParam->pszURL );

			pReq->iLocalType = pParam->iLocalType;
			if (pReq->iLocalType == REQUEST_LOCAL_FILE)
				MyStrDup( pReq->Local.pszFile, pParam->pszLocalFile );

			if (pParam->pszProxy && *pParam->pszProxy) {
				MyStrDup( pReq->pszProxy, pParam->pszProxy );
			}
			if (pParam->pszProxyUser && *pParam->pszProxyUser) {
				MyStrDup( pReq->pszProxyUser, pParam->pszProxyUser );
			}
			if (pParam->pszProxyPass && *pParam->pszProxyPass) {
				MyStrDup( pReq->pszProxyPass, pParam->pszProxyPass );
			}

			if (pParam->pszMethod && *pParam->pszMethod) {
				lstrcpyn( pReq->szMethod, pParam->pszMethod, ARRAYSIZE( pReq->szMethod ) );
			} else {
				lstrcpy( pReq->szMethod, _T( "GET" ) );		/// Default
			}
			if (pParam->pszHeaders && *pParam->pszHeaders) {
				MyStrDup( pReq->pszHeaders, pParam->pszHeaders );
			}
			if (pParam->pData && (pParam->iDataSize > 0)) {
				MyDataDup( pReq->pData, pParam->pData, pParam->iDataSize );
				pReq->iDataSize = pParam->iDataSize;
			}
			pReq->iTimeoutConnect = pParam->iTimeoutConnect;
			pReq->iTimeoutReconnect = pParam->iTimeoutReconnect;
			pReq->iOptConnectRetries = pParam->iOptConnectRetries;
			pReq->iOptConnectTimeout = pParam->iOptConnectTimeout;
			pReq->iOptReceiveTimeout = pParam->iOptReceiveTimeout;
			pReq->iOptSendTimeout = pParam->iOptSendTimeout;
			if (pParam->pszReferrer && *pParam->pszReferrer) {
				MyStrDup( pReq->pszReferer, pParam->pszReferrer );
			}
			pReq->iHttpInternetFlags = pParam->iHttpInternetFlags;
			pReq->iHttpSecurityFlags = pParam->iHttpSecurityFlags;

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
				_T( "  QueueAdd(%s, ID:%u, %s %s -> %s, Prio:%u, DependsOn:%d)\n" ),
				pQueue->szName,
				pReq->iId,
				pReq->szMethod,
				pReq->pszURL,
				pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY),
				pReq->iPriority, pReq->iDependId
				);

		} else {
			bRet = FALSE;
		}
	} else {
		bRet = FALSE;
	}
	return bRet;
}


BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_REQUEST pReq )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	if (pReq) {

		TRACE(
			_T( "  QueueRemove(%s, ID:%u, Err:%u \"%s\", St:%s, Speed:%s, Time:%ums, Drops:%u, %s %s -> %s)\n" ),
			pQueue->szName,
			pReq->iId,
			pReq->iWin32Error != ERROR_SUCCESS ? pReq->iWin32Error : pReq->iHttpStatus,
			pReq->iWin32Error != ERROR_SUCCESS ? pReq->pszWin32Error : pReq->pszHttpStatus,
			pReq->iStatus == REQUEST_STATUS_WAITING ? TEXT_STATUS_WAITING : (pReq->iStatus == REQUEST_STATUS_DOWNLOADING ? TEXT_STATUS_DOWNLOADING : TEXT_STATUS_COMPLETED),
			pReq->Speed.szSpeed,
			MyTimeDiff( &pReq->tmDisconnect, &pReq->tmConnect ),
			pReq->iConnectionDrops,
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
			PQUEUE_REQUEST pPrevReq;
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


BOOL QueueAbort( _In_ PQUEUE pQueue, _In_ PQUEUE_REQUEST pReq, _In_opt_ DWORD dwWaitMS )
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
			if (dwWaitMS) {
				/// Wait until it'll finish the job
				const DWORD dwAbortWait = 100;
				DWORD dwAbortElapsed = 0;
				while ((pReq->iStatus == REQUEST_STATUS_DOWNLOADING) && (dwAbortElapsed < dwWaitMS)) {
					Sleep( dwAbortWait );
					dwAbortElapsed += dwAbortWait;
				}
			}
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
	PQUEUE_REQUEST pReq;
	ULONG iSize;
	assert( pQueue );
	for (pReq = pQueue->pHead, iSize = 0; pReq; pReq = pReq->pNext, iSize++);
	TRACE2( _T( "  QueueSize(%s) == %u\n" ), pQueue->szName, iSize );
	return iSize;
}


ULONG RequestOptimalBufferSize( _In_ PQUEUE_REQUEST pReq )
{
	assert( pReq );
	ULONG iSize;

	if (pReq->Speed.iSpeed > 0) {
		/// Use already computed speed (bps)
		iSize = pReq->Speed.iSpeed;
	} else {
		/// Estimate speed based on what's been received so far
		ULONG iMS = GetTickCount() - pReq->Speed.iChunkTime;
		if (iMS > 10) {
			iSize = (pReq->Speed.iChunkSize / iMS) * 1000;
		} else {
			iSize = MIN_BUFFER_SIZE;
		}
	}

	iSize -= iSize % 1024;				/// Align to kilobyte
	iSize *= 2;							/// Allow the speed to grow. Accomodate more seconds' data...
	iSize = __max( iSize, MIN_BUFFER_SIZE );
	iSize = __min( iSize, MAX_BUFFER_SIZE );

/*	TRACE(
		_T( "[BufSize = %.4u KB] Speed = %u bps, ChunkTime = %u (%u ms), ChunkSize = %u\n" ),
		iSize / 1024,
		pReq->Speed.iSpeed,
		pReq->Speed.iChunkTime, GetTickCount() - pReq->Speed.iChunkTime,
		pReq->Speed.iChunkSize
		);*/
	return iSize;
}


BOOL RequestMemoryToString( _In_ PQUEUE_REQUEST pReq, _Out_ LPTSTR pszString, _In_ ULONG iStringLen )
{
	BOOL bRet = FALSE;
	if (pReq && pszString && iStringLen) {
		pszString[0] = _T( '\0' );
		if (pReq->iLocalType == REQUEST_LOCAL_MEMORY) {
			if (pReq->iStatus == REQUEST_STATUS_DONE) {
				if (pReq->iWin32Error == ERROR_SUCCESS && (pReq->iHttpStatus >= 200 && pReq->iHttpStatus < 300)) {
					if (pReq->iFileSize > 0 && pReq->iFileSize != INVALID_FILE_SIZE64) {
						BinaryToString( pReq->Local.pMemory, (ULONG)pReq->iFileSize, pszString, iStringLen );
						bRet = TRUE;
					}
				}
			}
		}
	}
	return bRet;
}


BOOL RequestDataToString( _In_ PQUEUE_REQUEST pReq, _Out_ LPTSTR pszString, _In_ ULONG iStringLen )
{
	BOOL bRet = FALSE;
	if (pReq && pszString && iStringLen) {
		BinaryToString( pReq->pData, pReq->iDataSize, pszString, iStringLen );
		bRet = TRUE;
	}
	return bRet;
}


int QueueWakeThreads( _In_ PQUEUE pQueue, _In_ int iThreadsToWake )
{
	int iCount = 0;

	assert( pQueue );
	if (iThreadsToWake > 0) {

		int i, n;

		/// Number of threads
		n = __min( iThreadsToWake, pQueue->iThreadCount );

		/// Determine whether we're already running in a worker thread
		for (i = 0; i < pQueue->iThreadCount; i++) {
			if (pQueue->pThreads[i].iTID == GetCurrentThreadId()) {
				n--;	/// One less thread to wake up
				break;
			}
		}

		/// Wake
		for (i = 0; i < n; i++)
			SetEvent( pQueue->hThreadWakeEvent );
		iCount += n;
	}

	return iCount;
}
