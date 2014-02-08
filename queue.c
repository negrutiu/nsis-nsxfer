#include "main.h"
#include "queue.h"
#include "utils.h"

PQUEUE_ITEM g_pQueue = NULL;
CRITICAL_SECTION g_csQueue = { 0 };
ULONG g_iLastItemId = 0;

VOID QueueInitialize()
{
	TRACE( _T( "  QueueInitialize()\n" ) );
	InitializeCriticalSectionAndSpinCount( &g_csQueue, 3000 );
}

VOID QueueDestroy()
{
	QueueLock();
	QueueReset();
	DeleteCriticalSection( &g_csQueue );
	TRACE( _T( "  QueueDestroy()\n" ) );
}

VOID QueueLock()
{
	if ( !TryEnterCriticalSection( &g_csQueue ) )
		EnterCriticalSection( &g_csQueue );
	TRACE( _T( "  QueueLock()\n" ) );
}

VOID QueueUnlock()
{
	LeaveCriticalSection( &g_csQueue );
	TRACE( _T( "  QueueUnlock()\n" ) );
}

BOOL QueueReset()
{
	BOOL bRet = TRUE;
	TRACE( _T( "  QueueReset()\n" ) );
	while ( g_pQueue )
		bRet = bRet && QueueRemove( g_pQueue );
	return bRet;
}

PQUEUE_ITEM QueueFind( _In_ ULONG iItemID )
{
	PQUEUE_ITEM pItem;
	for ( pItem = g_pQueue; pItem && pItem->iId != iItemID; pItem = pItem->pNext );
	TRACE( _T( "  QueueFind(ID:%u) == 0x%p\n" ), iItemID, pItem );
	return pItem;
}

PQUEUE_ITEM QueueFindFirstWaiting()
{
	// New items are always added in the front of our queue
	// Therefore, the first waiting item (chronologically) is the last waiting item found in the queue
	PQUEUE_ITEM pItem, pLastWaitingItem;
	for ( pItem = g_pQueue, pLastWaitingItem = NULL; pItem; pItem = pItem->pNext )
		if ( pItem->iStatus == ITEM_STATUS_WAITING )
			pLastWaitingItem = pItem;
	TRACE( _T( "  QueueFindFirstWaiting() == ID:%u, 0x%p\n" ), pLastWaitingItem ? pLastWaitingItem->iId : 0, pLastWaitingItem );
	return pLastWaitingItem;
}

BOOL QueueAdd(
	_In_ LPCTSTR pszURL,
	_In_ ITEM_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_Outptr_opt_ PQUEUE_ITEM *ppItem
	)
{
	BOOL bRet = TRUE;
	if ( pszURL && *pszURL && ((iLocalType != ITEM_LOCAL_FILE) || (pszLocalFile && *pszLocalFile))) {

		PQUEUE_ITEM pItem = (PQUEUE_ITEM)MyAlloc( sizeof(QUEUE_ITEM) );
		if ( pItem ) {

			pItem->iId = ++g_iLastItemId;
			pItem->iStatus = ITEM_STATUS_WAITING;
			MyStrDup( pItem->pszURL, pszURL );

			pItem->iLocalType = iLocalType;
			switch ( iLocalType ) {
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
				TRACE( _T( "ERROR: Unknown queue item type %d\n" ), (int)iLocalType );
			}

			pItem->iRetryCount = 2;
			pItem->iRetryDelay = 1000;

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
			pItem->pNext = g_pQueue;
			g_pQueue = pItem;

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


BOOL QueueRemove( _In_ PQUEUE_ITEM pItem )
{
	BOOL bRet = TRUE;
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

		switch ( pItem->iLocalType ) {
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
			for ( pPrevItem = g_pQueue; (pPrevItem != NULL) && (pPrevItem->pNext != pItem); pPrevItem = pPrevItem->pNext );
			if ( pPrevItem ) {
				pPrevItem->pNext = pItem->pNext;
			}
			else {
				g_pQueue = pItem->pNext;
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

ULONG QueueSize()
{
	PQUEUE_ITEM pItem;
	ULONG iSize;
	for ( pItem = g_pQueue, iSize = 0; pItem; pItem = pItem->pNext, iSize++ );
	TRACE( _T( "  QueueSize() == %u\n" ), iSize );
	return iSize;
}
