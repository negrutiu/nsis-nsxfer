#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define NSDOWN_USERAGENT _T("Mozilla/5.0 (NSdown)")


DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem );


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
		// Dequeue the first waiting item
		QueueLock( (PQUEUE)pThread->pQueue );
		pItem = QueueFindFirstWaiting( (PQUEUE)pThread->pQueue );
		if ( pItem ) {
			SYSTEMTIME st;
			GetLocalTime( &st );
			SystemTimeToFileTime( &st, &pItem->tmDownloadStart );
			pItem->iStatus = ITEM_STATUS_DOWNLOADING;
		}
		QueueUnlock( (PQUEUE)pThread->pQueue );

		// Start downloading
		if ( pItem ) {

			ThreadDownload( pThread, pItem );

		} else {

			// Wait for something to happen
			DWORD iWait = WaitForMultipleObjects( 2, handles, FALSE, INFINITE );
			if ( iWait == WAIT_OBJECT_0 ) {
				// TERM event
				TRACE( _T( "  Th:%s received TERM signal\n" ), pThread->szName );
				break;
			}
			else if ( iWait == WAIT_OBJECT_0 + 1 ) {
				// WAKE event
				TRACE( _T( "  Th:%s received WAKE signal\n" ), pThread->szName );
			}
			else {
				// Some error...
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


//++ ThreadDownload
VOID ThreadDownload( _In_ PTHREAD pThread, _Inout_ PQUEUE_ITEM pItem )
{
	assert( pThread && pItem );
	TRACE(
		_T( "  ThreadDownload(Th:%s, ID:%u, %s -> %s)\n" ),
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

					/// Delay between connection attempts
					if ( i > 0 )
						Sleep( iConnectDelay );

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
								ULONG iDummy = sizeof(pItem->iErrorCode);
								pItem->bErrorCodeIsHTTP = TRUE;
								HttpQueryInfo( hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &pItem->iErrorCode, &iDummy, NULL );
								if ( pItem->iErrorCode <= 299 )		/// Codes 1xx are informational. Codes 2xx are successful. Others represent errors.
								{
									// Download the content only when the caller requests it
									/*if ( pBuffer && iBufferSize )
									{
										// Read server's response
										// TODO: Reading loop...
										DWORD dwBytes;
										if ( InternetReadFile( hRequest, pBuffer, iBufferSize, &dwBytes ) ) {

											/// Done
											if ( piBytesReturned )
												*piBytesReturned = dwBytes;

										}
										else {
											err = GetLastError();
										}
									}*/

									/// Break the retry loop
									break;
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
							InternetGetLastResponseInfo( &dwWebError, szWebError, &dwWebErrorLen );
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
	{
		SYSTEMTIME st;
		GetLocalTime( &st );
		SystemTimeToFileTime( &st, &pItem->tmDownloadEnd );
		pItem->iStatus = ITEM_STATUS_DONE;
	}
}