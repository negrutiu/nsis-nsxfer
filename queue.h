
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2014/02/02

#pragma once
#include "thread.h"

#define MIN_WORKER_THREADS			2
#define MAX_WORKER_THREADS			20
#define INVALID_FILE_SIZE64			(ULONG64)-1
#define DEFAULT_VALUE				((ULONG)-1)
#define DEFAULT_PRIORITY			1000
#define MIN_BUFFER_SIZE				(2 * 1024)			// 2KB
#define MAX_BUFFER_SIZE				(1024 * 1024 * 2)	// 2MB
#define MAX_MEMORY_CONTENT_LENGTH	(5 * 1024 * 1024)	// When requested by the NSIS script the remote content will be truncated to max NSIS string size (usually 4K, sometimes 8K...)
#define ANY_REQUEST_ID				0
#define ANY_PRIORITY				0
#define ANY_STATUS					(REQUEST_STATUS)-1

#define TEXT_USERAGENT				_T( "xfer/1.0" )
#define TEXT_STATUS_WAITING			_T( "Waiting" )
#define TEXT_STATUS_DOWNLOADING		_T( "Downloading" )
#define TEXT_STATUS_COMPLETED		_T( "Completed" )
#define TEXT_LOCAL_NONE				_T( "None" )
#define TEXT_LOCAL_FILE				_T( "File" )
#define TEXT_LOCAL_MEMORY			_T( "Memory" )
#define TEXT_PER_SECOND				_T( "/s" )

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


//++ struct QUEUE_REQUEST_PARAM
typedef struct _QUEUE_REQUEST_PARAM {
	ULONG iPriority;				/// can be DEFAULT_VALUE
	ULONG iDependId;				/// can be DEFAULT_VALUE
	LPCTSTR pszURL;
	REQUEST_LOCAL_TYPE iLocalType;
	LPCTSTR pszLocalFile;
	LPCTSTR pszProxy;				/// can be NULL
	LPCTSTR pszProxyUser;			/// can be NULL
	LPCTSTR pszProxyPass;			/// can be NULL
	LPCTSTR pszMethod;				/// can be NULL
	LPCTSTR pszHeaders;				/// can be NULL
	LPVOID pData;					/// can be NULL
	ULONG iDataSize;				/// can be 0
	ULONG iTimeoutConnect;			/// can be DEFAULT_VALUE
	ULONG iTimeoutReconnect;		/// can be DEFAULT_VALUE
	ULONG iOptConnectRetries;		/// can be DEFAULT_VALUE
	ULONG iOptConnectTimeout;		/// can be DEFAULT_VALUE
	ULONG iOptReceiveTimeout;		/// can be DEFAULT_VALUE
	ULONG iOptSendTimeout;			/// can be DEFAULT_VALUE
	LPCTSTR pszReferrer;			/// can be NULL
	ULONG iHttpInternetFlags;		/// can be DEFAULT_VALUE
	ULONG iHttpSecurityFlags;		/// can be DEFAULT_VALUE
} QUEUE_REQUEST_PARAM, *PQUEUE_REQUEST_PARAM;

#define RequestParamInit(Param) \
	MyZeroMemory( &Param, sizeof( Param ) ); \
	Param.iPriority = DEFAULT_VALUE; \
	Param.iTimeoutConnect = Param.iTimeoutReconnect = DEFAULT_VALUE; \
	Param.iOptConnectRetries = Param.iOptConnectTimeout = Param.iOptReceiveTimeout = Param.iOptSendTimeout= DEFAULT_VALUE; \
	Param.iHttpInternetFlags = Param.iHttpSecurityFlags = DEFAULT_VALUE;

#define RequestParamDestroy(Param) \
	MyFree( Param.pszMethod ); \
	MyFree( Param.pszURL ); \
	MyFree( Param.pszLocalFile ); \
	MyFree( Param.pszHeaders ); \
	MyFree( Param.pData ); \
	MyFree( Param.pszProxy ); \
	MyFree( Param.pszProxyUser ); \
	MyFree( Param.pszProxyPass ); \
	MyFree( Param.pszReferrer ); \
	MyZeroMemory( &Param, sizeof( Param ) );


//++ struct QUEUE_REQUEST
typedef struct _QUEUE_REQUEST {

	ULONG iId;						/// Unique request ID
	REQUEST_STATUS iStatus;
	ULONG iPriority;				/// 0 (high prio) -> ULONG_MAX-1 (low prio)
	ULONG iDependId;

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
		LPBYTE pMemory;				/// Valid for REQUEST_LOCAL_MEMORY. Reserves MAX_MEMORY_CONTENT_LENGTH of virtual memory
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
	ULONG iOptSendTimeout;			/// InternetSetOption( INTERNET_OPTION_SEND_TIMEOUT )
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

	ULONG iConnectionDrops;			/// Number of connections dropped out

	// Runtime variables
	BOOLEAN bAbort;					/// Aborted by user
	HINTERNET hSession;				/// InternetOpen
	HINTERNET hConnect;				/// InternetConnect
	HINTERNET hRequest;				/// HttpOpenRequest
	BOOLEAN bUsingRanges;			/// The HTTP "Range" header is in use (supported by server)
	ULONG iLastCallbackStatus;		/// Last status received in InternetStatusCallback
	LPCTSTR pszSrvIP;				/// Servers resolved IP address (as string)
	LPCTSTR pszSrvHeaders;			/// HTTP headers received from server
	BOOLEAN bConnected;				/// Received INTERNET_STATUS_CONNECTED_TO_SERVER

	// Error code (Win32 and HTTP)
	ULONG iWin32Error;				/// Last Win32 error code
	LPTSTR pszWin32Error;			/// Last Win32 error code (as string)
	ULONG iHttpStatus;				/// Last HTTP status code
	LPTSTR pszHttpStatus;			/// Last HTTP status code (as string)

	struct _QUEUE_REQUEST *pNext;	/// Singly linked list

} QUEUE_REQUEST, *PQUEUE_REQUEST;


#define RequestReconnectionAllowed(pReq) \
	((pReq)->iTimeoutReconnect != DEFAULT_VALUE) && \
	((pReq)->iTimeoutReconnect > 0)

#define RequestRecvPercent(pReq) \
	(int)(((pReq)->iFileSize == 0 || (pReq)->iFileSize == INVALID_FILE_SIZE64) ? 0 : MyMulDiv64((pReq)->iRecvSize, 100, (pReq)->iFileSize))

ULONG RequestOptimalBufferSize( _In_ PQUEUE_REQUEST pReq );
	

#define RequestMatched(pReq, ID, Prio, Status) \
	(ID == ANY_REQUEST_ID || ID == pReq->iId) && \
	(Prio == ANY_PRIORITY || Prio == pReq->iPriority) && \
	(Status == ANY_STATUS || Status == pReq->iStatus)

BOOL RequestMemoryToString( _In_ PQUEUE_REQUEST pReq, _Out_ LPTSTR pszString, _In_ ULONG iStringLen );
BOOL RequestDataToString( _In_ PQUEUE_REQUEST pReq, _Out_ LPTSTR pszString, _In_ ULONG iStringLen );


//++ struct QUEUE
typedef struct _QUEUE {

	TCHAR szName[20];				/// Queue name. The default queue will be named MAIN

	// Queue
	CRITICAL_SECTION csLock;
	PQUEUE_REQUEST pHead;
	ULONG iLastId;

	// Worker threads
	THREAD pThreads[MAX_WORKER_THREADS];
	ULONG iThreadCount;					// Current thread count
	volatile LONG iThreadBusyCount;		// Busy thread count
	ULONG iThreadMaxCount;				// Maximum thread count
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

// Find a request in the queue
// The queue must be locked
PQUEUE_REQUEST QueueFind( _Inout_ PQUEUE pQueue, _In_ ULONG iReqID );	/// ...by ID
PQUEUE_REQUEST QueueFindNextWaiting( _Inout_ PQUEUE pQueue );			/// ...by status

// Add a new request in the queue
// The queue must be locked
BOOL QueueAdd(
	_Inout_ PQUEUE pQueue,
	_In_ PQUEUE_REQUEST_PARAM pParam,
	_Out_opt_ PQUEUE_REQUEST *ppReq
	);

// Remove a request from the queue and destroy it
// The queue must be locked
BOOL QueueRemove( _Inout_ PQUEUE pQueue, _In_ PQUEUE_REQUEST pReq );

// Abort a request
// The queue must be locked
// This routine will only schedule the request for abortion. It'll be aborted asynchronously by its worker thread.
// By specifying a wait period, the routine will wait until the request is actually aborted...
BOOL QueueAbort( _In_ PQUEUE pQueue, _In_ PQUEUE_REQUEST pReq, _In_opt_ DWORD dwWaitMS );

// Retrieve the queue size
// The queue must be locked
ULONG QueueSize( _Inout_ PQUEUE pQueue );

// Wake up one or more worker threads
// Returns the number of threads woken up
// The queue must be locked
int QueueWakeThreads( _In_ PQUEUE pQueue, _In_ ULONG iThreadsToWake );
