#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define MAX_MEMORY_CONTENT_LENGTH	104857600	/// 100 MiB
#define CONNECT_RETRY_DELAY			1000		/// milliseconds
#define SPEED_MEASURE_INTERVAL		1000		/// milliseconds
#define TIME_LOCKVIOLATION_WAIT 	15000
#define TIME_LOCKVIOLATION_DELAY 	500


DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _Inout_ PQUEUE_REQUEST pReq );
ULONG ThreadSetWin32Error( _Inout_ PQUEUE_REQUEST pReq, _In_ ULONG iError );
ULONG ThreadSetHttpStatus( _Inout_ PQUEUE_REQUEST pReq );


//++ ThreadIsTerminating
BOOL ThreadIsTerminating( _In_ PTHREAD pThread )
{
	assert( pThread );
	assert( pThread->hTermEvent );

	if ( WaitForSingleObject( pThread->hTermEvent, 0 ) == WAIT_TIMEOUT )
		return FALSE;

	return TRUE;
}


//++ ThreadSleep
// Returns TRUE if the sleep is uninterrupted, FALSE if the thread is shutting down
BOOL ThreadSleep(_In_ PTHREAD pThread, _In_ ULONG iMilliseconds)
{
	assert( pThread );
	assert( pThread->hTermEvent );

	return (WaitForSingleObject( pThread->hTermEvent, iMilliseconds ) == WAIT_TIMEOUT);
}


//++ ThreadProc
DWORD WINAPI ThreadProc( _In_ PTHREAD pThread )
{
	HANDLE handles[2];
	PQUEUE_REQUEST pReq;

	assert( pThread );
	assert( pThread->hTermEvent );
	assert( pThread->hWakeEvent );

	TRACE( _T( "  Th:%s started\n" ), pThread->szName );

	handles[0] = pThread->hTermEvent;
	handles[1] = pThread->hWakeEvent;

	while ( TRUE )
	{
		// Check TERM signal
		if ( ThreadIsTerminating( pThread )) {
			TRACE( _T( "  Th:%s received TERM signal\n" ), pThread->szName );
			break;
		}

		// Dequeue the first waiting request
		QueueLock( (PQUEUE)pThread->pQueue );
		pReq = QueueFindNextWaiting( (PQUEUE)pThread->pQueue );
		if (pReq) {
			pReq->pThread = pThread;
			GetLocalFileTime( &pReq->tmConnect );
			pReq->iStatus = REQUEST_STATUS_DOWNLOADING;
			///TRACE( _T( "  Th:%s Id:%u Dequeued\n" ), pThread->szName, pReq->iId );
		} else {
			TRACE( _T( "  Th:%s going to sleep (empty queue)\n" ), pThread->szName );
		}
		QueueUnlock( (PQUEUE)pThread->pQueue );

		// Start downloading
		if (pReq) {

			ThreadDownload( pReq );
			GetLocalFileTime( &pReq->tmDisconnect );
			pReq->iStatus = REQUEST_STATUS_DONE;

			/// There may be transfer requests waiting for this one (dependency)
			/// Wake up all threads to check out
			QueueWakeThreads( (PQUEUE)pThread->pQueue, 0xffff );

		} else {

			// Wait for something to happen
			DWORD iWait = WaitForMultipleObjects( 2, handles, FALSE, INFINITE );
			if ( iWait == WAIT_OBJECT_0 ) {
				/// TERM event
				TRACE( _T( "  Th:%s received TERM signal\n" ), pThread->szName );
				break;
			}
			else if ( iWait == WAIT_OBJECT_0 + 1 ) {
				/// WAKE event
				TRACE( _T( "  Th:%s received WAKE signal\n" ), pThread->szName );
			}
			else {
				/// Some error...
				DWORD err = GetLastError();
				TRACE( _T( "  [!] Th:%s, WaitForMultipleObjects(...) == %u, GLE == 0x%x" ), pThread->szName, iWait, err );
				break;
			}
		}
	}

	TRACE( _T( "  Th:%s terminated\n" ), pThread->szName );
	return 0;
}


//++ ThreadDownload_QueryContentLength64
ULONG ThreadDownload_QueryContentLength64( _In_ HINTERNET hFile, _Out_ PULONG64 piContentLength )
{
	ULONG err = ERROR_SUCCESS;
	TCHAR szContentLength[128];
	ULONG iDataSize = sizeof( szContentLength );

	assert( hFile );
	assert( piContentLength );

	if ( HttpQueryInfo( hFile, HTTP_QUERY_CONTENT_LENGTH, szContentLength, &iDataSize, NULL ) ) {
		if ( StrToInt64Ex( szContentLength, STIF_DEFAULT, piContentLength ) ) {
			/// SUCCESS
		} else {
			err = ERROR_DATATYPE_MISMATCH;
			*piContentLength = INVALID_FILE_SIZE64;
		}
	} else {
		err = GetLastError();
		*piContentLength = INVALID_FILE_SIZE64;
	}

	return err;
}


//++ ThreadDownload_StatusCallback
void CALLBACK ThreadDownload_StatusCallback(
	_In_ HINTERNET hRequest,
	_In_ DWORD_PTR dwContext,
	_In_ DWORD dwInternetStatus,
	_In_ LPVOID lpvStatusInformation,
	_In_ DWORD dwStatusInformationLength
	)
{
	PQUEUE_REQUEST pReq = (PQUEUE_REQUEST)dwContext;

	/// Remember the status
	pReq->iLastCallbackStatus = dwInternetStatus;

	/// Inspect the status
	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_RESOLVING_NAME:
	{
		PCTSTR pszName = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RESOLVING_NAME \"%s\" )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, pszName );
		break;
	}
	case INTERNET_STATUS_NAME_RESOLVED:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		/// Remember server's IP
		if (pszAddrA) {
#ifdef _UNICODE
			TCHAR szAddr[50];
			if (MultiByteToWideChar( CP_ACP, 0, pszAddrA, -1, szAddr, ARRAYSIZE( szAddr ) ) > 0) {
				MyFree( pReq->pszSrvIP );
				MyStrDup( pReq->pszSrvIP, szAddr );
			}
#else
			MyStrDup( pReq->pszSrvIP, pszAddrA );
#endif
		}
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_NAME_RESOLVED \"%hs\" )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTING_TO_SERVER \"%hs\" )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTED_TO_SERVER \"%hs\" )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, pszAddrA );
		pReq->bConnected = TRUE;	/// NOTE: INTERNET_STATUS_CONNECTED_TO_SERVER might not be received when connecting to the same server multiple times
		break;
	}
	case INTERNET_STATUS_SENDING_REQUEST:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_SENDING_REQUEST )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_REQUEST_SENT:
	{
		DWORD dwBytesSent = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REQUEST_SENT BytesSent:%u )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, dwBytesSent );
		pReq->bConnected = TRUE;	/// INTERNET_STATUS_CONNECTED_TO_SERVER is not reliable...
		break;
	}
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		/// Too noisy...
		///TRACE2( _T("  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RECEIVING_RESPONSE )\n"), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		/// Too noisy...
		///TRACE2( _T("  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RESPONSE_RECEIVED )\n"), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CTL_RESPONSE_RECEIVED )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_PREFETCH:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_PREFETCH )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CLOSING_CONNECTION:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CLOSING_CONNECTION )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CONNECTION_CLOSED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTION_CLOSED )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
	{
		HINTERNET hNewHandle = *((HINTERNET*)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_HANDLE_CREATED, Handle:0x%p )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, hNewHandle );
		break;
	}
	case INTERNET_STATUS_HANDLE_CLOSING:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_HANDLE_CLOSING )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_DETECTING_PROXY:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_DETECTING_PROXY )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_REQUEST_COMPLETE:
	{
		LPINTERNET_ASYNC_RESULT pResult = (LPINTERNET_ASYNC_RESULT)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REQUEST_COMPLETE )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	}
	case INTERNET_STATUS_REDIRECT:
	{
		PCTSTR pszNewURL = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REDIRECT \"%s\" )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, pszNewURL );
		break;
	}
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_INTERMEDIATE_RESPONSE )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_USER_INPUT_REQUIRED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_USER_INPUT_REQUIRED )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_STATE_CHANGE:
	{
		DWORD dwFlags = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_STATE_CHANGE Flags:0x%08x )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, dwFlags );
		break;
	}
	case INTERNET_STATUS_COOKIE_SENT:
	{
		DWORD dwSentCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_SENT CookiesSent:%u)\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, dwSentCookies );
		break;
	}
	case INTERNET_STATUS_COOKIE_RECEIVED:
	{
		DWORD dwRecvCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_RECEIVED CookiesRecv:%u )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus, dwRecvCookies );
		break;
	}
	case INTERNET_STATUS_PRIVACY_IMPACTED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_PRIVACY_IMPACTED )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_HEADER:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_P3P_HEADER )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_POLICYREF:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_P3P_POLICYREF )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_COOKIE_HISTORY:
	{
		InternetCookieHistory *pHistory = (InternetCookieHistory*)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_HISTORY )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	}
	default:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_UNKNOWN )\n" ), pReq->pThread->szName, pReq->iId, hRequest, dwInternetStatus );
		break;
	}
}


//++ ThreadDownload_CloseSession
VOID ThreadDownload_CloseSession( _Inout_ PQUEUE_REQUEST pReq )
{
	DWORD err;
	assert( pReq );
	assert( pReq->hSession );

	if (pReq->hSession) {
		err = InternetCloseHandle( pReq->hSession ) ? ERROR_SUCCESS : GetLastError();
		TRACE2( _T( "  Th:%s Id:%u InternetCloseHandle( 0x%p, InternetOpen ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hSession, err, err );
		pReq->hSession = NULL;
	}
}


//++ ThreadDownload_OpenSession
BOOL ThreadDownload_OpenSession( _Inout_ PQUEUE_REQUEST pReq )
{
	DWORD err = ERROR_SUCCESS;
	DWORD dwSecureProtocols;
	assert( pReq );
	assert( pReq->hSession == NULL );

	// HTTPS requires at least TLS 1.0 (users might disable it from Control Panel\Internet Options)
	// Make sure TLS 1.0 is enabled
#define PROTO_SSL2   0x0008
#define PROTO_SSL3   0x0020
#define PROTO_TLS1   0x0080		/// from WinHttp.h
#define PROTO_TLS1_1 0x0200
#define PROTO_TLS1_2 0x0800

#define SECURE_PROTOCOLS_KEY _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings")
#define SECURE_PROTOCOLS_VAL _T("SecureProtocols")

	if (pReq->pszURL && CompareString( 0, NORM_IGNORECASE, pReq->pszURL, 8, _T( "https://" ), -1 ) == CSTR_EQUAL)
		RegReadDWORD( HKEY_CURRENT_USER, SECURE_PROTOCOLS_KEY, SECURE_PROTOCOLS_VAL, &dwSecureProtocols );
		if (!(dwSecureProtocols & PROTO_TLS1))
			RegWriteDWORD( HKEY_CURRENT_USER, SECURE_PROTOCOLS_KEY, SECURE_PROTOCOLS_VAL, dwSecureProtocols | PROTO_TLS1 );

	// Create a WinINet session
	pReq->hSession = InternetOpen( TEXT_USERAGENT, pReq->pszProxy ? INTERNET_OPEN_TYPE_PROXY : INTERNET_OPEN_TYPE_PRECONFIG, pReq->pszProxy, NULL, 0 );
	err = pReq->hSession ? ERROR_SUCCESS : GetLastError();
	TRACE2( _T( "  Th:%s Id:%u InternetOpen( 0x%p, Agent:%s, Proxy:%s ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hSession, TEXT_USERAGENT, pReq->pszProxy, err, err );
	if (pReq->hSession) {

		// Set callback function
		InternetSetStatusCallback( pReq->hSession, ThreadDownload_StatusCallback );

		/// Authenticated proxy
		if (pReq->pszProxy) {
			if (pReq->pszProxyUser && *pReq->pszProxyUser)
				verify( InternetSetOption( pReq->hSession, INTERNET_OPTION_PROXY_USERNAME, pReq->pszProxyUser, lstrlen( pReq->pszProxyUser ) ) );
			if (pReq->pszProxyPass && *pReq->pszProxyPass)
				verify( InternetSetOption( pReq->hSession, INTERNET_OPTION_PROXY_PASSWORD, pReq->pszProxyPass, lstrlen( pReq->pszProxyPass ) ) );
		}

		/// Options
		if (pReq->iOptConnectRetries != DEFAULT_VALUE)
			verify( InternetSetOption( pReq->hSession, INTERNET_OPTION_CONNECT_RETRIES, &pReq->iOptConnectRetries, sizeof( pReq->iOptConnectRetries ) ) );
		if (pReq->iOptConnectTimeout != DEFAULT_VALUE)
			verify( InternetSetOption( pReq->hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &pReq->iOptConnectTimeout, sizeof( pReq->iOptConnectTimeout ) ) );

		/// Reconnect if disconnected by user
		if ( TRUE ) {
			DWORD dwConnectState = 0;
			DWORD dwConnectStateSize = sizeof( dwConnectState );
			if (InternetQueryOption( pReq->hSession, INTERNET_OPTION_CONNECTED_STATE, &dwConnectState, &dwConnectStateSize ) &&
				(dwConnectState & INTERNET_STATE_DISCONNECTED_BY_USER) )
			{
				INTERNET_CONNECTED_INFO ci = { INTERNET_STATE_CONNECTED, 0 };
				verify( InternetSetOption( pReq->hSession, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof( ci ) ) );
			}
		}

	} else {
		err = ThreadSetWin32Error( pReq, GetLastError() );	/// InternetOpen
	}

	// Restore TLS 1.0 original value
	if (pReq->pszURL && CompareString( 0, NORM_IGNORECASE, pReq->pszURL, 8, _T( "https://" ), -1 ) == CSTR_EQUAL)
		if (!(dwSecureProtocols & PROTO_TLS1))
			RegWriteDWORD( HKEY_CURRENT_USER, SECURE_PROTOCOLS_KEY, SECURE_PROTOCOLS_VAL, dwSecureProtocols );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_RemoteDisconnect
VOID ThreadDownload_RemoteDisconnect( _Inout_ PQUEUE_REQUEST pReq )
{
	DWORD err;
	assert( pReq );

	if (pReq->hRequest) {
		err = InternetCloseHandle( pReq->hRequest ) ? ERROR_SUCCESS : GetLastError();
		TRACE2( _T( "  Th:%s Id:%u InternetCloseHandle( 0x%p, HttpOpenRequest ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hRequest, err, err );
		pReq->hRequest = NULL;
	}
	if (pReq->hConnect) {
		err = InternetCloseHandle( pReq->hConnect ) ? ERROR_SUCCESS : GetLastError();
		TRACE2( _T( "  Th:%s Id:%u InternetCloseHandle( 0x%p, InternetConnect ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hConnect, err, err );
		pReq->hConnect = NULL;
	}
}


//++ ThreadDownload_RemoteConnect
BOOL ThreadDownload_RemoteConnect( _Inout_ PQUEUE_REQUEST pReq, _In_ BOOL bReconnecting )
{
	BOOL bRet = FALSE;
	DWORD err, dwStartTime;
	ULONG i, iTimeout;
	URL_COMPONENTS uc;

	assert( pReq );
	assert( pReq->hConnect == NULL );
	assert( pReq->hRequest == NULL );

	// Grand timeout value
	if ( bReconnecting ) {
		iTimeout = (pReq->iTimeoutReconnect != DEFAULT_VALUE ? pReq->iTimeoutReconnect : 0);	/// Default: 0ms
	} else {
		iTimeout = (pReq->iTimeoutConnect != DEFAULT_VALUE ? pReq->iTimeoutConnect : 0);		/// Default: 0ms
	}

	// Crack the URL
	uc.dwStructSize = sizeof( uc );
	uc.dwSchemeLength = 50;
	uc.lpszScheme = MyAllocStr( uc.dwSchemeLength );
	uc.nScheme = INTERNET_SCHEME_DEFAULT;
	uc.dwHostNameLength = 128;
	uc.lpszHostName = MyAllocStr( uc.dwHostNameLength );
	uc.nPort = 0;
	uc.dwUserNameLength = 1024;
	uc.lpszUserName = MyAllocStr( uc.dwUserNameLength );
	uc.dwPasswordLength = 1024;
	uc.lpszPassword = MyAllocStr( uc.dwPasswordLength );
	uc.dwUrlPathLength = 1024;
	uc.lpszUrlPath = MyAllocStr( uc.dwUrlPathLength );
	uc.dwExtraInfoLength = 1024;
	uc.lpszExtraInfo = MyAllocStr( uc.dwExtraInfoLength );

	if ( uc.lpszScheme && uc.lpszHostName && uc.lpszUserName && uc.lpszPassword && uc.lpszUrlPath && uc.lpszExtraInfo ) {
		err = InternetCrackUrl( pReq->pszURL, 0, 0, &uc ) ? ERROR_SUCCESS : GetLastError();
		TRACE2( _T( "  Th:%s Id:%u InternetCrackUrl( %s ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->pszURL, err, err );
		if (err == ERROR_SUCCESS) {

			/// Multiple attempts to connect
			for (dwStartTime = GetTickCount(), i = 0; TRUE; i++) {

				/// Check TERM event, Check ABORT flag
				if (ThreadIsTerminating( pReq->pThread ) || pReq->bAbort) {
					ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );
					break;
				}

				/// Timed out?
				if ( i > 0 ) {
					if ( GetTickCount() - dwStartTime < iTimeout ) {
						/// Pause between attempts. Keep monitoring TERM event
						if (!ThreadSleep( pReq->pThread, CONNECT_RETRY_DELAY )) {
							ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );
							break;	/// Cancelled
						}
					} else {
						break;	/// Timeout
					}
				}

				TRACE(
					_T( "  Th:%s Id:%u %s(#%d, Elapsed:%ums/%ums, %s %s)\n" ),
					pReq->pThread->szName, pReq->iId,
					bReconnecting ? _T( "Reconnect" ) : _T( "Connect" ),
					i + 1,
					GetTickCount() - dwStartTime, iTimeout,
					pReq->szMethod, pReq->pszURL
					);

				// Connect
				pReq->hConnect = InternetConnect(
					pReq->hSession,
					uc.lpszHostName, uc.nPort,
					*uc.lpszUserName ? uc.lpszUserName : NULL,
					*uc.lpszPassword ? uc.lpszPassword : NULL,
					INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)pReq
					);
				err = pReq->hConnect ? ERROR_SUCCESS : GetLastError();
				TRACE2( _T( "  Th:%s Id:%u InternetConnect( 0x%p, %ws, %hu ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hSession, uc.lpszHostName, uc.nPort, err, err );

				if (pReq->hConnect) {

					// Check the TERM event, Check ABORT flag
					if (!ThreadIsTerminating( pReq->pThread ) && !pReq->bAbort) {

						// Make an HTTP request
						LPCTSTR szReqType[2] = { _T( "*/*" ), 0 };
						ULONG iObjectNameLen = uc.dwUrlPathLength + uc.dwExtraInfoLength;
						LPTSTR pszObjectName = MyAllocStr( iObjectNameLen );

						/// Set callback function
						InternetSetStatusCallback( pReq->hConnect, ThreadDownload_StatusCallback );

						assert( pszObjectName );
						if ( pszObjectName ) {

							wnsprintf( pszObjectName, iObjectNameLen + 1, _T( "%s%s" ), uc.lpszUrlPath, uc.lpszExtraInfo );

							if (pReq->iHttpInternetFlags == DEFAULT_VALUE) {
								pReq->iHttpInternetFlags =
									INTERNET_FLAG_NO_CACHE_WRITE |
									INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
									///INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
									INTERNET_FLAG_NO_COOKIES |
									INTERNET_FLAG_NO_UI |
									INTERNET_FLAG_RELOAD;
							}
							if ( uc.nScheme == INTERNET_SCHEME_HTTPS )
								pReq->iHttpInternetFlags |= INTERNET_FLAG_SECURE;

							pReq->hRequest = HttpOpenRequest(
								pReq->hConnect,
								pReq->szMethod,
								pszObjectName,
								_T( "HTTP/1.1" ),
								pReq->pszReferer,
								szReqType,
								pReq->iHttpInternetFlags,
								(DWORD_PTR)pReq		/// Context
								);
							err = pReq->hRequest ? ERROR_SUCCESS : GetLastError();
							TRACE2( _T( "  Th:%s Id:%u HttpOpenRequest( 0x%p, %s, %s ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hConnect, pReq->szMethod, pszObjectName, err, err );

							MyFree( pszObjectName );

						} else {
							err = ERROR_OUTOFMEMORY;	/// MyAllocStr
						}

						if (pReq->hRequest) {

							// Set callback function
							InternetSetStatusCallback( pReq->hRequest, ThreadDownload_StatusCallback );

							/// Options
							if (pReq->iOptReceiveTimeout != DEFAULT_VALUE)
								verify( InternetSetOption( pReq->hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &pReq->iOptReceiveTimeout, sizeof( pReq->iOptReceiveTimeout ) ) );
							if (pReq->iOptSendTimeout != DEFAULT_VALUE)
								verify( InternetSetOption( pReq->hRequest, INTERNET_OPTION_SEND_TIMEOUT, &pReq->iOptSendTimeout, sizeof( pReq->iOptSendTimeout ) ) );

							// On some Vistas (Home edition), HttpSendRequest returns ERROR_INTERNET_SEC_CERT_REV_FAILED if authenticated proxy is used
							// By default, we ignore the revocation information
							if (pReq->iHttpSecurityFlags == DEFAULT_VALUE) {
								pReq->iHttpSecurityFlags =
									SECURITY_FLAG_IGNORE_REVOCATION |
									///SECURITY_FLAG_IGNORE_UNKNOWN_CA |
									///SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
									SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
							}
							InternetSetOption( pReq->hRequest, INTERNET_OPTION_SECURITY_FLAGS, &pReq->iHttpSecurityFlags, sizeof( DWORD ) );

							// Ignore 'Work offline' setting
							InternetSetOption( pReq->hRequest, INTERNET_OPTION_IGNORE_OFFLINE, 0, 0 );

							// Check TERM event, Check ABORT flag
							if (!ThreadIsTerminating( pReq->pThread ) && !pReq->bAbort) {

								/// Add the Range header to resume the transfer
								/// NOTE: If the file is already downloaded, the server will return HTTP status 416 (see below!)
								pReq->bRangeSent = FALSE;
								if (pReq->iRecvSize > 0) {
									TCHAR szRangeHeader[255];
									wnsprintf( szRangeHeader, ARRAYSIZE( szRangeHeader ), _T( "Range: bytes=%I64u-" ), pReq->iRecvSize );
									pReq->bRangeSent = HttpAddRequestHeaders( pReq->hRequest, szRangeHeader, -1, HTTP_ADDREQ_FLAG_ADD_IF_NEW );
								}

							_send_request:

								// Send the HTTP request
								err = HttpSendRequest( pReq->hRequest, pReq->pszHeaders, -1, pReq->pData, pReq->iDataSize ) ? ERROR_SUCCESS : GetLastError();
								TRACE2( _T( "  Th:%s Id:%u HttpSendRequest( 0x%p, Headers:%s, Data:%hs ) == (0x%x) %d\n" ), pReq->pThread->szName, pReq->iId, pReq->hRequest, pReq->pszHeaders, (LPSTR)pReq->pData, err, err );
								if (err == ERROR_SUCCESS) {

									ULONG iHttpStatus;

									/// HTTP headers
									if ( TRUE ) {

										TCHAR szHeaders[512];
										ULONG iHeadersSize = sizeof(szHeaders);

										szHeaders[0] = 0;
										if (HttpQueryInfo( pReq->hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, szHeaders, &iHeadersSize, NULL )) {
											MyFree( pReq->pszSrvHeaders );
											MyStrDup( pReq->pszSrvHeaders, szHeaders );	/// Remember the headers
										} else {
											err = GetLastError();
										}

										/// Debugging
										if (!bReconnecting) {
											LPTSTR psz;
											for (psz = szHeaders; *psz; psz++) {
												if (*psz == _T( '\r' ))
													*psz = _T( '\\' );
												if (*psz == _T( '\n' ))
													*psz = _T( 'n' );
											}
											TRACE( _T( "  Th:%s Id:%u HttpQueryInfo( HTTP_QUERY_RAW_HEADERS_CRLF ) == 0x%x, \"%s\"\n" ), pReq->pThread->szName, pReq->iId, err, szHeaders );
										}
									}

									/// HTTP status code
									iHttpStatus = ThreadSetHttpStatus( pReq );

									// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
									if ( iHttpStatus <= 299 ) {

										/// 1xx Informational
										/// 2xx Success

										// Extract the remote content length
										if (ThreadDownload_QueryContentLength64( pReq->hRequest, &pReq->iFileSize ) == ERROR_SUCCESS) {
											if (pReq->bRangeSent) {
												/// If the Range header was used, the remote content length represents the amount not yet downloaded,
												/// rather the full file size
												pReq->iFileSize += pReq->iRecvSize;
											}
										}

										/// Success. Break the loop
										bRet = TRUE;
										break;

									} else {

										/// 3xx Redirection - The client must take additional action to complete the request
										/// 4xx Client Error
										/// 5xx Server Error

										if ( iHttpStatus == 416 ) {								/// 416 Requested Range Not Satisfiable
											if (pReq->bRangeSent) {
												/// Download all remote content before making another HTTP request
												if ( TRUE ) {
													CHAR szBufA[255];
													DWORD iBytesRecv;
													while ((InternetReadFile( pReq->hRequest, szBufA, ARRAYSIZE( szBufA ), &iBytesRecv )) && (iBytesRecv > 0));
												}
												/// Retry without the Range header
												pReq->bRangeSent = FALSE;
												HttpAddRequestHeaders( pReq->hRequest, _T( "Range:" ), -1, HTTP_ADDREQ_FLAG_REPLACE );
												goto _send_request;
											}
										}

										if ( iHttpStatus != HTTP_STATUS_SERVICE_UNAVAIL &&		/// 503 Service Unavailable
											iHttpStatus != HTTP_STATUS_GATEWAY_TIMEOUT &&		/// 504 Gateway Timeout
											iHttpStatus != 598 &&								/// 598 Network read timeout error (Unknown)
											iHttpStatus != 599 )								/// 599 Network connect timeout error (Unknown)
										{
											/// Error. Break the loop
											break;
										}
									}

								} else {
									ThreadSetWin32Error( pReq, err );	/// HttpSendRequest error
								}
							} else {
								ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating || bAbort
								break;
							}
						} else {
							ThreadSetWin32Error( pReq, err );	/// HttpOpenRequest error
						}
					} else {
						ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating || bAbort
						break;
					}
				} else {
					ThreadSetWin32Error( pReq, err );	/// InternetConnect error
				}

				TRACE(
					_T( "  Th:%s Id:%u Status (#%d, %u \"%s\")\n" ),
					pReq->pThread->szName, pReq->iId,
					i + 1,
					pReq->iWin32Error == ERROR_SUCCESS ? pReq->iHttpStatus : pReq->iWin32Error,
					pReq->iWin32Error == ERROR_SUCCESS ? pReq->pszHttpStatus : pReq->pszWin32Error
					);

			}	/// for
		} else {
			ThreadSetWin32Error( pReq, err );	/// InternetCrackUrl error
		}
	} else {
		ThreadSetWin32Error( pReq, ERROR_OUTOFMEMORY );	/// MyAllocStr
	}

	MyFree( uc.lpszScheme );
	MyFree( uc.lpszHostName );
	MyFree( uc.lpszUserName );
	MyFree( uc.lpszPassword );
	MyFree( uc.lpszUrlPath );
	MyFree( uc.lpszExtraInfo );

	/// Cleanup
	if ( !bRet )
		ThreadDownload_RemoteDisconnect( pReq );

	return bRet;
}


//++ ThreadDownload_LocalCreate1
ULONG ThreadDownload_LocalCreate1( _Inout_ PQUEUE_REQUEST pReq )
{
	///
	/// NOTE:
	/// This function is called *before* sending the HTTP request
	/// The remote content length (pReq->iFileSize) is unknown
	/// We'll open the local file and get its size
	///
	ULONG err = ERROR_SUCCESS;

	assert( pReq );
	assert( pReq->hRequest == NULL );			/// Must not be connected

	/// Cleanup
	pReq->iRecvSize = 0;

	switch (pReq->iLocalType) {

		case REQUEST_LOCAL_NONE:
		{
			// Exit immediately
			break;
		}

		case REQUEST_LOCAL_FILE:
		{
			DWORD dwTime;
			assert( !VALID_FILE_HANDLE( pReq->Local.hFile ) );			/// File must not be opened
			assert( pReq->Local.pszFile && *pReq->Local.pszFile );	/// File name must not be empty

			// Try and open already existing file (resume)
			dwTime = GetTickCount();
		_create_file:
			pReq->Local.hFile = CreateFile( pReq->Local.pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if (pReq->Local.hFile != INVALID_HANDLE_VALUE) {
				LARGE_INTEGER iExistingSize;
				if (GetFileSizeEx( pReq->Local.hFile, &iExistingSize )) {
					/// SUCCESS
					pReq->iRecvSize = iExistingSize.QuadPart;
				} else {
					err = GetLastError();	/// GetFileSizeEx
				}
			} else {
				err = GetLastError();	/// CreateFile
				if (err == ERROR_LOCK_VIOLATION && (GetTickCount() - dwTime < TIME_LOCKVIOLATION_WAIT)) {
					/// The file is locked by some other process. Retry...
					Sleep( TIME_LOCKVIOLATION_DELAY );
					goto _create_file;
				}
			}

			/// Handle errors
			if ((err != ERROR_SUCCESS) && VALID_FILE_HANDLE( pReq->Local.hFile )) {
				CloseHandle( pReq->Local.hFile );
				pReq->Local.hFile = NULL;
			}
			break;
		}

		case REQUEST_LOCAL_MEMORY:
		{
			assert( pReq->Local.pMemory == NULL );
			break;
		}
	}

	/// Handle errors
	ThreadSetWin32Error( pReq, err );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_LocalCreate2
ULONG ThreadDownload_LocalCreate2( _Inout_ PQUEUE_REQUEST pReq )
{
	///
	/// NOTE:
	/// This function is called *after* sending the HTTP request
	/// At this point, the remote content length is usually known, unless the server fails to report it
	/// Resume the transfer if possible, otherwise start all over
	///
	ULONG err = ERROR_SUCCESS;

	assert( pReq );
	assert( pReq->hRequest != NULL );			/// Must be connected

	switch (pReq->iLocalType) {

		case REQUEST_LOCAL_NONE:
		{
			// Exit immediately
			break;
		}

		case REQUEST_LOCAL_FILE:
		{
			assert( VALID_FILE_HANDLE( pReq->Local.hFile ) );		/// The file must already be opened

			// Determine if resuming is possible
			if (pReq->iFileSize != INVALID_FILE_SIZE64) {
				if (!pReq->bRangeSent || (pReq->iHttpStatus == HTTP_STATUS_PARTIAL_CONTENT)) {	/// Server supports the Range header
					if (pReq->iRecvSize <= pReq->iFileSize) {
						ULONG iZero = 0;
						if ((SetFilePointer( pReq->Local.hFile, 0, &iZero, FILE_END ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS)) {
							/// SUCCESS (resume)
						} else {
							err = GetLastError();		/// SetFilePointer
						}
					} else {
						err = ERROR_FILE_TOO_LARGE;	/// Local file larger than the remote file
					}
				} else {
					err = ERROR_UNSUPPORTED_TYPE;/// The server doesn't support Range header
				}
			} else {
				err = ERROR_INVALID_DATA;	/// The remote content length is still unknown
			}

			// Full download if we can't resume
			if (err != ERROR_SUCCESS) {
				ULONG iZero = 0;
				if ((SetFilePointer( pReq->Local.hFile, 0, &iZero, FILE_BEGIN ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS)) {
					if (SetEndOfFile( pReq->Local.hFile )) {
						/// SUCCESS (full download)
						pReq->iRecvSize = 0;
						err = ERROR_SUCCESS;
					} else {
						err = GetLastError();	/// SetEndOfFile
					}
				} else {
					err = GetLastError();	/// SetFilePointer
				}
			}

			/// Handle errors
			if ((err != ERROR_SUCCESS) && VALID_FILE_HANDLE( pReq->Local.hFile )) {
				CloseHandle( pReq->Local.hFile );
				pReq->Local.hFile = NULL;
			}
			break;
		}

		case REQUEST_LOCAL_MEMORY:
		{
			/// NOTE: If we're reconnecting, the memory buffer is already be allocated. We'll resume the transfer...
			if (pReq->iFileSize != INVALID_FILE_SIZE64) {
				if (pReq->iFileSize <= MAX_MEMORY_CONTENT_LENGTH) {		// Size limit
					if (!pReq->Local.pMemory) {
						pReq->Local.pMemory = (LPBYTE)MyAlloc( (SIZE_T)pReq->iFileSize );
						if (pReq->Local.pMemory) {
							/// SUCCESS (full download)
						} else {
							err = ERROR_OUTOFMEMORY;	/// MyAlloc
						}
					} else {
						/// SUCCESS (resume)
						/// iRecvSize already set
					}
				} else {
					err = ERROR_FILE_TOO_LARGE;	/// The remote content length exceeds the limit
				}
			} else {
				err = ERROR_INVALID_DATA;	/// The remote content length is still unknown
			}
			break;
		}
	}

	/// Handle errors
	ThreadSetWin32Error( pReq, err );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_LocalClose
BOOL ThreadDownload_LocalClose( _Inout_ PQUEUE_REQUEST pReq )
{
	BOOL bRet = TRUE;

	assert( pReq );

	switch (pReq->iLocalType) {

		case REQUEST_LOCAL_NONE:
		{
			break;
		}

		case REQUEST_LOCAL_FILE:
		{
			if (VALID_FILE_HANDLE( pReq->Local.hFile )) {
				bRet = CloseHandle( pReq->Local.hFile );
				pReq->Local.hFile = NULL;
			}
			break;
		}

		case REQUEST_LOCAL_MEMORY:
		{
			///MyFree( pReq->Local.pMemory );	// Memory content must remain available after the transfer has completed
			break;
		}
	}

	return bRet;
}


//++ ThreadDownload_RefreshSpeed
void ThreadDownload_RefreshSpeed( _Inout_ PQUEUE_REQUEST pReq, _In_ BOOL bXferFinished )
{
	BOOL bFormatString = FALSE;
	assert( pReq );

	// Compute speed
	if (bXferFinished) {

		ULONG iTimeDiff = MyTimeDiff( &pReq->Xfer.tmEnd, &pReq->Xfer.tmStart );		/// Milliseconds
		if (iTimeDiff == 0)
			iTimeDiff = 1;	/// Prevent accidental division by zero
///		if (iTimeDiff >= SPEED_MEASURE_INTERVAL) {
			pReq->Speed.iSpeed = (ULONG)MyDoubleToUlonglong( MyUlonglongToDouble( pReq->Xfer.iXferSize ) / (MyUlonglongToDouble( iTimeDiff ) / 1000.0F) + 0.5F );
///		} else {
///			pReq->Speed.iSpeed = (ULONG)pReq->Xfer.iXferSize;
///		}
		bFormatString = TRUE;

		pReq->Speed.iChunkTime = 0;
		pReq->Speed.iChunkSize = 0;

	} else {

		ULONG iTimeDiff = GetTickCount() - pReq->Speed.iChunkTime;
		if (iTimeDiff >= SPEED_MEASURE_INTERVAL) {

			pReq->Speed.iSpeed = (ULONG)MyDoubleToUlonglong( MyUlonglongToDouble( pReq->Speed.iChunkSize ) / (MyUlonglongToDouble( iTimeDiff ) / 1000.0F) + 0.5F );
			bFormatString = TRUE;

			pReq->Speed.iChunkSize = 0;
			pReq->Speed.iChunkTime = GetTickCount();
		}
	}

	// Format as text
	if (bFormatString) {
#ifdef UNICODE
		StrFormatByteSizeW( (LONGLONG)pReq->Speed.iSpeed, pReq->Speed.szSpeed, ARRAYSIZE( pReq->Speed.szSpeed ) );
#else
		StrFormatByteSizeA( pReq->Speed.iSpeed, pReq->Speed.szSpeed, ARRAYSIZE( pReq->Speed.szSpeed ) );
#endif
		lstrcat( pReq->Speed.szSpeed, TEXT_PER_SECOND );
	}
}


//++ ThreadDownload_Transfer
BOOL ThreadDownload_Transfer( _Inout_ PQUEUE_REQUEST pReq )
{
	DWORD err = ERROR_SUCCESS;

	assert( pReq );
	assert( pReq->hRequest != NULL );

	/// Initializations
	GetLocalFileTime( &pReq->Xfer.tmStart );
	pReq->Xfer.tmEnd.dwLowDateTime = 0;
	pReq->Xfer.tmEnd.dwHighDateTime = 0;
	pReq->Xfer.iXferSize = 0;

	pReq->Speed.iSpeed = 0;
	lstrcpy( pReq->Speed.szSpeed, _T( "..." ) );
	pReq->Speed.iChunkTime = GetTickCount();
	pReq->Speed.iChunkSize = 0;

	/// Anything to download?
	if (pReq->iRecvSize >= pReq->iFileSize)
		return TRUE;

	// Debugging definitions
///#define DEBUG_XFER_MAX_BYTES		1024*1024
///#define DEBUG_XFER_SLOWDOWN		500
///#define DEBUG_XFER_PROGRESS

	switch (pReq->iLocalType) {

		case REQUEST_LOCAL_NONE:
		{
			/// Don't transfer anything
			break;
		}

		case REQUEST_LOCAL_FILE:
		{
			/// Allocate a transfer buffer
			LPBYTE pBuf = MyAlloc( MAX_BUFFER_SIZE );
			if ( pBuf ) {

				/// Transfer loop
				ULONG iBytesRecv, iBytesWritten;
				while (err == ERROR_SUCCESS && (pReq->iRecvSize < pReq->iFileSize)) {
#ifdef DEBUG_XFER_MAX_BYTES
					/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
					if ( pReq->Xfer.iXferSize >= DEBUG_XFER_MAX_BYTES ) {
						err = ThreadSetWin32Error( pReq, ERROR_INTERNET_CONNECTION_RESET );
						break;
					}
#endif ///DEBUG_XFER_MAX_BYTES
					if (!ThreadIsTerminating( pReq->pThread ) && !pReq->bAbort) {
						if (InternetReadFile( pReq->hRequest, pBuf, RequestOptimalBufferSize( pReq ), &iBytesRecv )) {
							if ( iBytesRecv > 0 ) {
								if (WriteFile( pReq->Local.hFile, pBuf, iBytesRecv, &iBytesWritten, NULL )) {
									/// Update fields
									pReq->iRecvSize += iBytesRecv;
									pReq->Xfer.iXferSize += iBytesRecv;
									pReq->Speed.iChunkSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
									/// Simulate transfer slow-download
									Sleep( DEBUG_XFER_SLOWDOWN );
#endif ///DEBUG_XFER_SLOWDOWN
									/// Speed measurement
									ThreadDownload_RefreshSpeed( pReq, FALSE );
#ifdef DEBUG_XFER_PROGRESS
									/// Display transfer progress
									TRACE(
										_T( "  Th:%s Id:%u ThreadTransfer(Recv:%d%% %I64u/%I64u @ %s, %s %s -> File)\n" ),
										pReq->pThread->szName, pReq->iId,
										RequestRecvPercent( pReq ),
										pReq->iRecvSize, pReq->iFileSize == INVALID_FILE_SIZE64 ? 0 : pReq->iFileSize,
										pReq->Speed.szSpeed,
										pReq->szMethod, pReq->pszURL
										);
#endif ///DEBUG_XFER_PROGRESS
								} else {
									err = ThreadSetWin32Error( pReq, GetLastError() );	/// WriteFile
								}
							} else {
								// Transfer complete
								ThreadSetWin32Error( pReq, ERROR_SUCCESS );
								ThreadSetHttpStatus( pReq );
								break;
							}
						} else {
							err = ThreadSetWin32Error( pReq, GetLastError() );	/// InternetReadFile
							pReq->iConnectionDrops++;
						}
					} else {
						err = ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );
					}
				}	///while

				MyFree( pBuf );

			} else {
				err = ERROR_OUTOFMEMORY;
			}
			break;
		}

		case REQUEST_LOCAL_MEMORY:
		{
			/// Transfer loop
			ULONG iBytesRecv;
			while (err == ERROR_SUCCESS && (pReq->iRecvSize < pReq->iFileSize)) {
#ifdef DEBUG_XFER_MAX_BYTES
				/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
				if ( pReq->Xfer.iXferSize >= DEBUG_XFER_MAX_BYTES ) {
					err = ThreadSetWin32Error( pReq, ERROR_INTERNET_CONNECTION_RESET );
					break;
				}
#endif ///DEBUG_XFER_MAX_BYTES
				if (!ThreadIsTerminating( pReq->pThread ) && !pReq->bAbort) {
					if (InternetReadFile( pReq->hRequest, pReq->Local.pMemory + pReq->iRecvSize, RequestOptimalBufferSize( pReq ), &iBytesRecv )) {
						if ( iBytesRecv > 0 ) {
							/// Update fields
							pReq->iRecvSize += iBytesRecv;
							pReq->Xfer.iXferSize += iBytesRecv;
							pReq->Speed.iChunkSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
							/// Simulate transfer slow-download
							Sleep( DEBUG_XFER_SLOWDOWN );
#endif ///DEBUG_XFER_SLOWDOWN
							/// Speed measurement
							ThreadDownload_RefreshSpeed( pReq, FALSE );
#ifdef DEBUG_XFER_PROGRESS
							/// Display transfer progress
							TRACE(
								_T( "  Th:%s Id:%u ThreadTransfer(Recv:%d%% %I64u/%I64u @ %s, %s %s -> Memory)\n" ),
								pReq->pThread->szName, pReq->iId,
								RequestRecvPercent( pReq ),
								pReq->iRecvSize, pReq->iFileSize == INVALID_FILE_SIZE64 ? 0 : pReq->iFileSize,
								pReq->Speed.szSpeed,
								pReq->szMethod, pReq->pszURL
								);
#endif ///DEBUG_XFER_PROGRESS
						} else {
							// Transfer complete
							ThreadSetWin32Error( pReq, ERROR_SUCCESS );
							ThreadSetHttpStatus( pReq );
							break;
						}
					} else {
						err = ThreadSetWin32Error( pReq, GetLastError() );	/// InternetReadFile
						pReq->iConnectionDrops++;
					}
				} else {
					err = ThreadSetWin32Error( pReq, ERROR_INTERNET_OPERATION_CANCELLED );
				}
			}			/// while
			break;		/// switch
		}
	}

	/// Finalize
	if (pReq->iFileSize == INVALID_FILE_SIZE64)
		pReq->iFileSize = pReq->iRecvSize;		/// Set the (previously unknown) file size
	GetLocalFileTime( &pReq->Xfer.tmEnd );
	ThreadDownload_RefreshSpeed( pReq, TRUE );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload
VOID ThreadDownload( _Inout_ PQUEUE_REQUEST pReq )
{
	assert( pReq );
	TRACE(
		_T( "  Th:%s Id:%u ThreadDownload(%s %s -> %s)\n" ),
		pReq->pThread->szName, pReq->iId,
		pReq->szMethod, pReq->pszURL,
		pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY)
		);

	if (pReq->pszURL && *pReq->pszURL) {

		if (ThreadDownload_LocalCreate1( pReq )) {
			if (ThreadDownload_OpenSession( pReq )) {

				BOOL bReconnectAllowed = TRUE;
				int i;
				for (i = 0; (i < 1000) && bReconnectAllowed; i++) {

					bReconnectAllowed = FALSE;
					if (ThreadDownload_RemoteConnect( pReq, (BOOL)(i > 0) )) {
						if (ThreadDownload_LocalCreate2( pReq )) {

							if (ThreadDownload_Transfer( pReq )) {
								// Success
							}

							ThreadDownload_RemoteDisconnect( pReq );

							/// Reconnect and resume?
							bReconnectAllowed =
								pReq->iWin32Error != ERROR_SUCCESS &&
								pReq->iWin32Error != ERROR_INTERNET_OPERATION_CANCELLED &&
								pReq->Xfer.iXferSize > 0 &&			/// We already received something...
								RequestReconnectionAllowed( pReq );
						}
					}
				}
				ThreadDownload_CloseSession( pReq );
			}
			ThreadDownload_LocalClose( pReq );
		}
	}

	TRACE(
		_T( "  Th:%s Id:%u ThreadDownload(Recv:%d%% %I64u @ %s, %s %s [%s] -> %s)\n" ),
		pReq->pThread->szName, pReq->iId,
		RequestRecvPercent( pReq ), pReq->iFileSize, pReq->Speed.szSpeed,
		pReq->szMethod, pReq->pszURL, pReq->pszSrvIP,
		pReq->iLocalType == REQUEST_LOCAL_NONE ? TEXT_LOCAL_NONE : (pReq->iLocalType == REQUEST_LOCAL_FILE ? pReq->Local.pszFile : TEXT_LOCAL_MEMORY)
		);
}


//++ ThreadSetWin32Error
ULONG ThreadSetWin32Error( _Inout_ PQUEUE_REQUEST pReq, _In_ ULONG iError )
{
	assert( pReq );
	if (pReq->iWin32Error != iError) {
		/// Numeric error code
		pReq->iWin32Error = iError;
		/// Error string
		MyFree( pReq->pszWin32Error );
		AllocErrorStr( pReq->iWin32Error, &pReq->pszWin32Error );
	}

	/// Handle ERROR_INTERNET_EXTENDED_ERROR
	if (pReq->iWin32Error == ERROR_INTERNET_EXTENDED_ERROR) {
		DWORD dwWebError;
		TCHAR szWebError[512];
		DWORD dwWebErrorLen = ARRAYSIZE( szWebError );
		szWebError[0] = 0;
		if ( InternetGetLastResponseInfo( &dwWebError, szWebError, &dwWebErrorLen ) ) {
			MyFree( pReq->pszWin32Error );
			MyStrDup( pReq->pszWin32Error, szWebError );
		} else {
			MyFree( pReq->pszWin32Error );
			AllocErrorStr( pReq->iWin32Error, &pReq->pszWin32Error );
		}
	}
	return iError;
}


//++ ThreadSetHttpStatus
ULONG ThreadSetHttpStatus( _Inout_ PQUEUE_REQUEST pReq )
{
	ULONG iHttpStatus = 0;

	assert( pReq );
	assert( pReq->hRequest );

	if (pReq && pReq->hRequest) {

		TCHAR szErrorText[512];
		ULONG iDataSize;

		/// Get HTTP status (numeric)
		iDataSize = sizeof( iHttpStatus );
		if (HttpQueryInfo( pReq->hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &iHttpStatus, &iDataSize, NULL )) {
			if (pReq->iHttpStatus != iHttpStatus) {

				pReq->iHttpStatus = iHttpStatus;

				/// Get HTTP status (string)
				MyFree( pReq->pszHttpStatus );
				szErrorText[0] = 0;
				iDataSize = sizeof( szErrorText );
				HttpQueryInfo( pReq->hRequest, HTTP_QUERY_STATUS_TEXT, szErrorText, &iDataSize, NULL );
				if ( *szErrorText )
					MyStrDup( pReq->pszHttpStatus, szErrorText );
			}
		}
	}

	return iHttpStatus;
}
