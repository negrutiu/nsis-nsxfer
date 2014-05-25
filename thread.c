#include "main.h"
#include "thread.h"
#include "queue.h"
#include "utils.h"


#define NSDOWN_USERAGENT			_T("Mozilla/5.0 (NSdown)")
#define INVALID_FILE_SIZE64			(ULONG64)-1
#define TRANSFER_CHUNK_SIZE			256			/// 256 KiB
#define MAX_MEMORY_CONTENT_LENGTH	104857600	/// 100 MiB

/*
	TODO:
	- Modify errors to: LastWin32Error & LastHttpStatus
	- Reconnect during transfer
*/

DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
VOID ThreadDownload( _Inout_ PQUEUE_ITEM pItem );


//++ ThreadIsTerminating
BOOL ThreadIsTerminating( _In_ PTHREAD pThread )
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
	iTempErr = HttpQueryInfo( pItem->hConnect, iHttpInfo, szTemp, &iTempSize, NULL ) ? ERROR_SUCCESS : GetLastError();
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
#endif ///DBG || _DEBUG


//++ ThreadDownload_Session
BOOL ThreadDownload_Session( _Inout_ PQUEUE_ITEM pItem )
{
	DWORD err = ERROR_SUCCESS;
	assert( pItem );
	assert( pItem->hSession == NULL );

	pItem->hSession = InternetOpen( NSDOWN_USERAGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if ( pItem->hSession ) {

		/// Options
		if ( pItem->iConnectRetries != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_RETRIES, &pItem->iConnectRetries, sizeof( pItem->iConnectRetries ) );
		if ( pItem->iConnectTimeout != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &pItem->iConnectTimeout, sizeof( pItem->iConnectTimeout ) );
		if ( pItem->iReceiveTimeout != DEFAULT_VALUE )
			InternetSetOption( pItem->hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &pItem->iReceiveTimeout, sizeof( pItem->iReceiveTimeout ) );

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
		err = GetLastError();	/// InternetOpen
	}

	/// Remember the error code
	pItem->iErrorCode = err;
	pItem->bErrorCodeIsHTTP = FALSE;
	MyFree( pItem->pszErrorText );

	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload_RemoteDisconnect
VOID ThreadDownload_RemoteDisconnect( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );

	if ( pItem->hConnect ) {
		InternetCloseHandle( pItem->hConnect );
		pItem->hConnect = NULL;
	}
	pItem->iFileSize = INVALID_FILE_SIZE64;
}


//++ ThreadDownload_RemoteConnect
BOOL ThreadDownload_RemoteConnect( _Inout_ PQUEUE_ITEM pItem )
{
	BOOL bRet = FALSE;
	ULONG i, iConnectRetries, iConnectDelay;
	ULONG iConnectFlags, iRequestFlags;

	assert( pItem );
	assert( pItem->hConnect == NULL );

	iConnectRetries = (pItem->iRetryCount != DEFAULT_VALUE ? pItem->iRetryCount : 1);	/// Default: 1
	iConnectDelay = (pItem->iRetryDelay != DEFAULT_VALUE ? pItem->iRetryDelay : 0);		/// Default: 0

	// InternetOpen flags
	iConnectFlags =
		INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
	///	INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
		INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI |
		INTERNET_FLAG_RELOAD;
	if ( CompareString( 0, NORM_IGNORECASE, pItem->pszURL, 8, _T( "https://" ), 8 ) == CSTR_EQUAL )
		iConnectFlags |= INTERNET_FLAG_SECURE;

	// InternetOpenUrl flags
	iRequestFlags =
		SECURITY_FLAG_IGNORE_REVOCATION |
	///	SECURITY_FLAG_IGNORE_UNKNOWN_CA |
	///	SECURITY_FLAG_IGNORE_CERT_CN_INVALID
		SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

	/// Multiple retries in case of failure
	for ( i = 0; i < iConnectRetries; i++ ) {

		/// Check TERM event
		if ( ThreadIsTerminating( pItem->pThread ) )
			break;

		/// Delay between attempts. Keep monitoring TERM event
		if ( i > 0 ) {
			DWORD iWait = WaitForSingleObject( pItem->pThread->hTermEvent, iConnectDelay );
			if ( iWait == WAIT_OBJECT_0 )
				break;
		}

		// Connect
		/// TODO: Call InternetCanonicalizeUrl first if the URL being used contains a relative URL and a base URL separated by blank spaces
		pItem->hConnect = InternetOpenUrl( pItem->hSession, pItem->pszURL, NULL, 0, iConnectFlags, (DWORD_PTR)pItem );
		if ( pItem->hConnect ) {

			// On some Vistas (e.g. Home), HttpSendRequest returns ERROR_INTERNET_SEC_CERT_REV_FAILED if authenticated proxy is used
			// We've decided to ignore the revocation status.
			InternetSetOption( pItem->hConnect, INTERNET_OPTION_SECURITY_FLAGS, &iRequestFlags, sizeof( DWORD ) );

			// The stupid 'Work offline' setting from IE
			InternetSetOption( pItem->hConnect, INTERNET_OPTION_IGNORE_OFFLINE, 0, 0 );

			// Send the HTTP request
			if ( HttpSendRequest( pItem->hConnect, NULL, 0, NULL, 0 ) ) {

				/// Check the query status code
				TCHAR szErrorText[512];
				ULONG iDataSize;

				pItem->bErrorCodeIsHTTP = TRUE;
				iDataSize = sizeof( pItem->iErrorCode );
				HttpQueryInfo( pItem->hConnect, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &pItem->iErrorCode, &iDataSize, NULL );

				iDataSize = sizeof( szErrorText );
				szErrorText[0] = 0;
				HttpQueryInfo( pItem->hConnect, HTTP_QUERY_STATUS_TEXT, szErrorText, &iDataSize, NULL );
				if ( *szErrorText )
					MyStrDup( pItem->pszErrorText, szErrorText );

				// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
				if ( pItem->iErrorCode <= 299 ) {

					/// 1xx Informational
					/// 2xx Success

					//ThreadTraceHttpInfo( pThread, hConnect, HTTP_QUERY_RAW_HEADERS_CRLF );

					/// Success. Break the loop
					bRet = TRUE;
					break;

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
						/// Error. Break the loop
						ThreadDownload_RemoteDisconnect( pItem );
						break;
					}
				}

			} else {
				/// HttpOpenRequest error
				pItem->iErrorCode = GetLastError();
				ThreadDownload_RemoteDisconnect( pItem );
			}

		} else {
			/// InternetOpenUrl error
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

	return bRet;
}


//++ ThreadDownload_QueryContentLength
ULONG ThreadDownload_QueryContentLength( _In_ HINTERNET hFile, _Out_ PULONG64 piContentLength )
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


//++ ThreadDownload_LocalCreate
ULONG ThreadDownload_LocalCreate( _Inout_ PQUEUE_ITEM pItem )
{
	ULONG err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hConnect != NULL );

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
			ThreadDownload_QueryContentLength( pItem->hConnect, &pItem->iFileSize );

			// Resume download?
			if ( pItem->bResume && (pItem->iFileSize != INVALID_FILE_SIZE64) ) {

				pItem->Local.hFile = CreateFile( pItem->Local.pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( pItem->Local.hFile != INVALID_HANDLE_VALUE ) {
					LARGE_INTEGER iExistingSize;
					if ( GetFileSizeEx( pItem->Local.hFile, &iExistingSize ) ) {
						if ( (ULONG64)iExistingSize.QuadPart <= pItem->iFileSize ) {
							if ( (SetFilePointer( pItem->Local.hFile, 0, NULL, FILE_END ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS) ) {
								if ( (InternetSetFilePointer( pItem->hConnect, iExistingSize.LowPart, &iExistingSize.HighPart, FILE_BEGIN, 0 ) != INVALID_SET_FILE_POINTER) || (GetLastError() == ERROR_SUCCESS) ) {
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

			/// Close the file on errors
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
			err = ThreadDownload_QueryContentLength( pItem->hConnect, &pItem->iFileSize );
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

	/// Remember the error code
	if ( err != ERROR_SUCCESS ) {
		pItem->iErrorCode = err;
		pItem->bErrorCodeIsHTTP = FALSE;
		MyFree( pItem->pszErrorText );
	}

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
BOOL ThreadDownload_Transfer( _Inout_ PQUEUE_ITEM pItem )
{
	DWORD err = ERROR_SUCCESS;

	assert( pItem );
	assert( pItem->hConnect != NULL );

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
				while ( err == ERROR_SUCCESS ) {
					if ( !ThreadIsTerminating( pItem->pThread )) {
						if ( InternetReadFile( pItem->hConnect, pBuf, iBufSize, &iBytesRecv ) ) {
							if ( iBytesRecv > 0 ) {
								if ( WriteFile( pItem->Local.hFile, pBuf, iBytesRecv, &iBytesWritten, NULL ) ) {
									pItem->iRecvSize += iBytesRecv;
									///Sleep( 200 );		/// Emulate slow download
								} else {
									err = GetLastError();	/// WriteFile
								}
							} else {
								// Transfer complete
								break;
							}
						} else {
							err = GetLastError();	/// InternetReadFile
							/*if ( err == ERROR_INTERNET_EXTENDED_ERROR ) {
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
							}*/
						}
					} else {
						err = ERROR_INTERNET_OPERATION_CANCELLED;
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
				if ( !ThreadIsTerminating( pItem->pThread ) ) {
					if ( InternetReadFile( pItem->hConnect, pItem->Local.pMemory + pItem->iRecvSize, TRANSFER_CHUNK_SIZE, &iBytesRecv ) ) {
						if ( iBytesRecv > 0 ) {
							pItem->iRecvSize += iBytesRecv;
							///Sleep( 200 );		/// Emulate slow download
						} else {
							// Transfer complete
							break;
						}
					} else {
						err = GetLastError();	/// InternetReadFile
						/*if ( err == ERROR_INTERNET_EXTENDED_ERROR ) {
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
						}*/
					}
				} else {
					err = ERROR_INTERNET_OPERATION_CANCELLED;
				}
			}
			break;
		}
	}

	if ( err != ERROR_SUCCESS ) {
		pItem->iErrorCode = err;
		pItem->bErrorCodeIsHTTP = FALSE;
		MyFree( pItem->pszErrorText );
	}
	return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}


//++ ThreadDownload
VOID ThreadDownload( _Inout_ PQUEUE_ITEM pItem )
{
	assert( pItem );
	TRACE(
		_T( "  Th:%s ThreadDownload(ID:%u, %s -> %s)\n" ),
		pItem->pThread->szName,
		pItem->iId,
		pItem->pszURL,
		pItem->iLocalType == ITEM_LOCAL_NONE ? _T( "None" ) : (pItem->iLocalType == ITEM_LOCAL_FILE ? pItem->Local.pszFile : _T( "Memory" ))
		);

	/// Initialize Win32 error
	pItem->bErrorCodeIsHTTP = FALSE;
	pItem->iErrorCode = ERROR_SUCCESS;

	if ( pItem->pszURL && *pItem->pszURL ) {

		if ( ThreadDownload_Session( pItem ) ) {
			if ( ThreadDownload_RemoteConnect( pItem ) ) {

				ThreadTraceHttpInfo( pItem, HTTP_QUERY_RAW_HEADERS_CRLF );

				if ( ThreadDownload_LocalCreate( pItem ) ) {
					if ( ThreadDownload_Transfer( pItem ) ) {
						// Success
					}
					ThreadDownload_LocalClose( pItem );
				}

				ThreadDownload_RemoteDisconnect( pItem );
			}
		}
	}

	// Error as text
	if ( !pItem->bErrorCodeIsHTTP &&	/// Win32 error (HTTP errors should already have been retrieved)
		!pItem->pszErrorText )			/// Don't overwrite existing error text
	{
		AllocErrorStr( pItem->iErrorCode, &pItem->pszErrorText );
	}
}