#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define NSDOWN_USERAGENT			_T("Mozilla/5.0 (NSdown)")
#define INVALID_FILE_SIZE64			(ULONG64)-1
#define TRANSFER_CHUNK_SIZE			256			/// 256 KiB
#define MAX_MEMORY_CONTENT_LENGTH	104857600	/// 100 MiB
#define CONNECT_RETRY_DELAY			1000		/// milliseconds


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
			GetLocalFileTime( &pItem->tmDownloadStart );
			pItem->iStatus = ITEM_STATUS_DOWNLOADING;
			TRACE( _T( "  Th:%s dequeued item ID:%u, %s\n" ), pThread->szName, pItem->iId, pItem->pszURL );
		} else {
			TRACE( _T( "  Th:%s has nothing to do, going to sleep\n" ), pThread->szName );
		}
		QueueUnlock( (PQUEUE)pThread->pQueue );

		// Start downloading
		if ( pItem ) {

			ThreadDownload( pItem );
			GetLocalFileTime( &pItem->tmDownloadEnd );
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

	TRACE( _T( "  Th:%s stopped\n" ), pThread->szName );
	return 0;
}


//++ ThreadTraceHttpInfo
#if DBG || _DEBUG

#if UNICODE
#define ThreadTraceHttpInfo( pItem, iHttpInfo ) \
	ThreadTraceHttpInfoImpl( pItem, iHttpInfo, L#iHttpInfo )
#else
#define ThreadTraceHttpInfo( pItem, iHttpInfo ) \
	ThreadTraceHttpInfoImpl( pItem, iHttpInfo, #iHttpInfo )
#endif

VOID ThreadTraceHttpInfoImpl( _In_ PQUEUE_ITEM pItem, _In_ UINT iHttpInfo, _In_ LPCTSTR szHttpInfo )
{
	TCHAR szTemp[512];
	ULONG iTempSize = sizeof(szTemp);
	ULONG iTempErr = 0;

	szTemp[0] = 0;
	iTempErr = HttpQueryInfo( pItem->hRequest, iHttpInfo, szTemp, &iTempSize, NULL ) ? ERROR_SUCCESS : GetLastError();
	if ( iTempErr != ERROR_HTTP_HEADER_NOT_FOUND ) {
		if ( iTempErr == ERROR_SUCCESS ) {
			LPTSTR psz;
			for ( psz = szTemp; *psz; psz++ )
				if ( *psz == _T( '\r' ) || *psz == _T( '\n' ) )
					*psz = _T( '|' );
			TRACE( _T( "  Th:%s HttpQueryInfo( %s ) == \"%s\" [%u bytes]\n" ), pItem->pThread->szName, szHttpInfo, szTemp, iTempSize );
		} else {
			LPTSTR pszTemp = NULL;
			AllocErrorStr( iTempErr, &pszTemp );
			TRACE( _T( "  Th:%s HttpQueryInfo( %s ) == %u \"%s\"\n" ), pItem->pThread->szName, szHttpInfo, iTempErr, pszTemp );
			MyFree( pszTemp );
		}
	}
}
#else
	#define ThreadTraceHttpInfo(...)
#endif ///DBG || _DEBUG


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
	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		/// Too noisy...
		///TRACE2( _T("  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_RECEIVING_RESPONSE[%u] )\n"), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		/// Too noisy...
		///TRACE2( _T("  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_RESPONSE_RECEIVED[%u] )\n"), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CLOSING_CONNECTION:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_CLOSING_CONNECTION[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_CONNECTED_TO_SERVER[%u] \"%hs\" )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_CONNECTING_TO_SERVER[%u] \"%hs\" )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_CONNECTION_CLOSED:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_CONNECTION_CLOSED[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_HANDLE_CLOSING:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_HANDLE_CLOSING[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
	{
		HINTERNET hNewHandle = *((HINTERNET*)lpvStatusInformation);
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_HANDLE_CREATED[%u], Handle:0x%p )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, hNewHandle );
		break;
	}
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_INTERMEDIATE_RESPONSE[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_NAME_RESOLVED:
	{
		PCSTR pszAddrA = (PCSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_NAME_RESOLVED[%u] \"%hs\" )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, pszAddrA );
		break;
	}
	case INTERNET_STATUS_REDIRECT:
	{
		PCTSTR pszNewURL = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_REDIRECT[%u] \"%s\" )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, pszNewURL );
		break;
	}
	case INTERNET_STATUS_REQUEST_COMPLETE:
	{
		LPINTERNET_ASYNC_RESULT pResult = (LPINTERNET_ASYNC_RESULT)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_REQUEST_COMPLETE[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	}
	case INTERNET_STATUS_REQUEST_SENT:
	{
		DWORD dwBytesSent = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_REQUEST_SENT[%u] BytesSent:%u )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, dwBytesSent );
		break;
	}
	case INTERNET_STATUS_RESOLVING_NAME:
	{
		PCTSTR pszName = (PCTSTR)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_RESOLVING_NAME[%u] \"%s\" )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, pszName );
		break;
	}
	case INTERNET_STATUS_SENDING_REQUEST:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_SENDING_REQUEST[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_STATE_CHANGE:
	{
		DWORD dwFlags = *((LPDWORD)lpvStatusInformation);
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_STATE_CHANGE[%u] Flags:0x%08x )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, dwFlags );
		break;
	}
	case INTERNET_STATUS_DETECTING_PROXY:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_DETECTING_PROXY[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_CTL_RESPONSE_RECEIVED[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_PREFETCH:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_PREFETCH[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_USER_INPUT_REQUIRED:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_USER_INPUT_REQUIRED[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_COOKIE_SENT:
	{
		DWORD dwSentCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_COOKIE_SENT[%u] CookiesSent:%u)\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, dwSentCookies );
		break;
	}
	case INTERNET_STATUS_COOKIE_RECEIVED:
	{
		DWORD dwRecvCookies = (DWORD)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_COOKIE_RECEIVED[%u] CookiesRecv:%u )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus, dwRecvCookies );
		break;
	}
	case INTERNET_STATUS_PRIVACY_IMPACTED:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_PRIVACY_IMPACTED[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_HEADER:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_P3P_HEADER[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_P3P_POLICYREF:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_P3P_POLICYREF[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	case INTERNET_STATUS_COOKIE_HISTORY:
	{
		InternetCookieHistory *pHistory = (InternetCookieHistory*)lpvStatusInformation;
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_COOKIE_HISTORY[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	}
	default:
		TRACE2( _T( "  Th:%s StatusCallback( 0x%p, INTERNET_STATUS_UNKNOWN[%u] )\n" ), pItem->pThread->szName, hRequest, dwInternetStatus );
		break;
	}
#endif ///DBG || _DEBUG
}


//++ ThreadDownload_OpenSession
BOOL ThreadDownload_OpenSession(_Inout_ PQUEUE_ITEM pItem)
{
	DWORD err = ERROR_SUCCESS;
	assert( pItem );
	assert( pItem->hSession == NULL );

	pItem->hSession = InternetOpen( NSDOWN_USERAGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if ( pItem->hSession ) {

		// Set callback function
		InternetSetStatusCallback(pItem->hSession, ThreadDownload_StatusCallback);

		/// Options
		if ( pItem->iOptConnectRetries != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_RETRIES, &pItem->iOptConnectRetries, sizeof( pItem->iOptConnectRetries ) );
		if ( pItem->iOptConnectTimeout != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &pItem->iOptConnectTimeout, sizeof( pItem->iOptConnectTimeout ) );
		if ( pItem->iOptReceiveTimeout != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &pItem->iOptReceiveTimeout, sizeof( pItem->iOptReceiveTimeout ) );

		/// Reconnect if disconnected by user
		if ( TRUE ) {
			DWORD dwConnectState = 0;
			DWORD dwConnectStateSize = sizeof( dwConnectState );
			if ( InternetQueryOption( pItem->hSession, INTERNET_OPTION_CONNECTED_STATE, &dwConnectState, &dwConnectStateSize ) &&
				(dwConnectState & INTERNET_STATE_DISCONNECTED_BY_USER) )
			{
				INTERNET_CONNECTED_INFO ci = { INTERNET_STATE_CONNECTED, 0 };
				InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof( ci ) );
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
	ULONG iFlagsHttpOpen, iFlagsHttpSend;
	ULONG iHttpStatus;
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

	// HttpOpenRequest flags
	iFlagsHttpOpen =
		INTERNET_FLAG_NO_CACHE_WRITE |
		INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
	///	INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
		INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI |
		INTERNET_FLAG_RELOAD;

	// HttpSendRequest flags
	iFlagsHttpSend =
		SECURITY_FLAG_IGNORE_REVOCATION |
	///	SECURITY_FLAG_IGNORE_UNKNOWN_CA |
	///	SECURITY_FLAG_IGNORE_CERT_CN_INVALID
		SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

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
			for ( dwStartTime = GetTickCount(), i = 0; TRUE; i++ ) {

				/// Check TERM event
				if ( ThreadIsTerminating( pItem->pThread ) ) {
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
					_T( "  Th:%s %s( #%d, Elapsed:%ums/%ums, %s %s -> %s )\n" ),
					pItem->pThread->szName,
					bReconnecting ? _T( "Reconnect" ) : _T( "Connect" ),
					i + 1,
					GetTickCount() - dwStartTime, iTimeout,
					pItem->szMethod,
					pItem->pszURL,
					pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
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

					// Check again the TERM event
					if ( !ThreadIsTerminating( pItem->pThread ) ) {

						// Make an HTTP request
						LPCTSTR szReqType[2] = { _T( "*/*" ), 0 };
						ULONG iObjectNameLen = uc.dwUrlPathLength + uc.dwExtraInfoLength;
						LPTSTR pszObjectName = MyAllocStr( iObjectNameLen );

						/// Set callback function
						InternetSetStatusCallback( pItem->hConnect, ThreadDownload_StatusCallback );

						assert( pszObjectName );
						if ( pszObjectName ) {

							wnsprintf( pszObjectName, iObjectNameLen + 1, _T( "%s%s" ), uc.lpszUrlPath, uc.lpszExtraInfo );
							if ( uc.nScheme == INTERNET_SCHEME_HTTPS )
								iFlagsHttpOpen |= INTERNET_FLAG_SECURE;

							pItem->hRequest = HttpOpenRequest(
								pItem->hConnect,
								pItem->szMethod,
								pszObjectName,
								_T( "HTTP/1.1" ),
								NULL,					/// Referer
								szReqType,
								iFlagsHttpOpen,
								(DWORD_PTR)pItem		/// Context
								);

							MyFree( pszObjectName );
						}

						if ( pItem->hRequest ) {

							// Set callback function
							InternetSetStatusCallback( pItem->hRequest, ThreadDownload_StatusCallback );

							// On some Vistas (e.g. Home), HttpSendRequest returns ERROR_INTERNET_SEC_CERT_REV_FAILED if authenticated proxy is used
							// We've decided to ignore the revocation status.
							InternetSetOption( pItem->hRequest, INTERNET_OPTION_SECURITY_FLAGS, &iFlagsHttpSend, sizeof( DWORD ) );

							// The stupid 'Work offline' setting from IE
							InternetSetOption( pItem->hRequest, INTERNET_OPTION_IGNORE_OFFLINE, 0, 0 );

							// Check TERM event
							if ( !ThreadIsTerminating( pItem->pThread ) ) {

								// Send the HTTP request
								if ( HttpSendRequest( pItem->hRequest, NULL, 0, NULL, 0 ) ) {

									/// Check the HTTP status code
									iHttpStatus = ThreadSetHttpStatus( pItem );

									// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
									if ( iHttpStatus <= 299 ) {

										/// 1xx Informational
										/// 2xx Success

										//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_RAW_HEADERS_CRLF );

										/// Success. Break the loop
										bRet = TRUE;
										break;

									} else {

										/// 3xx Redirection - The client must take additional action to complete the request
										/// 4xx Client Error
										/// 5xx Server Error

										if ( iHttpStatus != HTTP_STATUS_SERVICE_UNAVAIL &&		/// 503 Service Unavailable
											iHttpStatus != HTTP_STATUS_GATEWAY_TIMEOUT &&		/// 504 Gateway Timeout
											iHttpStatus != 598 &&								/// 598 Network read timeout error (Unknown)
											iHttpStatus != 599 )								/// 599 Network connect timeout error (Unknown)
										{
											/// Error. Break the loop
											ThreadDownload_RemoteDisconnect( pItem );
											break;
										}
									}

								} else {
									ThreadSetWin32Error( pItem, GetLastError() );	/// HttpSendRequest error
								}
							} else {
								ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating
								break;
							}
						} else {
							ThreadSetWin32Error( pItem, GetLastError() );	/// HttpOpenRequest error
						}
					} else {
						ThreadSetWin32Error( pItem, ERROR_INTERNET_OPERATION_CANCELLED );	/// ThreadIsTerminating
						break;
					}
				} else {
					ThreadSetWin32Error( pItem, GetLastError() );	/// InternetConnect error
				}
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
			err = ERROR_INVALID_DATA;
			*piContentLength = INVALID_FILE_SIZE64;
		}
	} else {
		err = GetLastError();
		*piContentLength = INVALID_FILE_SIZE64;
	}

	return err;
}


//++ ThreadDownload_SetRemotePosition
ULONG ThreadDownload_SetRemotePosition( _Inout_ PQUEUE_ITEM pItem, _In_ PLARGE_INTEGER piFilePos )
{
	ULONG err = ERROR_SUCCESS;
	TCHAR szHeader[128];

	assert( pItem );
	assert( pItem->hRequest != NULL );
	assert( pItem->iFileSize != INVALID_FILE_SIZE64 );
	assert( piFilePos );

	/// By design, this function is called when the remote file position is zero
	if ((piFilePos->QuadPart > 0) &&							/// The new position must be different than the old one
		((ULONG64)piFilePos->QuadPart < pItem->iFileSize) )		/// Don't set the postition past the EOF
	{

		if ( pItem->iFileSize != INVALID_FILE_SIZE64 ) {

			/// Method 1: Use InternetSetFilePointer
			/// Doesn't work with POST method
			/// Doesn't work if the cache is disabled
			if ( InternetSetFilePointer( pItem->hRequest, piFilePos->LowPart, &piFilePos->HighPart, FILE_BEGIN, 0 ) == INVALID_SET_FILE_POINTER )
				err = GetLastError();

			/// Method 2: Use the "Range" HTTP header
			/// http://www.clevercomponents.com/articles/article015/resuming.asp
			if ( err != ERROR_SUCCESS ) {

				/// Make sure the server doesn't explicitly reject ranges
				TCHAR szAcceptRanges[128];
				ULONG iAcceptRangesSize = sizeof( szAcceptRanges );
				if ( !HttpQueryInfo( pItem->hRequest, HTTP_QUERY_ACCEPT_RANGES, szAcceptRanges, &iAcceptRangesSize, NULL ) )
					szAcceptRanges[0] = _T( '\0' );
				if ( lstrcmpi( szAcceptRanges, _T( "none" ) ) != 0 ) {

					wnsprintf( szHeader, ARRAYSIZE( szHeader ), _T( "Range: bytes=%I64u-" ), piFilePos->QuadPart );
					if ( HttpAddRequestHeaders( pItem->hRequest, szHeader, -1, HTTP_ADDREQ_FLAG_ADD_IF_NEW ) ) {
						if ( HttpSendRequest( pItem->hRequest, NULL, 0, NULL, 0 ) ) {
							/// We'll check later if the server truly supports the Range header
							pItem->bResumeNeedsValidation = TRUE;
							/// Success
							err = ERROR_SUCCESS;
						} else {
							err = GetLastError();
						}
					} else {
						err = GetLastError();
					}
				} else {
					err = ERROR_INTERNET_INVALID_OPERATION;		/// The server explicitly rejects resuming
				}
			}

		} else {
			err = ERROR_INTERNET_INCORRECT_FORMAT;
		}
	}

	return err;
}


//++ ThreadDownload_LocalCreate
ULONG ThreadDownload_LocalCreate( _Inout_ PQUEUE_ITEM pItem )
{
	ULONG err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hRequest != NULL );

	/// Cleanup
	pItem->iFileSize = INVALID_FILE_SIZE64;
	pItem->iRecvSize = 0;

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			// Exit immediately
			break;
		}

		case ITEM_LOCAL_FILE:
		{
			assert( pItem->Local.hFile == NULL || pItem->Local.hFile == INVALID_HANDLE_VALUE );
			assert( pItem->Local.pszFile && *pItem->Local.pszFile );

			// Query the remote content length
			ThreadDownload_QueryContentLength64( pItem->hRequest, &pItem->iFileSize );

			// Create/Append local file
			if ( pItem->iFileSize != INVALID_FILE_SIZE64 ) {

				pItem->Local.hFile = CreateFile( pItem->Local.pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( pItem->Local.hFile != INVALID_HANDLE_VALUE ) {
					/// TODO: Retry if ERROR_LOCK_VIOLATION
					LARGE_INTEGER iExistingSize;
					if ( GetFileSizeEx( pItem->Local.hFile, &iExistingSize ) ) {
						if ( (ULONG64)iExistingSize.QuadPart <= pItem->iFileSize ) {
							if ( (SetFilePointer( pItem->Local.hFile, 0, NULL, FILE_END ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS) ) {
								if ( ThreadDownload_SetRemotePosition( pItem, &iExistingSize ) == ERROR_SUCCESS ) {
									/// SUCCESS (resume)
									pItem->iRecvSize = iExistingSize.QuadPart;
								} else {
									// Full download (the remote server probably doesn't support resuming)
									SetFilePointer( pItem->Local.hFile, 0, NULL, FILE_BEGIN );
									if ( SetEndOfFile( pItem->Local.hFile ) ) {
										/// SUCCESS
									} else {
										err = GetLastError();	/// SetEndOfFile
									}
								}
							} else {
								err = GetLastError();	/// SetFilePointer
							}
						} else {
							// Full download (the existing file is larger than the remote file)
							if ( (SetFilePointer( pItem->Local.hFile, 0, NULL, FILE_BEGIN ) != INVALID_SET_FILE_POINTER ) || (GetLastError() == ERROR_SUCCESS)) {
								if ( SetEndOfFile( pItem->Local.hFile ) ) {
									/// SUCCESS
								} else {
									err = GetLastError();	/// SetEndOfFile
								}
							} else {
								err = GetLastError();	/// SetFilePointer
							}
						}
					} else {
						err = GetLastError();	/// GetFileSizeEx
					}
				} else {
					err = GetLastError();	/// CreateFile
				}

			} else {
				// Full download (remote file size is not available)
				pItem->Local.hFile = CreateFile( pItem->Local.pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( pItem->Local.hFile != INVALID_HANDLE_VALUE ) {
					/// SUCCESS
				} else {
					err = GetLastError();	/// CreateFile
				}
			}

			/// Handle errors
			if ( (err != ERROR_SUCCESS) && (pItem->Local.hFile != NULL) && (pItem->Local.hFile != INVALID_HANDLE_VALUE) ) {
				CloseHandle( pItem->Local.hFile );
				pItem->Local.hFile = NULL;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			assert( pItem->Local.pMemory == NULL );

			// Query the remote content length
			err = ThreadDownload_QueryContentLength64( pItem->hRequest, &pItem->iFileSize );
			if ( err == ERROR_SUCCESS ) {

				// Size limit
				if ( pItem->iFileSize <= MAX_MEMORY_CONTENT_LENGTH ) {

					// Allocate memory
					pItem->Local.pMemory = (LPBYTE)MyAlloc( (SIZE_T)pItem->iFileSize );
					if ( pItem->Local.pMemory ) {
						/// SUCCESS
					} else {
						err = ERROR_OUTOFMEMORY;
					}

				} else {
					err = ERROR_FILE_TOO_LARGE;
					pItem->Local.pMemory = NULL;
				}
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
			if ( (pItem->Local.hFile != NULL) && (pItem->Local.hFile != INVALID_HANDLE_VALUE) ) {
				bRet = CloseHandle( pItem->Local.hFile );
				pItem->Local.hFile = NULL;
			}
			break;
		}

		case ITEM_LOCAL_MEMORY:
		{
			if ( pItem->Local.pMemory != NULL ) {
				MyFree( pItem->Local.pMemory );
			}
			break;
		}
	}

	return bRet;
}


//++ ThreadDownload_Transfer
BOOL ThreadDownload_Transfer( _Inout_ PQUEUE_ITEM pItem, _Out_opt_ PULONG64 piRecvBytes )
{
	DWORD err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hRequest != NULL );

	// Debugging definitions
///#define DEBUG_XFER_MAX_BYTES		1024*1024
///#define DEBUG_XFER_SLOWDOWN		1000
///#define DEBUG_XFER_PROGRESS

	switch ( pItem->iLocalType ) {

		case ITEM_LOCAL_NONE:
		{
			/// Exit immediately
			return TRUE;
		}

		case ITEM_LOCAL_FILE:
		{
			/// Allocate a transfer buffer
			CONST ULONG iBufSize = 1024 * TRANSFER_CHUNK_SIZE;
			LPBYTE pBuf = MyAlloc( iBufSize );
			if ( pBuf ) {

				/// Transfer loop
				ULONG iBytesRecv, iBytesWritten;
				if ( piRecvBytes )
					*piRecvBytes = 0;
				while ( err == ERROR_SUCCESS && (pItem->iRecvSize < pItem->iFileSize)) {
#ifdef DEBUG_XFER_MAX_BYTES
					/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
					if ( piRecvBytes && (*piRecvBytes >= DEBUG_XFER_MAX_BYTES) ) {
						err = ThreadSetWin32Error( pItem, ERROR_INTERNET_CONNECTION_RESET );
						break;
					}
#endif ///DEBUG_XFER_MAX_BYTES
					if ( !ThreadIsTerminating( pItem->pThread )) {
						if ( InternetReadFile( pItem->hRequest, pBuf, iBufSize, &iBytesRecv ) ) {
							if ( pItem->bResumeNeedsValidation ) {
								pItem->bResumeNeedsValidation = FALSE;		/// Reset the flag. We're doing this check only once
								ThreadSetHttpStatus( pItem );				/// Retrieve the HTTP status
								if ( pItem->iHttpStatus == HTTP_STATUS_OK ) {
									/// We're resuming the download using the Range header, but the server doesn't support it
									/// The server should have returned HTTP_STATUS_PARTIAL_CONTENT[216] HTTP status...
									/// Abort everything
									ThreadSetWin32Error( pItem, ERROR_HTTP_INVALID_SERVER_RESPONSE );
									break;
								}
							}
							if ( piRecvBytes )
								*piRecvBytes += iBytesRecv;
							if ( iBytesRecv > 0 ) {
								if ( WriteFile( pItem->Local.hFile, pBuf, iBytesRecv, &iBytesWritten, NULL ) ) {
									pItem->iRecvSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
									/// Simulate transfer slow download
									Sleep( DEBUG_XFER_SLOWDOWN );
#endif ///DEBUG_XFER_SLOWDOWN
#ifdef DEBUG_XFER_PROGRESS
									/// Display transfer progress
									TRACE(
										_T( "  Th:%s ThreadTransfer(ID:%u, Recv:(%d%%)%I64u/%I64u, %s %s -> %s)\n" ),
										pItem->pThread->szName, pItem->iId,
										ItemGetRecvPercent( pItem ),
										pItem->iRecvSize, pItem->iFileSize,
										pItem->szMethod, pItem->pszURL, pItem->Local.pszFile
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
				}

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
			while ( err == ERROR_SUCCESS ) {
#ifdef DEBUG_XFER_MAX_BYTES
				/// Simulate connection drop after transferring DEBUG_XFER_MAX_BYTES bytes
				if ( piRecvBytes && (*piRecvBytes >= DEBUG_XFER_MAX_BYTES) ) {
					err = ThreadSetWin32Error( pItem, ERROR_INTERNET_CONNECTION_RESET );
					break;
				}
#endif ///DEBUG_XFER_MAX_BYTES
				if ( !ThreadIsTerminating( pItem->pThread ) ) {
					if ( InternetReadFile( pItem->hRequest, pItem->Local.pMemory + pItem->iRecvSize, TRANSFER_CHUNK_SIZE, &iBytesRecv ) ) {
						if ( iBytesRecv > 0 ) {
							pItem->iRecvSize += iBytesRecv;
#ifdef DEBUG_XFER_SLOWDOWN
							/// Simulate transfer slow download
							Sleep( DEBUG_XFER_SLOWDOWN );	/// Emulate slow download
#endif ///DEBUG_XFER_SLOWDOWN
#ifdef DEBUG_XFER_PROGRESS
							/// Display transfer progress
							TRACE(
								_T( "  Th:%s ThreadTransfer(ID:%u, Recv:(%d%%)%I64u/%I64u, %s %s -> Memory)\n" ),
								pItem->pThread->szName, pItem->iId,
								ItemGetRecvPercent( pItem ),
								pItem->iRecvSize, pItem->iFileSize,
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

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload
VOID ThreadDownload( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );
	TRACE(
		_T( "  Th:%s ThreadDownload(ID:%u, %s %s -> %s)\n" ),
		pItem->pThread->szName,
		pItem->iId,
		pItem->szMethod,
		pItem->pszURL,
		pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
		);

	if ( pItem->pszURL && *pItem->pszURL ) {

		if ( ThreadDownload_OpenSession( pItem ) ) {

			BOOL bReconnectAllowed = TRUE;
			for (int i = 0; (i < 1000) && bReconnectAllowed; i++) {

				bReconnectAllowed = FALSE;
				if ( ThreadDownload_RemoteConnect( pItem, (BOOL)(i > 0) ) ) {

					ULONG64 iRecvBytes = 0;

					if ( i == 0 )
						ThreadTraceHttpInfo( pItem, HTTP_QUERY_RAW_HEADERS_CRLF );

					if ( ThreadDownload_LocalCreate( pItem ) ) {
						if ( ThreadDownload_Transfer( pItem, &iRecvBytes ) ) {
							// Success
						}
						ThreadDownload_LocalClose( pItem );
					}
					ThreadDownload_RemoteDisconnect( pItem );

					/// Reconnect and resume?
					bReconnectAllowed =
						pItem->iWin32Error != ERROR_SUCCESS &&
						pItem->iWin32Error != ERROR_INTERNET_OPERATION_CANCELLED &&
						iRecvBytes > 0 &&			/// We already received something...
						ItemIsReconnectAllowed( pItem );
				}
			}
		}
	}
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
		szWebError[0] = 0;
		DWORD dwWebErrorLen = ARRAYSIZE( szWebError );
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
