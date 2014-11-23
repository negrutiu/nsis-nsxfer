#pragma once
#include "thread.h"

#define MIN_WORKER_THREADS			2
#define MAX_WORKER_THREADS			20
#define INVALID_FILE_SIZE64			(ULONG64)-1
#define DEFAULT_VALUE				((ULONG)-1)
#define DEFAULT_PRIORITY			1000
#define ANY_REQUEST_ID				0
#define ANY_PRIORITY				0
#define ANY_STATUS					(REQUEST_STATUS)-1

#define TEXT_STATUS_WAITING			_T( "Waiting" )
#define TEXT_STATUS_DOWNLOADING		_T( "Downloading" )
#define TEXT_STATUS_COMPLETED		_T( "Completed" )
#define TEXT_LOCAL_NONE				_T( "None" )
#define TEXT_LOCAL_FILE				_T( "File" )
#define TEXT_LOCAL_MEMORY			_T( "Memory" )

/*
	About queues:
	- A queue manages the list of transfer requests, and a pool of worker threads
	- Each transfer request waits in queue (REQUEST_STATUS_WAITING) until a worker thread becomes available (REQUEST_STATUS_DOWNLOADING)
	- After completion, the request will remain in queue (REQUEST_STATUS_DONE) until explicitly removed
	- QueueClear(...) will empty the queue. All completed transfers will be forgotten.
*/

typedef enum {
	REQUEST_STATUS_WAITING,			/// The request is waiting in queue. Not downloaded yet
	REQUEST_STATUS_DOWNLOADING,		/// The request is being downloaded
	REQUEST_STATUS_DONE				/// The request has been downloaded (successful or not)
} REQUEST_STATUS;

typedef enum {
	REQUEST_LOCAL_NONE,				/// The remote content will not be downloaded. Useful to simply connect (HTTP GET) and disconnect instantly
	REQUEST_LOCAL_FILE,				/// The remote content will be downloaded to a local file
	REQUEST_LOCAL_MEMORY			/// The remote content will be downloaded to a memory
} REQUEST_LOCAL_TYPE;


typedef struct _QUEUE_ITEM {

	ULONG iId;						/// Unique request ID
	REQUEST_STATUS iStatus;
	ULONG iPriority;				/// 0 (high prio) -> ULONG_MAX-1 (low prio)

	// Related objects
	struct _QUEUE *pQueue;
	struct _THREAD *pThread;

	// Source
	LPTSTR pszURL;

	LPTSTR pszProxy;				/// CERN type proxies (ex: "HTTP=http://my_http_proxy:port HTTPS=https://my_https_proxy:port")
	LPTSTR pszProxyUser;
	LPTSTR pszProxyPass;

	// Destination
	REQUEST_LOCAL_TYPE iLocalType;
	union {
		struct {
			LPTSTR pszFile;			/// Valid for REQUEST_LOCAL_FILE
			HANDLE hFile;
		};
		LPBYTE pMemory;				/// Valid for REQUEST_LOCAL_MEMORY. The buffer size will be iFileSize
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
	FILETIME tmEnqueue;				/// Enque timestamp
	FILETIME tmConnect;				/// Connect timestamp
	FILETIME tmDisconnect;			/// Disconnect timestamp

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
	BOOLEAN bAbort;					/// Aborted by user
	HINTERNET hSession;				/// InternetOpen
	HINTERNET hConnect;				/// InternetConnect
	HINTERNET hRequest;				/// HttpOpenRequest
	BOOLEAN bRangeSent;				/// The Range HTTP header has been sent
	ULONG iLastCallbackStatus;		/// Last status received in InternetStatusCallback
	LPCTSTR pszSrvIP;				/// Servers resolved IP address (as string)
	LPCTSTR pszSrvHeaders;			/// HTTP headers received from server
	BOOLEAN bConnected;				/// Received INTERNET_STATUS_CONNECTED_TO_SERVER

	// Error code (Win32 and HTTP)
	ULONG iWin32Error;				/// Last Win32 error code
	LPTSTR pszWin32Error;			/// Last Win32 error code (as string)
	ULONG iHttpStatus;				/// Last HTTP status code
	LPTSTR pszHttpStatus;			/// Last HTTP status code (as string)

	struct _QUEUE_ITEM *pNext;		/// Singly linked list

} QUEUE_ITEM, *PQUEUE_ITEM;


#define RequestReconnectionAllowed(pReq) \
	((pReq)->iTimeoutReconnect != DEFAULT_VALUE) && \
	((pReq)->iTimeoutReconnect > 0)

#define RequestRecvPercent(pReq) \
	(int)(((pReq)->iFileSize == 0 || (pReq)->iFileSize == INVALID_FILE_SIZE64) ? 0 : MyMulDiv64((pReq)->iRecvSize, 100, (pReq)->iFileSize))

#define RequestMatched(pReq, ID, Prio, Status) \
	(ID == ANY_REQUEST_ID || ID == pReq->iId) && \
	(Prio == ANY_PRIORITY || Prio == pReq->iPriority) && \
	(Status == ANY_STATUS || Status == pReq->iStatus)

BOOL RequestMemoryToString(
	_In_ PQUEUE_ITEM pReq,
	_Out_ LPTSTR pszString,
	_In_ ULONG iStringLen
	);


typedef struct _QUEUE {

	TCHAR szName[20];				/// Queue name. The default queue will be named MAIN

	// Queue
	CRITICAL_SECTION csLock;
	PQUEUE_ITEM pHead;
	ULONG iLastId;

	// Worker threads
	THREAD pThreads[MAX_WORKER_THREADS];
	int iThreadCount;
	HANDLE hThreadTermEvent;
	HANDLE hThreadWakeEvent;

} QUEUE, *PQUEUE;


// Initializing
VOID QueueInitialize( _Inout_ PQUEUE pQueue, _In_ LPCTSTR szName, _In_ int iThreadCount );
VOID QueueDestroy( _Inout_ PQUEUE pQueue, _In_ BOOLEAN bFromDllUnload );

// Queue locking
VOID QueueLock( _Inout_ PQUEUE pQueue );
VOID QueueUnlock( _Inout_ PQUEUE pQueue );


// Clear the queue, destroy everything
// The queue must be locked
BOOL QueueReset( _Inout_ PQUEUE pQueue );

// Find a request in the queue
// The queue must be locked
PQUEUE_ITEM QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iReqID );	/// ...by ID
PQUEUE_ITEM QueueFindFirstWaiting( _Inout_ PQUEUE pQueue );			/// ...by status

// Add a new request in the queue
// The queue must be locked
BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_opt_ ULONG iPriority,					/// can be DEFAULT_VALUE
	_In_ LPCTSTR pszURL,
	_In_ REQUEST_LOCAL_TYPE iLocalType,
	_In_opt_ LPCTSTR pszLocalFile,
	_In_opt_ LPCTSTR pszProxy,					/// can be NULL
	_In_opt_ LPCTSTR pszProxyUser,				/// can be NULL
	_In_opt_ LPCTSTR pszProxyPass,				/// can be NULL
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
	_Outptr_opt_ PQUEUE_ITEM *ppReq
	);

// Remove a request from the queue and destroy it
// The queue must be locked
BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_ITEM pReq );

// Abort a request
// The queue must be locked
BOOL QueueAbort( _In_ PQUEUE pQueue, _In_ PQUEUE_ITEM pReq );

// Retrieve the queue size
// The queue must be locked
ULONG QueueSize( _Inout_ PQUEUE pQueue );

// Retrieve queue statistics
// The queue must be locked
BOOL QueueStatistics(
	_In_ PQUEUE pQueue,
	_Out_opt_ PULONG piThreadCount,
	_Out_opt_ PULONG piTotalCount,
	_Out_opt_ PULONG piTotalDone,
	_Out_opt_ PULONG piTotalDownloading,
	_Out_opt_ PULONG piTotalWaiting,
	_Out_opt_ PULONG64 piTotalRecvSize,
	_Out_opt_ PULONG piTotalSpeed				/// Combined transfer speed in bytes/s
	);