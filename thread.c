#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define USERAGENT					_T("Mozilla/5.0 (Windows; WOW64) xfer/1.0")
#define TRANSFER_CHUNK_SIZE			256			/// 256 KiB
#define MAX_MEMORY_CONTENT_LENGTH	104857600	/// 100 MiB
#define CONNECT_RETRY_DELAY			1000		/// milliseconds
#define SPEED_MEASURE_INTERVAL		1000		/// milliseconds
#define TIME_LOCKVIOLATION_WAIT 	15000
#define TIME_LOCKVIOLATION_DELAY 	500


DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _Inout_ PQUEUE_ITEM pItem );
ULONG ThreadSetWin32Error( _Inout_ PQUEUE_ITEM pItem, _In_ ULONG iError );
ULONG ThreadSetHttpStatus( _Inout_ PQUEUE_ITEM pItem );


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
// Returns TRUE if the sleep was uninterrupted, FALSE if the thread is shutting down
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
	PQUEUE_ITEM pItem;

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

		// Dequeue the first waiting item
		QueueLock( (PQUEUE)pThread->pQueue );
		pItem = QueueFindFirstWaiting( (PQUEUE)pThread->pQueue );
		if ( pItem ) {
			pItem->pThread = pThread;
			GetLocalFileTime( &pItem->tmConnect );
			pItem->iStatus = ITEM_STATUS_DOWNLOADING;
			///TRACE( _T( "  Th:%s Id:%u Dequeued\n" ), pThread->szName, pItem->iId );
		} else {
			TRACE( _T( "  Th:%s going to sleep (empty queue)\n" ), pThread->szName );
		}
		QueueUnlock( (PQUEUE)pThread->pQueue );

		// Start downloading
		if ( pItem ) {

			ThreadDownload( pItem );
			GetLocalFileTime( &pItem->tmDisconnect );
			pItem->iStatus = ITEM_STATUS_DONE;

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
#if DBG || _DEBUG
	PQUEUE_ITEM pItem = (PQUEUE_ITEM)dwContext;

	/// Remember the status
	pItem->iLastCallbackStatus = dwInternetStatus;

	/// Inspect the status
	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_RESOLVING_NAME:
	{
		PCTSTR pszName = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RESOLVING_NAME \"%s\" )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, pszName );
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
				MyFree( pItem->pszSrvIP );
				MyStrDup( pItem->pszSrvIP, szAddr );
			}
#else
			MyStrDup( pItem->pszSrvIP, pszAddrA );
#endif
		}
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_NAME_RESOLVED \"%hs\" )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTING_TO_SERVER \"%hs\" )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTED_TO_SERVER \"%hs\" )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, pszAddrA );
		pItem->bConnected = TRUE;
		break;
	}
	case INTERNET_STATUS_SENDING_REQUEST:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_SENDING_REQUEST )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_REQUEST_SENT:
	{
		DWORD dwBytesSent = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REQUEST_SENT BytesSent:%u )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, dwBytesSent );
		break;
	}
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		/// Too noisy...
		///TRACE2( _T("  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RECEIVING_RESPONSE )\n"), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		/// Too noisy...
		///TRACE2( _T("  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_RESPONSE_RECEIVED )\n"), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CTL_RESPONSE_RECEIVED )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_PREFETCH:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_PREFETCH )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CLOSING_CONNECTION:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CLOSING_CONNECTION )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CONNECTION_CLOSED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_CONNECTION_CLOSED )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
	{
		HINTERNET hNewHandle = *((HINTERNET*)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_HANDLE_CREATED, Handle:0x%p )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, hNewHandle );
		break;
	}
	case INTERNET_STATUS_HANDLE_CLOSING:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_HANDLE_CLOSING )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_DETECTING_PROXY:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_DETECTING_PROXY )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_REQUEST_COMPLETE:
	{
		LPINTERNET_ASYNC_RESULT pResult = (LPINTERNET_ASYNC_RESULT)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REQUEST_COMPLETE )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	}
	case INTERNET_STATUS_REDIRECT:
	{
		PCTSTR pszNewURL = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_REDIRECT \"%s\" )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, pszNewURL );
		break;
	}
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_INTERMEDIATE_RESPONSE )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_USER_INPUT_REQUIRED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_USER_INPUT_REQUIRED )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_STATE_CHANGE:
	{
		DWORD dwFlags = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_STATE_CHANGE Flags:0x%08x )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, dwFlags );
		break;
	}
	case INTERNET_STATUS_COOKIE_SENT:
	{
		DWORD dwSentCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_SENT CookiesSent:%u)\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, dwSentCookies );
		break;
	}
	case INTERNET_STATUS_COOKIE_RECEIVED:
	{
		DWORD dwRecvCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_RECEIVED CookiesRecv:%u )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus, dwRecvCookies );
		break;
	}
	case INTERNET_STATUS_PRIVACY_IMPACTED:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_PRIVACY_IMPACTED )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_HEADER:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_P3P_HEADER )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_POLICYREF:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_P3P_POLICYREF )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_COOKIE_HISTORY:
	{
		InternetCookieHistory *pHistory = (InternetCookieHistory*)lpvStatusInformation;
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_COOKIE_HISTORY )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	}
	default:
		TRACE2( _T( "  Th:%s Id:%u StatusCallback( 0x%p, [%u]INTERNET_STATUS_UNKNOWN )\n" ), pItem->pThread->szName, pItem->iId, hRequest, dwInternetStatus );
		break;
	}
#endif ///DBG || _DEBUG
}


//++ ThreadDownload_CloseSession
VOID ThreadDownload_CloseSession( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );
	assert( pItem->hSession );

	if (pItem->hSession) {
		InternetCloseHandle( pItem->hSession );
		pItem->hSession = NULL;
	}
}


//++ ThreadDownload_OpenSession
BOOL ThreadDownload_OpenSession(_Inout_ PQUEUE_ITEM pItem)
{
	DWORD err = ERROR_SUCCESS;
	assert( pItem );
	assert( pItem->hSession == NULL );

	pItem->hSession = InternetOpen( USERAGENT, pItem->pszProxy ? INTERNET_OPEN_TYPE_PROXY : INTERNET_OPEN_TYPE_PRECONFIG, pItem->pszProxy, NULL, 0 );
	if ( pItem->hSession ) {

		// Set callback function
		InternetSetStatusCallback(pItem->hSession, ThreadDownload_StatusCallback);

		/// Authenticated proxy
		if (pItem->pszProxy) {
			if (pItem->pszProxyUser && *pItem->pszProxyUser)
				verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_PROXY_USERNAME, pItem->pszProxyUser, lstrlen( pItem->pszProxyUser ) ) );
			if (pItem->pszProxyPass && *pItem->pszProxyPass)
				verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_PROXY_PASSWORD, pItem->pszProxyPass, lstrlen( pItem->pszProxyPass ) ) );
		}

		/// Options
		if (pItem->iOptConnectRetries != DEFAULT_VALUE)
			verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_RETRIES, &pItem->iOptConnectRetries, sizeof( pItem->iOptConnectRetries ) ) );
		if (pItem->iOptConnectTimeout != DEFAULT_VALUE)
			verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &pItem->iOptConnectTimeout, sizeof( pItem->iOptConnectTimeout ) ) );
		if (pItem->iOptReceiveTimeout != DEFAULT_VALUE)
			verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &pItem->iOptReceiveTimeout, sizeof( pItem->iOptReceiveTimeout ) ) );

		/// Reconnect if disconnected by user
		if ( TRUE ) {
			DWORD dwConnectState = 0;
			DWORD dwConnectStateSize = sizeof( dwConnectState );
			if ( InternetQueryOption( pItem->hSession, INTERNET_OPTION_CONNECTED_STATE, &dwConnectState, &dwConnectStateSize ) &&
				(dwConnectState & INTERNET_STATE_DISCONNECTED_BY_USER) )
			{
				INTERNET_CONNECTED_INFO ci = { INTERNET_STATE_CONNECTED, 0 };
				verify( InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof( ci ) ) );
			}
		}

	} else {
		err = ThreadSetWin32Error( pItem, GetLastError() );	/// InternetOpen
	}

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_RemoteDisconnect
VOID ThreadDownload_RemoteDisconnect( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );

	if ( pItem->hRequest ) {
		InternetCloseHandle( pItem->hRequest );
		pItem->hRequest = NULL;
	}
	if ( pItem->hConnect ) {
		InternetCloseHandle( pItem->hConnect );
		pItem->hConnect = NULL;
	}
}


//++ ThreadDownload_RemoteConnect
BOOL ThreadDownload_RemoteConnect( _Inout_ PQUEUE_ITEM pItem, _In_ BOOL bReconnecting )
{
	BOOL bRet = FALSE;
	DWORD dwStartTime;
	ULONG i, iTimeout;
	URL_COMPONENTS uc;

	assert( pItem );
	assert( pItem->hConnect == NULL );
	assert( pItem->hRequest == NULL );

	// Grand timeout value
	if ( bReconnecting ) {
		iTimeout = (pItem->iTimeoutReconnect != DEFAULT_VALUE ? pItem->iTimeoutReconnect : 0);	/// Default: 0ms
	} else {
		iTimeout = (pItem->iTimeoutConnect != DEFAULT_VALUE ? pItem->iTimeoutConnect : 0);		/// Default: 0ms
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
		if ( InternetCrackUrl( pItem->pszURL, 0, 0, &uc ) ) {

			/// Multiple attempts to connect
			for (dwStartTime = GetTickCount(), i = 0; TRUE; i++) {

				/// Check TERM event, Check ABORT flag
				if ( ThreadIsTerminating( pItem->pThread ) || pItem->bAbort ) {
					ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );
					break;
				}

				/// Timed out?
				if ( i > 0 ) {
					if ( GetTickCount() - dwStartTime < iTimeout ) {
						/// Delay between attempts. Keep monitoring TERM event
						if ( !ThreadSleep( pItem->pThread, CONNECT_RETRY_DELAY ) ) {
							ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );
							break;	/// Canceled
						}
					} else {
						break;	/// Timeout
					}
				}

				TRACE(
					_T( "  Th:%s Id:%u %s(#%d, Elapsed:%ums/%ums, %s %s)\n" ),
					pItem->pThread->szName, pItem->iId,
					bReconnecting ? _T( "Reconnect" ) : _T( "Connect" ),
					i + 1,
					GetTickCount() - dwStartTime, iTimeout,
					pItem->szMethod, pItem->pszURL
					);

				// Connect
				pItem->hConnect = InternetConnect(
					pItem->hSession,
					uc.lpszHostName, uc.nPort,
					*uc.lpszUserName ? uc.lpszUserName : NULL,
					*uc.lpszPassword ? uc.lpszPassword : NULL,
					INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)pItem
					);

				if ( pItem->hConnect ) {

					// Check the TERM event, Check ABORT flag
					if ( !ThreadIsTerminating( pItem->pThread ) && !pItem->bAbort ) {

						// Make an HTTP request
						LPCTSTR szReqType[2] = { _T( "*/*" ), 0 };
						ULONG iObjectNameLen = uc.dwUrlPathLength + uc.dwExtraInfoLength;
						LPTSTR pszObjectName = MyAllocStr( iObjectNameLen );

						/// Set callback function
						InternetSetStatusCallback( pItem->hConnect, ThreadDownload_StatusCallback );

						assert( pszObjectName );
						if ( pszObjectName ) {

							wnsprintf( pszObjectName, iObjectNameLen + 1, _T( "%s%s" ), uc.lpszUrlPath, uc.lpszExtraInfo );

							if (pItem->iHttpInternetFlags == DEFAULT_VALUE) {
								pItem->iHttpInternetFlags =
									INTERNET_FLAG_NO_CACHE_WRITE |
									INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
									///INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
									INTERNET_FLAG_NO_COOKIES |
									INTERNET_FLAG_NO_UI |
									INTERNET_FLAG_RELOAD;
							}
							if ( uc.nScheme == INTERNET_SCHEME_HTTPS )
								pItem->iHttpInternetFlags |= INTERNET_FLAG_SECURE;

							pItem->hRequest = HttpOpenRequest(
								pItem->hConnect,
								pItem->szMethod,
								pszObjectName,
								_T( "HTTP/1.1" ),
								pItem->pszReferer,
								szReqType,
								pItem->iHttpInternetFlags,
								(DWORD_PTR)pItem		/// Context
								);

							MyFree( pszObjectName );
						}

						if ( pItem->hRequest ) {

							// Set callback function
							InternetSetStatusCallback( pItem->hRequest, ThreadDownload_StatusCallback );

							// On some Vistas (Home edition), HttpSendRequest returns ERROR_INTERNET_SEC_CERT_REV_FAILED if authenticated proxy is used
							// By default, we ignore the revocation information
							if (pItem->iHttpSecurityFlags == DEFAULT_VALUE) {
								pItem->iHttpSecurityFlags =
									SECURITY_FLAG_IGNORE_REVOCATION |
									///SECURITY_FLAG_IGNORE_UNKNOWN_CA |
									///SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
									SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
							}
							InternetSetOption( pItem->hRequest, INTERNET_OPTION_SECURITY_FLAGS, &pItem->iHttpSecurityFlags, sizeof( DWORD ) );

							// The stupid 'Work offline' setting from IE
							InternetSetOption( pItem->hRequest, INTERNET_OPTION_IGNORE_OFFLINE, 0, 0 );

							// Check TERM event, Check ABORT flag
							if ( !ThreadIsTerminating( pItem->pThread ) && !pItem->bAbort ) {

								/// Add the Range header if local content is present (resume)
								/// NOTE: If the file is already downloaded, the server will return HTTP status 416 (see below!)
								pItem->bRangeSent = FALSE;
								if ( pItem->iRecvSize > 0 ) {
									TCHAR szRangeHeader[255];
									wnsprintf( szRangeHeader, ARRAYSIZE( szRangeHeader ), _T( "Range: bytes=%I64u-" ), pItem->iRecvSize );
									pItem->bRangeSent = HttpAddRequestHeaders( pItem->hRequest, szRangeHeader, -1, HTTP_ADDREQ_FLAG_ADD_IF_NEW );
								}

								// Send the HTTP request
								_send_request:
								if ( HttpSendRequest( pItem->hRequest, pItem->pszHeaders, -1, pItem->pData, pItem->iDataSize ) ) {

									ULONG iHttpStatus;

									/// HTTP headers
									if ( TRUE ) {

										DWORD err = ERROR_SUCCESS;
										TCHAR szHeaders[512];
										ULONG iHeadersSize = sizeof(szHeaders);

										szHeaders[0] = 0;
										if (HttpQueryInfo( pItem->hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, szHeaders, &iHeadersSize, NULL )) {
											MyFree( pItem->pszSrvHeaders );
											MyStrDup( pItem->pszSrvHeaders, szHeaders );	/// Remember the headers
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
											TRACE( _T( "  Th:%s Id:%u HttpQueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF) == 0x%x, \"%s\"\n" ), pItem->pThread->szName, pItem->iId, err, szHeaders );
										}
									}

									/// HTTP status code
									iHttpStatus = ThreadSetHttpStatus( pItem );

									// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
									if ( iHttpStatus <= 299 ) {

										/// 1xx Informational
										/// 2xx Success

										// Extract the remote content length
										if (ThreadDownload_QueryContentLength64( pItem->hRequest, &pItem->iFileSize ) == ERROR_SUCCESS) {
											if (pItem->bRangeSent) {
												/// If the Range header was used, the remote content length represents the amount not yet downloaded,
												/// rather the full file size
												pItem->iFileSize += pItem->iRecvSize;
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
											if ( pItem->bRangeSent ) {
												/// Download all remote content before making another HTTP request
												if ( TRUE ) {
													CHAR szBufA[255];
													DWORD iBytesRecv;
													while ( (InternetReadFile( pItem->hRequest, szBufA, ARRAYSIZE(szBufA), &iBytesRecv )) && (iBytesRecv > 0) );
												}
												/// Retry without the Range header
												pItem->bRangeSent = FALSE;
												HttpAddRequestHeaders( pItem->hRequest, _T("Range:"), -1, HTTP_ADDREQ_FLAG_REPLACE );
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
									ThreadSetWin32Error( pItem, GetLastError() );	/// HttpSendRequest error
								}
							} else {
								ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating || bAbort
								break;
							}
						} else {
							ThreadSetWin32Error( pItem, GetLastError() );	/// HttpOpenRequest error
						}
					} else {
						ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating || bAbort
						break;
					}
				} else {
					ThreadSetWin32Error( pItem, GetLastError() );	/// InternetConnect error
				}

				TRACE(
					_T( "  Th:%s Id:%u Status (#%d, %u \"%s\")\n" ),
					pItem->pThread->szName, pItem->iId,
					i + 1,
					pItem->iWin32Error == ERROR_SUCCESS ? pItem->iHttpStatus : pItem->iWin32Error,
					pItem->iWin32Error == ERROR_SUCCESS ? pItem->pszHttpStatus : pItem->pszWin32Error
					);

			}	/// for
		} else {
			ThreadSetWin32Error( pItem, GetLastError() );	/// InternetCrackUrl error
		}
	} else {
		ThreadSetWin32Error( pItem, ERROR_OUTOFMEMORY );	/// MyAllocStr
	}

	MyFree( uc.lpszScheme );
	MyFree( uc.lpszHostName );
	MyFree( uc.lpszUserName );
	MyFree( uc.lpszPassword );
	MyFree( uc.lpszUrlPath );
	MyFree( uc.lpszExtraInfo );

	/// Cleanup
	if ( !bRet )
		ThreadDownload_RemoteDisconnect( pItem );

	return bRet;
}


//++ ThreadDownload_LocalCreate1
ULONG ThreadDownload_LocalCreate1( _Inout_ PQUEUE_ITEM pItem )
{
	///
	/// NOTE:
	/// This function is called *before* sending the HTTP request
	/// The remote content length (pItem->iFileSize) is unknown at this point
	/// We'll simply open the local file and get its size
	///
	ULONG err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hRequest == NULL );			/// Must not be connected

	/// Cleanup
	pItem->iRecvSize = 0;

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			// Exit immediately
			break;
		}

		case ITEM_LOCAL_FILE:
		{
			DWORD dwTime;
			assert( !VALID_FILE_HANDLE( pItem->Local.hFile ) );			/// File must not be opened
			assert( pItem->Local.pszFile && *pItem->Local.pszFile );	/// File name must not be empty

			// Try and open already existing file (resume)
			dwTime = GetTickCount();
		_create_file:
			pItem->Local.hFile = CreateFile( pItem->Local.pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if (pItem->Local.hFile != INVALID_HANDLE_VALUE) {
				LARGE_INTEGER iExistingSize;
				if (GetFileSizeEx( pItem->Local.hFile, &iExistingSize )) {
					/// SUCCESS
					pItem->iRecvSize = iExistingSize.QuadPart;
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
			if ((err != ERROR_SUCCESS) && VALID_FILE_HANDLE( pItem->Local.hFile )) {
				CloseHandle( pItem->Local.hFile );
				pItem->Local.hFile = NULL;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			/// Nothing to do without the remote content length
			assert( pItem->Local.pMemory == NULL );
			break;
		}
	}

	/// Handle errors
	ThreadSetWin32Error( pItem, err );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_LocalCreate2
ULONG ThreadDownload_LocalCreate2( _Inout_ PQUEUE_ITEM pItem )
{
	///
	/// NOTE:
	/// This function is called *after* sending the HTTP request
	/// At this point, the remote content length is usually known, unless the server fails to report it
	/// If it's known we'll attempt resuming the download
	/// Otherwise, we'll start over
	///
	ULONG err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hRequest != NULL );			/// Must be connected

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			// Exit immediately
			break;
		}

		case ITEM_LOCAL_FILE:
		{
			assert( VALID_FILE_HANDLE( pItem->Local.hFile ));		/// The file must already be opened

			// Determine if resuming is possible
			if ( pItem->iFileSize != INVALID_FILE_SIZE64 ) {
				if ( !pItem->bRangeSent || (pItem->iHttpStatus == HTTP_STATUS_PARTIAL_CONTENT) ) {	/// Server supports the Range header
					if ( pItem->iRecvSize <= pItem->iFileSize ) {
						ULONG iZero = 0;
						if ( (SetFilePointer( pItem->Local.hFile, 0, &iZero, FILE_END ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS) ) {
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
				if ( (SetFilePointer( pItem->Local.hFile, 0, &iZero, FILE_BEGIN ) != INVALID_SET_FILE_POINTER ) || (GetLastError() == ERROR_SUCCESS)) {
					if ( SetEndOfFile( pItem->Local.hFile ) ) {
						/// SUCCESS (full download)
						pItem->iRecvSize = 0;
						err = ERROR_SUCCESS;
					} else {
						err = GetLastError();	/// SetEndOfFile
					}
				} else {
					err = GetLastError();	/// SetFilePointer
				}
			}

			/// Handle errors
			if ( (err != ERROR_SUCCESS) && VALID_FILE_HANDLE(pItem->Local.hFile) ) {
				CloseHandle( pItem->Local.hFile );
				pItem->Local.hFile = NULL;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			/// NOTE: If we're reconnecting, the memory buffer is already be allocated. We'll resume the transfer...
			if ( pItem->iFileSize != INVALID_FILE_SIZE64 ) {
				if ( pItem->iFileSize <= MAX_MEMORY_CONTENT_LENGTH ) {		// Size limit
					if (!pItem->Local.pMemory) {
						pItem->Local.pMemory = (LPBYTE)MyAlloc( (SIZE_T)pItem->iFileSize );
						if (pItem->Local.pMemory) {
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
	ThreadSetWin32Error( pItem, err );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_LocalClose
BOOL ThreadDownload_LocalClose( _Inout_ PQUEUE_ITEM pItem )
{
	BOOL bRet = TRUE;

	assert( pItem );

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			break;
		}

		case ITEM_LOCAL_FILE:
		{
			if ( VALID_FILE_HANDLE(pItem->Local.hFile) ) {
				bRet = CloseHandle( pItem->Local.hFile );
				pItem->Local.hFile = NULL;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			///MyFree( pItem->Local.pMemory );	// Memory content must remain available after the transfer has completed
			break;
		}
	}

	return bRet;
}


//++ ThreadDownload_RefreshSpeed
void ThreadDownload_RefreshSpeed( _Inout_ PQUEUE_ITEM pItem, _In_ BOOL bXferFinished )
{
	BOOL bFormatString = FALSE;
	assert( pItem );

	// Compute speed
	if (bXferFinished) {

		ULONG iTimeDiff = MyTimeDiff( &pItem->Xfer.tmEnd, &pItem->Xfer.tmStart );		/// Milliseconds
		if (iTimeDiff >= SPEED_MEASURE_INTERVAL) {
			pItem->Speed.iSpeed = (ULONG)MyDoubleToUlonglong( MyUlonglongToDouble( pItem->Xfer.iXferSize ) / (MyUlonglongToDouble( iTimeDiff ) / 1000.0F) + 0.5F );
		} else {
			pItem->Speed.iSpeed = (ULONG)pItem->Xfer.iXferSize;
		}
		bFormatString = TRUE;

		pItem->Speed.iChunkTime = 0;
		pItem->Speed.iChunkSize = 0;

	} else {

		ULONG iTimeDiff = GetTickCount() - pItem->Speed.iChunkTime;
		if (iTimeDiff >= SPEED_MEASURE_INTERVAL) {

			pItem->Speed.iSpeed = (ULONG)MyDoubleToUlonglong( MyUlonglongToDouble( pItem->Speed.iChunkSize ) / (MyUlonglongToDouble( iTimeDiff ) / 1000.0F) + 0.5F );
			bFormatString = TRUE;

			pItem->Speed.iChunkSize = 0;
			pItem->Speed.iChunkTime = GetTickCount();
		}
	}

	// Format as text
	if (bFormatString) {
#ifdef UNICODE
		StrFormatByteSizeW( (LONGLONG)pItem->Speed.iSpeed, pItem->Speed.szSpeed, ARRAYSIZE( pItem->Speed.szSpeed ) );
#else
		StrFormatByteSizeA( pItem->Speed.iSpeed, pItem->Speed.szSpeed, ARRAYSIZE( pItem->Speed.szSpeed ) );
#endif
		lstrcat( pItem->Speed.szSpeed, _T( "/s" ) );
	}
}


//++ ThreadDownload_Transfer
BOOL ThreadDownload_Transfer( _Inout_ PQUEUE_ITEM pItem )
{
	DWORD err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hRequest != NULL );

	/// Initializations
	GetLocalFileTime( &pItem->Xfer.tmStart );
	pItem->Xfer.tmEnd.dwLowDateTime = 0;
	pItem->Xfer.tmEnd.dwHighDateTime = 0;
	pItem->Xfer.iXferSize = 0;

	pItem->Speed.iSpeed = 0;
	lstrcpy( pItem->Speed.szSpeed, _T( "..." ) );
	pItem->Speed.iChunkTime = GetTickCount();
	pItem->Speed.iChunkSize = 0;

	/// Anything to download?
	if ( pItem->iRecvSize >= pItem->iFileSize )
		return TRUE;

	// Debugging definitions
///#define DEBUG_XFER_MAX_BYTES		1024*1024
///#define DEBUG_XFER_SLOWDOWN		500
///#define DEBUG_XFER_PROGRESS

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			/// Don't transfer anything
			break;
		}

		case ITEM_LOCAL_FILE:
		{
			/// Allocate a transfer buffer
			CONST ULONG iBufSize = 1024 * TRANSFER_CHUNK_SIZE;
			LPBYTE pBuf = MyAlloc( iBufSize );
			if ( pBuf ) {

				/// Transfer loop
				ULONG iBytesRecv, iBytesWritten;
				while ( err == ERROR_SUCCESS && (pItem->iRecvSize < pItem->iFileSize)) {
#ifdef DEBUG_XFER_MAX_BYTES
					/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
					if ( pItem->Xfer.iXferSize >= DEBUG_XFER_MAX_BYTES ) {
						err = ThreadSetWin32Error( pItem, ERROR_INTERNET_CONNECTION_RESET );
						break;
					}
#endif ///DEBUG_XFER_MAX_BYTES
					if ( !ThreadIsTerminating( pItem->pThread ) && !pItem->bAbort ) {
						if ( InternetReadFile( pItem->hRequest, pBuf, iBufSize, &iBytesRecv ) ) {
							if ( iBytesRecv > 0 ) {
								if ( WriteFile( pItem->Local.hFile, pBuf, iBytesRecv, &iBytesWritten, NULL ) ) {
									/// Update fields
									pItem->iRecvSize += iBytesRecv;
									pItem->Xfer.iXferSize += iBytesRecv;
									pItem->Speed.iChunkSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
									/// Simulate transfer slow download
									Sleep( DEBUG_XFER_SLOWDOWN );
#endif ///DEBUG_XFER_SLOWDOWN
									/// Speed measurement
									ThreadDownload_RefreshSpeed( pItem, FALSE );
#ifdef DEBUG_XFER_PROGRESS
									/// Display transfer progress
									TRACE(
										_T( "  Th:%s Id:%u ThreadTransfer(Recv:%d%% %I64u/%I64u @ %s, %s %s -> File)\n" ),
										pItem->pThread->szName, pItem->iId,
										ItemGetRecvPercent( pItem ),
										pItem->iRecvSize, pItem->iFileSize == INVALID_FILE_SIZE64 ? 0 : pItem->iFileSize,
										pItem->Speed.szSpeed,
										pItem->szMethod, pItem->pszURL
										);
#endif ///DEBUG_XFER_PROGRESS
								} else {
									err = ThreadSetWin32Error( pItem, GetLastError() );	/// WriteFile
								}
							} else {
								// Transfer complete
								ThreadSetWin32Error( pItem, ERROR_SUCCESS );
								ThreadSetHttpStatus( pItem );
								break;
							}
						} else {
							err = ThreadSetWin32Error( pItem, GetLastError() );	/// InternetReadFile
						}
					} else {
						err = ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );
					}
				}	///while

				MyFree( pBuf );

			} else {
				err = ERROR_OUTOFMEMORY;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			/// Transfer loop
			ULONG iBytesRecv;
			while ( err == ERROR_SUCCESS && (pItem->iRecvSize < pItem->iFileSize)) {
#ifdef DEBUG_XFER_MAX_BYTES
				/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
				if ( pItem->Xfer.iXferSize >= DEBUG_XFER_MAX_BYTES ) {
					err = ThreadSetWin32Error( pItem, ERROR_INTERNET_CONNECTION_RESET );
					break;
				}
#endif ///DEBUG_XFER_MAX_BYTES
				if ( !ThreadIsTerminating( pItem->pThread ) && !pItem->bAbort ) {
					if ( InternetReadFile( pItem->hRequest, pItem->Local.pMemory + pItem->iRecvSize, 1024 * TRANSFER_CHUNK_SIZE, &iBytesRecv ) ) {
						if ( iBytesRecv > 0 ) {
							/// Update fields
							pItem->iRecvSize += iBytesRecv;
							pItem->Xfer.iXferSize += iBytesRecv;
							pItem->Speed.iChunkSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
							/// Simulate transfer slow download
							Sleep( DEBUG_XFER_SLOWDOWN );	/// Emulate slow download
#endif ///DEBUG_XFER_SLOWDOWN
							/// Speed measurement
							ThreadDownload_RefreshSpeed( pItem, FALSE );
#ifdef DEBUG_XFER_PROGRESS
							/// Display transfer progress
							TRACE(
								_T( "  Th:%s Id:%u ThreadTransfer(Recv:%d%% %I64u/%I64u @ %s, %s %s -> Memory)\n" ),
								pItem->pThread->szName, pItem->iId,
								ItemGetRecvPercent( pItem ),
								pItem->iRecvSize, pItem->iFileSize == INVALID_FILE_SIZE64 ? 0 : pItem->iFileSize,
								pItem->Speed.szSpeed,
								pItem->szMethod, pItem->pszURL
								);
#endif ///DEBUG_XFER_PROGRESS
						} else {
							// Transfer complete
							ThreadSetWin32Error( pItem, ERROR_SUCCESS );
							ThreadSetHttpStatus( pItem );
							break;
						}
					} else {
						err = ThreadSetWin32Error( pItem, GetLastError() );	/// InternetReadFile
					}
				} else {
					err = ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );
				}
			}
			break;
		}
	}

	/// Finalize
	if (pItem->iFileSize == INVALID_FILE_SIZE64)
		pItem->iFileSize = pItem->iRecvSize;		/// Set the (previously unknown) file size
	GetLocalFileTime( &pItem->Xfer.tmEnd );
	ThreadDownload_RefreshSpeed( pItem, TRUE );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload
VOID ThreadDownload( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );
	TRACE(
		_T( "  Th:%s Id:%u ThreadDownload(%s %s -> %s)\n" ),
		pItem->pThread->szName, pItem->iId,
		pItem->szMethod, pItem->pszURL,
		pItem->iLocalType == ITEM_LOCAL_NONE ? TEXT_LOCAL_NONE : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : TEXT_LOCAL_MEMORY )
		);

	if ( pItem->pszURL && *pItem->pszURL ) {

		if ( ThreadDownload_LocalCreate1( pItem ) ) {
			if ( ThreadDownload_OpenSession( pItem ) ) {

				BOOL bReconnectAllowed = TRUE;
				int i;
				for (i = 0; (i < 1000) && bReconnectAllowed; i++) {

					bReconnectAllowed = FALSE;
					if ( ThreadDownload_RemoteConnect( pItem, (BOOL)(i > 0) ) ) {
						if ( ThreadDownload_LocalCreate2( pItem ) ) {

							if ( ThreadDownload_Transfer( pItem ) ) {
								// Success
							}

							ThreadDownload_RemoteDisconnect( pItem );

							/// Reconnect and resume?
							bReconnectAllowed =
								pItem->iWin32Error != ERROR_SUCCESS &&
								pItem->iWin32Error != ERROR_INTERNET_OPERATION_CANCELLED &&
								pItem->Xfer.iXferSize > 0 &&			/// We already received something...
								ItemIsReconnectAllowed( pItem );
						}
					}
				}
				ThreadDownload_CloseSession( pItem );
			}
			ThreadDownload_LocalClose( pItem );
		}
	}

	TRACE(
		_T( "  Th:%s Id:%u ThreadDownload(Recv:%d%% %I64u @ %s, %s %s [%s] -> %s)\n" ),
		pItem->pThread->szName, pItem->iId,
		ItemGetRecvPercent( pItem ), pItem->iFileSize, pItem->Speed.szSpeed,
		pItem->szMethod, pItem->pszURL, pItem->pszSrvIP,
		pItem->iLocalType == ITEM_LOCAL_NONE ? TEXT_LOCAL_NONE : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : TEXT_LOCAL_MEMORY )
		);
}


//++ ThreadSetWin32Error
ULONG ThreadSetWin32Error( _Inout_ PQUEUE_ITEM pItem, _In_ ULONG iError )
{
	assert( pItem );
	if ( pItem->iWin32Error != iError ) {
		/// Numeric error code
		pItem->iWin32Error = iError;
		/// Error string
		MyFree( pItem->pszWin32Error );
		AllocErrorStr( pItem->iWin32Error, &pItem->pszWin32Error );
	}

	/// Handle ERROR_INTERNET_EXTENDED_ERROR
	if ( pItem->iWin32Error == ERROR_INTERNET_EXTENDED_ERROR ) {
		DWORD dwWebError;
		TCHAR szWebError[512];
		DWORD dwWebErrorLen = ARRAYSIZE( szWebError );
		szWebError[0] = 0;
		if ( InternetGetLastResponseInfo( &dwWebError, szWebError, &dwWebErrorLen ) ) {
			MyFree( pItem->pszWin32Error );
			MyStrDup( pItem->pszWin32Error, szWebError );
		} else {
			MyFree( pItem->pszWin32Error );
			AllocErrorStr( pItem->iWin32Error, &pItem->pszWin32Error );
		}
	}
	return iError;
}


//++ ThreadSetHttpStatus
ULONG ThreadSetHttpStatus( _Inout_ PQUEUE_ITEM pItem )
{
	ULONG iHttpStatus = 0;

	assert( pItem );
	assert( pItem->hRequest );

	if ( pItem && pItem->hRequest ) {

		TCHAR szErrorText[512];
		ULONG iDataSize;

		/// Get HTTP status (numeric)
		iDataSize = sizeof( iHttpStatus );
		if ( HttpQueryInfo( pItem->hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &iHttpStatus, &iDataSize, NULL ) ) {
			if ( pItem->iHttpStatus != iHttpStatus ) {

				pItem->iHttpStatus = iHttpStatus;

				/// Get HTTP status (string)
				MyFree( pItem->pszHttpStatus );
				szErrorText[0] = 0;
				iDataSize = sizeof( szErrorText );
				HttpQueryInfo( pItem->hRequest, HTTP_QUERY_STATUS_TEXT, szErrorText, &iDataSize, NULL );
				if ( *szErrorText )
					MyStrDup( pItem->pszHttpStatus, szErrorText );
			}
		}
	}

	return iHttpStatus;
}
