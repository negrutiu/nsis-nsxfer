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
	- QueueClear(...) will empty the queue. All completed downloads will be forgotten.
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

	// Transfer options
	TCHAR szMethod[20];				/// GET, POST, HEAD, etc. Default is GET
	LPTSTR pszHeaders;				/// Additional HTTP headers sent by HttpSendRequest
	LPVOID pData;					/// Additional data sent by HttpSendRequest (useful for POST)
	ULONG iDataSize;
	ULONG iTimeoutConnect;			/// Keep trying to connect for X ms. Default is 0
	ULONG iTimeoutReconnect;		/// Keep trying to reconnect for X ms when the connection drops while downloading. Default is 0
	ULONG iOptConnectRetries;		/// InternetSetOption( INTERNET_OPTION_CONNECT_RETRIES ). Relevant only for hosts with multiple IPs!
	ULONG iOptConnectTimeout;		/// InternetSetOption( INTERNET_OPTION_CONNECT_TIMEOUT )
	ULONG iOptReceiveTimeout;		/// InternetSetOption( INTERNET_OPTION_RECEIVE_TIMEOUT )
	LPTSTR pszReferer;				/// Sent to HttpOpenRequest. Default is NULL
	ULONG iHttpInternetFlags;		/// INTERNET_FLAG_XXX. Default is INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_IGNORE_CERT_DATE_INVALID|INTERNET_FLAG_NO_COOKIES|INTERNET_FLAG_NO_UI|INTERNET_FLAG_RELOAD
	ULONG iHttpSecurityFlags;		/// SECURITY_FLAG_XXX. Default is SECURITY_FLAG_IGNORE_REVOCATION|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID

	// Runtime statistics
	FILETIME tmEnqueue;				/// Enqueuing timestamp
	FILETIME tmConnect;				/// Connecting timestamp
	FILETIME tmDisconnect;			/// Disconnecting timestamp

	ULONG64 iFileSize;				/// File size or -1 if not available
	ULONG64 iRecvSize;				/// Received bytes

	struct {
		FILETIME tmStart;			/// Last transfer (loop) startup time
		FILETIME tmEnd;				/// Last transfer (loop) finish time
		ULONG64 iXferSize;			/// Last transfer (loop) data size
	} Xfer;

	struct {
		ULONG iSpeed;				/// Transfer speed (bytes/sec)
		TCHAR szSpeed[30];			/// Transfer speed nicely formatted ("255.4 KB/s", "2 MB/s", etc)
		ULONG iChunkTime;			/// Tick count
		ULONG iChunkSize;			/// Bytes
	} Speed;

	// Runtime variables
	HINTERNET hSession;				/// InternetOpen
	HINTERNET hConnect;				/// InternetConnect
	HINTERNET hRequest;				/// HttpOpenRequest
	BOOLEAN bResumeNeedsValidation;	/// Set to TRUE when The Range HTTP header is used, and we must validate whether the server truly supports it

	// Error code (Win32 and HTTP)
	ULONG iWin32Error;				/// Last Win32 error code
	LPTSTR pszWin32Error;			/// Last Win32 error code (as string)
	ULONG iHttpStatus;				/// Last HTTP status code
	LPTSTR pszHttpStatus;			/// Last HTTP status code (as string)

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

} QUEUE_ITEM, *PQUEUE_ITEM;


#define ItemIsReconnectAllowed(pItem) \
	((pItem)->iTimeoutReconnect != DEFAULT_VALUE) && \
	((pItem)->iTimeoutReconnect > 0)

#define ItemGetRecvPercent(pItem) \
	(int)(((pItem)->iFileSize == 0 || (pItem)->iFileSize == INVALID_FILE_SIZE64) ? 0 : (((pItem)->iRecvSize * 100) / (pItem)->iFileSize))


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
	_In_opt_ LPCTSTR pszMethod,					/// can be NULL
	_In_opt_ LPCTSTR pszHeaders,				/// can be NULL
	_In_opt_ LPVOID pData,						/// can be NULL
	_In_opt_ ULONG iDataSize,					/// can be 0
	_In_opt_ ULONG iTimeoutConnect,				/// can be DEFAULT_VALUE
	_In_opt_ ULONG iTimeoutReconnect,			/// can be DEFAULT_VALUE
	_In_opt_ ULONG iOptConnectRetries,			/// can be DEFAULT_VALUE
	_In_opt_ ULONG iOptConnectTimeout,			/// can be DEFAULT_VALUE
	_In_opt_ ULONG iOptReceiveTimeout,			/// can be DEFAULT_VALUE
	_In_opt_ LPCTSTR pszReferer,				/// can be NULL
	_In_opt_ ULONG iHttpInternetFlags,			/// can be DEFAULT_VALUE
	_In_opt_ ULONG iHttpSecurityFlags,			/// can be DEFAULT_VALUE
	_Outptr_opt_ PQUEUE_ITEM *ppItem
	);

// Remove an item from the queue and destroys the item
// The queue must be locked
BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_ITEM pItem );

// Retrieve the queue size
// The queue must be locked
ULONG QueueSize( _Inout_ PQUEUE pQueue );

// Retrieve queue statistics
// The queue must be locked
BOOL QueueStatistics(
	_In_ PQUEUE pQueue,
	_Out_opt_ PULONG piThreadCount,
	_Out_opt_ PULONG piItemsTotal,
	_Out_opt_ PULONG piItemsDone,
	_Out_opt_ PULONG piItemsDownloading,
	_Out_opt_ PULONG piItemsWaiting,
	_Out_opt_ PULONG piItemsSpeed				/// Combined transfer speed in bytes/s
	);