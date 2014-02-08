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

typedef enum {
	ITEM_LOCAL_NONE,				/// The remote content will not be downloaded. Useful for simply connecting (HTTP GET) and then quickly exit
	ITEM_LOCAL_FILE,				/// The remote content will be downloaded to a local file
	ITEM_LOCAL_MEMORY				/// The remote content will be downloaded to a memory
} ITEM_LOCAL_TYPE;


typedef struct _QUEUE_ITEM {

	ULONG iId;						/// Unique download ID
	ITEM_STATUS iStatus;

	// Source
	LPTSTR pszURL;

	// Destination
	ITEM_LOCAL_TYPE iLocalType;
	union {
		struct {
			LPTSTR pszFile;			/// Valid for ITEM_LOCAL_FILE
			HANDLE hFile;
		};
		LPBYTE pMemory;				/// Valid for ITEM_LOCAL_MEMORY. The buffer size will be iFileSize
	} LocalData;

	// Download options
	ULONG iRetryCount;				/// (optional)
	ULONG iRetryDelay;				/// (optional)

	// Runtime statistics
	FILETIME tmEnqueue;				/// Enqueue time
	FILETIME tmDownloadStart;		/// Download startup time
	FILETIME tmDownloadEnd;			/// Download startup time
	ULONG64 iFileSize;				/// File size or -1 if not available
	ULONG64 iRecvSize;				/// Received bytes

	// Error code (Win32 or HTTP)
	BOOL bErrorCodeIsHTTP;			/// TRUE: error code is HTTP status, FALSE: error code is Win32 error
	ULONG iErrorCode;

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

} QUEUE_ITEM, *PQUEUE_ITEM;


// Initializing
VOID QueueInitialize();
VOID QueueDestroy();

// Queue locking
VOID QueueLock();
VOID QueueUnlock();


// Clear the queue, destroy everything
// The queue must be locked
BOOL QueueReset();

// Find an item in the queue
// The queue must be locked
PQUEUE_ITEM QueueFind( _In_ ULONG iItemID );	/// ...by ID
PQUEUE_ITEM QueueFindFirstWaiting();			/// ...by status

// Add a new queue item
// The queue must be locked
BOOL QueueAdd(
	_In_ LPCTSTR pszURL,
	_In_ ITEM_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_Outptr_opt_ PQUEUE_ITEM *ppItem
	);

// Remove an item from the queue and destroys the item
// The queue must be locked
BOOL QueueRemove( _In_ PQUEUE_ITEM pItem );

// Retrieve the queue size
// The queue must be locked
ULONG QueueSize();
