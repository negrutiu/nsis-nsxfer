#include "main.h"
#include "queue.h"
#include "utils.h"


// The global download queue
QUEUE g_Queue = { 0 };


VOID QueueInitialize( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	TRACE( _T( "  QueueInitialize()\n" ) );
	InitializeCriticalSectionAndSpinCount( &pQueue->csLock, 3000 );
}

VOID QueueDestroy( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	QueueLock( pQueue );
	QueueReset( pQueue );
	DeleteCriticalSection( &pQueue->csLock );
	TRACE( _T( "  QueueDestroy()\n" ) );
}

VOID QueueLock( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	if ( !TryEnterCriticalSection( &pQueue->csLock ) )
		EnterCriticalSection( &pQueue->csLock );
	TRACE( _T( "  QueueLock()\n" ) );
}

VOID QueueUnlock( _Inout_ PQUEUE pQueue )
{
	assert( pQueue );
	LeaveCriticalSection( &pQueue->csLock );
	TRACE( _T( "  QueueUnlock()\n" ) );
}

BOOL QueueReset( _Inout_ PQUEUE pQueue )
{
	BOOL bRet = TRUE;
	assert( pQueue );
	TRACE( _T( "  QueueReset()\n" ) );
	while ( pQueue->pHead )
		bRet = bRet && QueueRemove( pQueue, pQueue->pHead );
	return bRet;
}

PQUEUE_ITEM QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iItemID )
{
	PQUEUE_ITEM pItem;
	assert( pQueue );
	for ( pItem = pQueue->pHead; pItem && pItem->iId != iItemID; pItem = pItem->pNext );
	TRACE( _T( "  QueueFind(ID:%u) == 0x%p\n" ), iItemID, pItem );
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
	TRACE( _T( "  QueueFindFirstWaiting() == ID:%u, 0x%p\n" ), pLastWaitingItem ? pLastWaitingItem->iId : 0, pLastWaitingItem );
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

			{
				SYSTEMTIME st;
				GetLocalTime( &st );
				SystemTimeToFileTime( &st, &pItem->tmEnqueue );
			}
			pItem->tmDownloadStart.dwLowDateTime = 0;
			pItem->tmDownloadStart.dwHighDateTime = 0;
			pItem->tmDownloadEnd.dwLowDateTime = 0;
			pItem->tmDownloadEnd.dwHighDateTime = 0;

			pItem->bErrorCodeIsHTTP = FALSE;
			pItem->iErrorCode = ERROR_SUCCESS;

			// Add in front
			pItem->pNext = pQueue->pHead;
			pQueue->pHead = pItem;

			// Return
			if ( ppItem )
				*ppItem = pItem;

			TRACE(
				_T( "  QueueAdd(ID:%u, %s -> %s)\n" ),
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
			_T( "  QueueRemove(ID:%u, Err:%u, St:%s, %s -> %s)\n" ),
			pItem->iId,
			pItem->iErrorCode,
			pItem->iStatus == ITEM_STATUS_WAITING ? _T("Waiting") : (pItem->iStatus == ITEM_STATUS_DOWNLOADING ? _T("Downloading") : _T("Done")),
			pItem->pszURL,
			pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->LocalData.pszFile : _T( "Memory" ))
			);

		// Free item's content
		MyFree( pItem->pszURL );

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
			}
			else {
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
	TRACE( _T( "  QueueSize() == %u\n" ), iSize );
	return iSize;
}
