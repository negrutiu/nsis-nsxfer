#pragma once
#include "thread.h"

#define QUEUE_MAX_THREADS			20

/*
	Queue facts:
	- A "queue" is an object that manages a list of download requests (queue items), and a list of worker threads
	- An item waits in the queue (ITEM_STATUS_WAITING) until a worker thread will handle it
	- When a worker thread becomes available, it'll start processing the item (ITEM_STATUS_DOWNLOADING)
	- After the download is completed, the item will be left in the queue (ITEM_STATUS_DONE)
	- Items are stored in the queue forever. Their status can be queried at any time.
	- QueueClear(...) will empty the queue. All past downloads will be forgotten.
*/

#define DEFAULT_VALUE				((ULONG)-1)

typedef enum {
	ITEM_STATUS_WAITING,			/// The item is waiting in queue. Not downloaded yet
	ITEM_STATUS_DOWNLOADING,		/// The item is being downloaded
	ITEM_STATUS_DONE				/// The item has been downloaded (successful or not)
} ITEM_STATUS;

typedef enum {
	ITEM_LOCAL_NONE,				/// The remote content will not be downloaded. Useful to simply connect (HTTP GET) and disconnect instantly
	ITEM_LOCAL_FILE,				/// The remote content will be downloaded to a local file
	ITEM_LOCAL_MEMORY				/// The remote content will be downloaded to a memory
} ITEM_LOCAL_TYPE;


typedef struct _QUEUE_ITEM {

	ULONG iId;						/// Unique download ID
	ITEM_STATUS iStatus;

	// Objects this item belongs to
	struct _QUEUE *pQueue;
	struct _THREAD *pThread;

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
	} Local;

	// Download options
	ULONG iRetryCount;				/// The number of InternetOpenUrl calls. Default is 1
	ULONG iRetryDelay;				/// Delay between two InternetOpenUrl calls. Default is 0
	ULONG iConnectRetries;			/// InternetSetOption( INTERNET_OPTION_CONNECT_RETRIES ). Relevant only for hosts with multiple IPs!
	ULONG iConnectTimeout;			/// InternetSetOption( INTERNET_OPTION_CONNECT_TIMEOUT )
	ULONG iReceiveTimeout;			/// InternetSetOption( INTERNET_OPTION_RECEIVE_TIMEOUT )
	BOOL bResume;					/// Resume download. Works with ITEM_LOCAL_FILE only

	// Runtime statistics
	FILETIME tmEnqueue;				/// Enqueue time
	FILETIME tmDownloadStart;		/// Download startup time
	FILETIME tmDownloadEnd;			/// Download startup time
	ULONG64 iFileSize;				/// File size or -1 if not available
	ULONG64 iRecvSize;				/// Received bytes

	// Runtime variables
	HINTERNET hSession;				/// InternetOpen
	HINTERNET hConnect;				/// InternetOpenUrl

	// Error code (Win32 and HTTP)
	ULONG iWin32Error;				/// Last Win32 error code
	LPTSTR pszWin32Error;			/// Last Win32 error code (as string)
	ULONG iHttpStatus;				/// Last HTTP status code
	LPTSTR pszHttpStatus;			/// Last HTTP status code (as string)

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

} QUEUE_ITEM, *PQUEUE_ITEM;


typedef struct _QUEUE {

	TCHAR szName[20];				/// Queue name. The default queue will be named MAIN

	// Queue
	CRITICAL_SECTION csLock;
	PQUEUE_ITEM pHead;
	ULONG iLastId;

	// Worker threads
	THREAD pThreads[QUEUE_MAX_THREADS];
	int iThreadCount;
	HANDLE hThreadTermEvent;
	HANDLE hThreadWakeEvent;

} QUEUE, *PQUEUE;


// Initializing
VOID QueueInitialize( _Inout_ PQUEUE pQueue, _In_ LPCTSTR szName, _In_ int iThreadCount );
VOID QueueDestroy( _Inout_ PQUEUE pQueue );

// Queue locking
VOID QueueLock( _Inout_ PQUEUE pQueue );
VOID QueueUnlock( _Inout_ PQUEUE pQueue );


// Clear the queue, destroy everything
// The queue must be locked
BOOL QueueReset( _Inout_ PQUEUE pQueue );

// Find an item in the queue
// The queue must be locked
PQUEUE_ITEM QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iItemID );	/// ...by ID
PQUEUE_ITEM QueueFindFirstWaiting( _Inout_ PQUEUE pQueue );			/// ...by status

// Add a new queue item
// The queue must be locked
BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_ LPCTSTR pszURL,
	_In_ ITEM_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_In_opt_ ULONG iRetryCount,					/// can be DEFAULT_VALUE
	_In_opt_ ULONG iRetryDelay,					/// can be DEFAULT_VALUE
	_In_opt_ ULONG iConnectRetries,				/// can be DEFAULT_VALUE
	_In_opt_ ULONG iConnectTimeout,				/// can be DEFAULT_VALUE
	_In_opt_ ULONG iReceiveTimeout,				/// can be DEFAULT_VALUE
	_Outptr_opt_ PQUEUE_ITEM *ppItem
	);

// Remove an item from the queue and destroys the item
// The queue must be locked
BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_ITEM pItem );

// Retrieve the queue size
// The queue must be locked
ULONG QueueSize( _Inout_ PQUEUE pQueue );
