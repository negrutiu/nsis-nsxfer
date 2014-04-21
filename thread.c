#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define NSDOWN_USERAGENT _T("Mozilla/5.0 (NSdown)")


DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem );


//++ ThreadTerminating
BOOL ThreadTerminating( _In_ PTHREAD pThread )
{
	assert( pThread );
	assert( pThread->hTermEvent );

	if ( WaitForSingleObject( pThread->hTermEvent, 0 ) == WAIT_TIMEOUT )
		return FALSE;

	return TRUE;
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
		if ( ThreadTerminating( pThread )) {
			TRACE( _T( "  Th:%s received TERM signal\n" ), pThread->szName );
			break;
		}

		// Dequeue the first waiting item
		QueueLock( (PQUEUE)pThread->pQueue );
		pItem = QueueFindFirstWaiting( (PQUEUE)pThread->pQueue );
		if ( pItem ) {
			GetLocalFileTime( &pItem->tmDownloadStart );
			pItem->iStatus = ITEM_STATUS_DOWNLOADING;
			TRACE( _T( "  Th:%s dequeued item ID:%u, %s\n" ), pThread->szName, pItem->iId, pItem->pszURL );
		} else {
			TRACE( _T( "  Th:%s has nothing to do, going to sleep\n" ), pThread->szName );
		}
		QueueUnlock( (PQUEUE)pThread->pQueue );

		// Start downloading
		if ( pItem ) {

			ThreadDownload( pThread, pItem );

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


//++ ThreadDestroyCrackedUrl
DWORD ThreadDestroyCrackedUrl( __in URL_COMPONENTS *pUrlComps )
{
	DWORD err = ERROR_SUCCESS;
	if ( pUrlComps ) {

		if ( pUrlComps->lpszScheme )
			MyFree( pUrlComps->lpszScheme );
		if ( pUrlComps->lpszHostName )
			MyFree( pUrlComps->lpszHostName );
		if ( pUrlComps->lpszUserName )
			MyFree( pUrlComps->lpszUserName );
		if ( pUrlComps->lpszPassword )
			MyFree( pUrlComps->lpszPassword );
		if ( pUrlComps->lpszUrlPath )
			MyFree( pUrlComps->lpszUrlPath );
		if ( pUrlComps->lpszExtraInfo )
			MyFree( pUrlComps->lpszExtraInfo );

		MyZeroMemory( pUrlComps, sizeof(*pUrlComps) );

	}
	else {
		err = GetLastError();
	}
	return err;
}


//++ ThreadInitCrackedUrl
DWORD ThreadInitCrackedUrl( __in LPCTSTR pszUrl, __out URL_COMPONENTS *pUrlComps )
{
	DWORD err = ERROR_SUCCESS;
	if ( pszUrl && *pszUrl && pUrlComps ) {

		ULONG iUrlLen = lstrlen( pszUrl );

		MyZeroMemory( pUrlComps, sizeof(*pUrlComps) );
		pUrlComps->dwStructSize = sizeof(*pUrlComps);

		pUrlComps->dwSchemeLength = iUrlLen;
		pUrlComps->lpszScheme = MyAllocStr( pUrlComps->dwSchemeLength );
		pUrlComps->dwHostNameLength = iUrlLen;
		pUrlComps->lpszHostName = MyAllocStr( pUrlComps->dwHostNameLength );
		pUrlComps->dwUserNameLength = iUrlLen;
		pUrlComps->lpszUserName = MyAllocStr( pUrlComps->dwUserNameLength );
		pUrlComps->dwPasswordLength = iUrlLen;
		pUrlComps->lpszPassword = MyAllocStr( pUrlComps->dwPasswordLength );
		pUrlComps->dwUrlPathLength = iUrlLen;
		pUrlComps->lpszUrlPath = MyAllocStr( pUrlComps->dwUrlPathLength );
		pUrlComps->dwExtraInfoLength = iUrlLen;
		pUrlComps->lpszExtraInfo = MyAllocStr( pUrlComps->dwExtraInfoLength );

		if ( pUrlComps->lpszScheme && pUrlComps->lpszHostName && pUrlComps->lpszUserName && pUrlComps->lpszPassword && pUrlComps->lpszUrlPath && pUrlComps->lpszExtraInfo )
		{
			if ( InternetCrackUrl( pszUrl, 0, 0, pUrlComps ) ) {
				// Success
			} else {
				err = GetLastError();
			}
		} else {
			err = ERROR_OUTOFMEMORY;
		}

		/// Cleanup in case of errors
		if ( err != ERROR_SUCCESS )
			ThreadDestroyCrackedUrl( pUrlComps );

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ ThreadTraceHttpInfo
#if DBG || _DEBUG
#if UNICODE
#define ThreadTraceHttpInfo( pThread, hSession, iHttpInfo ) \
	ThreadTraceHttpInfoImpl( pThread, hSession, iHttpInfo, L#iHttpInfo )
#else
#define ThreadTraceHttpInfo( pThread, hSession, iHttpInfo ) \
	ThreadTraceHttpInfoImpl( pThread, hSession, iHttpInfo, #iHttpInfo )
#endif

VOID ThreadTraceHttpInfoImpl( _In_ PTHREAD pThread, _In_ HINTERNET hSession, _In_ UINT iHttpInfo, _In_ LPCTSTR szHttpInfo )
{
	TCHAR szTemp[512];
	ULONG iTempSize = sizeof(szTemp);
	ULONG iTempErr = 0;

	szTemp[0] = 0;
	iTempErr = HttpQueryInfo( hSession, iHttpInfo, szTemp, &iTempSize, NULL ) ? ERROR_SUCCESS : GetLastError();
	if ( iTempErr != ERROR_HTTP_HEADER_NOT_FOUND ) {
		if ( iTempErr == ERROR_SUCCESS ) {
			TRACE( _T( "  Th:%s HttpQueryInfo( %s ) == \"%s\" [%u bytes]\n" ), pThread->szName, szHttpInfo, szTemp, iTempSize );
		} else {
			LPTSTR pszTemp = NULL;
			AllocErrorStr( iTempErr, &pszTemp );
			TRACE( _T( "  Th:%s HttpQueryInfo( %s ) == %u \"%s\"\n" ), pThread->szName, szHttpInfo, iTempErr, pszTemp );
			MyFree( pszTemp );
		}
	}
}
#endif ///DBG || _DEBUG


//++ ThreadDownload
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem )
{
	assert( pThread && pItem );
	TRACE(
		_T( "  Th:%s ThreadDownload(ID:%u, %s -> %s)\n" ),
		pThread->szName,
		pItem->iId,
		pItem->pszURL,
		pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->LocalData.pszFile : _T( "Memory" ))
		);

	/// Initialize Win32 error
	pItem->bErrorCodeIsHTTP = FALSE;
	pItem->iErrorCode = ERROR_SUCCESS;

	if ( pItem->pszURL && *pItem->pszURL ) {

		HINTERNET hSession = InternetOpen( NSDOWN_USERAGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
		if ( hSession ) {

			DWORD dwConnectState, dwConnectStateSize;
			URL_COMPONENTS uc;

			if ( pItem->iConnectRetries != DEFAULT_VALUE )
				InternetSetOption( hSession, INTERNET_OPTION_CONNECT_RETRIES, &pItem->iConnectRetries, sizeof(pItem->iConnectRetries) );
			if ( pItem->iConnectTimeout != DEFAULT_VALUE )
				InternetSetOption( hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &pItem->iConnectTimeout, sizeof(pItem->iConnectTimeout) );
			if ( pItem->iReceiveTimeout != DEFAULT_VALUE )
				InternetSetOption( hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &pItem->iReceiveTimeout, sizeof(pItem->iReceiveTimeout) );

			/// Reconnect if disconnected by user
			dwConnectState = 0;
			dwConnectStateSize = sizeof(dwConnectState);
			if ( InternetQueryOption( hSession, INTERNET_OPTION_CONNECTED_STATE, &dwConnectState, &dwConnectStateSize ) &&
				(dwConnectState & INTERNET_STATE_DISCONNECTED_BY_USER) )
			{
				INTERNET_CONNECTED_INFO ci = { INTERNET_STATE_CONNECTED, 0 };
				InternetSetOption( hSession, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof(ci) );
			}

			/// Crack the URL
			pItem->iErrorCode = ThreadInitCrackedUrl( pItem->pszURL, &uc );
			if ( pItem->iErrorCode == ERROR_SUCCESS ) {

				ULONG i;
				ULONG iConnectRetries = (pItem->iRetryCount != DEFAULT_VALUE ? pItem->iRetryCount : 1);
				ULONG iConnectDelay = (pItem->iRetryDelay != DEFAULT_VALUE ? pItem->iRetryDelay : 0);
				HINTERNET hConnect;

				/// Multiple retries in case of failure
				for ( i = 0; i < iConnectRetries; i++ ) {

					/// Check TERM signal
					if ( ThreadTerminating( pThread ) )
						break;

					/// Delay between connection attempts. Monitor TERM signal
					if ( i > 0 ) {
						DWORD iWait = WaitForSingleObject( pThread->hTermEvent, iConnectDelay );
						if ( iWait == WAIT_OBJECT_0 )
							break;
					}

					hConnect = InternetConnect(
						hSession,
						uc.lpszHostName, uc.nPort,
						*uc.lpszUserName ? uc.lpszUserName : NULL,
						*uc.lpszPassword ? uc.lpszPassword : NULL,
						INTERNET_SERVICE_HTTP, 0, 0
						);

					if ( hConnect ) {

						// Make an HTTP request
						HINTERNET hRequest;
						LPCTSTR szReqType[2] = { _T( "*/*" ), 0 };
						LPTSTR pszWebObj;
						ULONG iWebObjLen;

						DWORD dwReqFlags = INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD;
						if ( uc.nScheme == INTERNET_SCHEME_HTTPS )
							dwReqFlags |= INTERNET_FLAG_SECURE;

						iWebObjLen = uc.dwUrlPathLength + uc.dwExtraInfoLength;
						pszWebObj = MyAllocStr( iWebObjLen );
						assert( pszWebObj );
						wnsprintf( pszWebObj, iWebObjLen + 1, _T( "%s%s" ), uc.lpszUrlPath, uc.lpszExtraInfo );
						hRequest = HttpOpenRequest( hConnect, _T( "GET" ), pszWebObj, _T( "HTTP/1.1" ), NULL, szReqType, dwReqFlags, 0 );
						MyFree( pszWebObj );
						if ( hRequest ) {

							// On some Vistas (e.g. Home), HttpSendRequest returns ERROR_INTERNET_SEC_CERT_REV_FAILED if authenticated proxy is used
							// We've decided to ignore the revocation status.
							DWORD dwFlag = SECURITY_FLAG_IGNORE_REVOCATION | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
							InternetSetOption( hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlag, sizeof(DWORD) );

							// The stupid 'Work offline' setting from IE
							InternetSetOption( hRequest, INTERNET_OPTION_IGNORE_OFFLINE, 0, 0 );

							// Send the request
							if ( HttpSendRequest( hRequest, NULL, 0, NULL, 0 ) ) {

								/// Check the query status code
								TCHAR szErrorText[512];
								ULONG iDataSize;
								
								iDataSize = sizeof(pItem->iErrorCode);
								pItem->bErrorCodeIsHTTP = TRUE;
								HttpQueryInfo( hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &pItem->iErrorCode, &iDataSize, NULL );

								iDataSize = sizeof(szErrorText);
								szErrorText[0] = 0;
								HttpQueryInfo( hRequest, HTTP_QUERY_STATUS_TEXT, szErrorText, &iDataSize, NULL );
								if ( *szErrorText )
									MyStrDup( pItem->pszErrorText, szErrorText );

								// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
								if ( pItem->iErrorCode <= 299 )
								{
									/// 1xx Informational
									/// 2xx Success

									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_RAW_HEADERS_CRLF );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_SERVER );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_TYPE );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_TRANSFER_ENCODING );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_ID );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_DESCRIPTION );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_LENGTH );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_LANGUAGE );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_BASE );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_ENCODING );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_CONTENT_MD5 );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_LAST_MODIFIED );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_URI );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_LINK );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_VERSION );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_DATE );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_ALLOW );
									//ThreadTraceHttpInfo( pThread, hRequest, HTTP_QUERY_EXPIRES );

									// Download the content only when the caller requests it
									if ( pItem->iLocalType != ITEM_LOCAL_NONE )
									{
										// Query the remote file size
										iDataSize = sizeof(pItem->iFileSize);
										if ( !HttpQueryInfo( hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &pItem->iFileSize, &iDataSize, NULL ) )
											pItem->iFileSize = (ULONG64)-1;

										/*										// Read server's response
										// TODO: Reading loop...
										// TODO: Check TERM signal
										DWORD dwBytes;
										if ( InternetReadFile( hRequest, pBuffer, iBufferSize, &dwBytes ) ) {

											/// Done
											if ( piBytesReturned )
												*piBytesReturned = dwBytes;

										}
										else {
											err = GetLastError();
										}
										*/
									}

									/// Break the retry loop
									i = iConnectRetries;

								} else {

									/// 3xx Redirection - The client must take additional action to complete the request
									/// 4xx Client Error
									/// 5xx Server Error

									if ( pItem->iErrorCode != HTTP_STATUS_SERVICE_UNAVAIL &&	/// 503 Service Unavailable
										pItem->iErrorCode != HTTP_STATUS_GATEWAY_TIMEOUT &&		/// 504 Gateway Timeout
										pItem->iErrorCode != 598 &&								/// 598 Network read timeout error (Unknown)
										pItem->iErrorCode != 599								/// 599 Network connect timeout error (Unknown)
										)
									{
										/// Break the retry loop
										i = iConnectRetries;
									}
								}

							} else {
								pItem->iErrorCode = GetLastError();
							}

							InternetCloseHandle( hRequest );

						} else {
							pItem->iErrorCode = GetLastError();
						}

						InternetCloseHandle( hConnect );

					} else {

						pItem->iErrorCode = GetLastError();
						if ( pItem->iErrorCode == ERROR_INTERNET_EXTENDED_ERROR ) {
							DWORD dwWebError;
							TCHAR szWebError[255];
							szWebError[0] = 0;
							DWORD dwWebErrorLen = ARRAYSIZE( szWebError );
							if ( InternetGetLastResponseInfo( &dwWebError, szWebError, &dwWebErrorLen ) ) {
								///pItem->bErrorCodeIsHTTP = TRUE;
								///pItem->iErrorCode = dwWebError;
								MyFree( pItem->pszErrorText );
								MyStrDup( pItem->pszErrorText, szWebError );
							}
						}
					}
				}	/// for

				ThreadDestroyCrackedUrl( &uc );
			}

			InternetCloseHandle( hSession );

		} else {
			pItem->iErrorCode = GetLastError();
		}
	}

	// Mark this item as done
	GetLocalFileTime( &pItem->tmDownloadEnd );
	pItem->iStatus = ITEM_STATUS_DONE;

	// Error as text
	if ( !pItem->bErrorCodeIsHTTP &&	/// Win32 error (HTTP errors should already have been retrieved)
		 !pItem->pszErrorText )			/// Don't overwrite existing error text
	{
		AllocErrorStr( pItem->iErrorCode, &pItem->pszErrorText );
	}
}