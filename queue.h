#pragma once

/*
	Download queue:
	- Each queue item represents a download request
	- An item waits in the queue (ITEM_STATUS_WAITING) until a worker thread will handle it
	- When a worker thread becomes available, it'll process the item (ITEM_STATUS_DOWNLOADING)
	- After the download is completed, the item will be left in the queue (ITEM_STATUS_DONE)
	- Items are stored in the queue forever. Their status can be queried at any time.
	- QueueClear(...) will empty the queue. All past downloads will be forgotten.
*/

typedef enum {
	ITEM_STATUS_WAITING,			/// The item is waiting in queue. Not downloaded yet
	ITEM_STATUS_DOWNLOADING,		/// The item is being downloaded
	ITEM_STATUS_DONE				/// The item has been downloaded (successful or not)
} ITEM_STATUS;

typedef struct _QUEUE_ITEM {

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

	ULONG iId;						/// Unique download ID
	ITEM_STATUS iStatus;

	TCHAR szURL[1024];
	TCHAR szFile[1024];
	ULONG iRetryCount;				/// 0 if not used
	ULONG iRetryDelay;				/// 0 if not used

	BOOL bErrorCodeHTTP;			/// TRUE == HTTP status code, FALSE == Win32 error code
	ULONG iErrorCode;

	struct {
		FILETIME tmEnqueue;			/// Enqueue time
		FILETIME tmDownloadStart;	/// Download startup time
		FILETIME tmDownloadEnd;		/// Download startup time
		ULONG64 iFileSize;			/// File size or -1 if not available
		ULONG64 iRecvSize;			/// Received bytes
	} Stats;

} QUEUE_ITEM;


// Queue locking
VOID QueueLock();
VOID QueueUnlock();


// Clears the queue, destroys everything.
// The queue must be locked.
BOOL QueueReset();

// Finds a queue item by its ID.
// The queue must be locked.
QUEUE_ITEM* QueueFind(
	_In_ ULONG iItemID
	);

// Adds a new queue item.
// The queue must be locked.
BOOL QueueAdd(
	_In_ LPCTSTR pszURL,
	_In_opt_ LPCTSTR pszFile,
	_Outptr_opt_ QUEUE_ITEM *pItem
	);

// Removes an item from the queue.
// The queue must be locked.
BOOL QueueRemove(
	_In_ QUEUE_ITEM *pItem
	);

// Retrieve the number of queue items.
// The queue must be locked.
ULONG QueueLength();
