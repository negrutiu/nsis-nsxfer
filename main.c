
#include "main.h"
#include "utils.h"
#include "queue.h"
#include "gui.h"

HINSTANCE g_hInst = NULL;
BOOL g_bInitialized = FALSE;
QUEUE g_Queue = { 0 };


/// Forward declarations
UINT_PTR __cdecl NsisMessageCallback( enum NSPIM iMessage );


//++ PluginInit
BOOL PluginInit()
{
	BOOL bRet = TRUE;
	if (!g_bInitialized) {

		TRACE( _T( "PluginInit\n" ) );

		UtilsInitialize();

		if (TRUE) {
			SYSTEM_INFO si;
			GetSystemInfo( &si );
			QueueInitialize( &g_Queue, _T( "MAIN" ), __max( si.dwNumberOfProcessors, MIN_WORKER_THREADS ) );
		}

		if ( TRUE ) {
			g_bInitialized = TRUE;
		} else {
			bRet = FALSE;
		}
	}
	return bRet;
}


//++ PluginUninit
BOOL PluginUninit( _In_ BOOLEAN bForced )
{
	BOOL bRet = FALSE;
	if ( g_bInitialized ) {

		TRACE( _T( "PluginUninit(Forced:%u)\n" ), (ULONG)bForced );

		QueueDestroy( &g_Queue, bForced );
		UtilsDestroy();

		g_bInitialized = FALSE;
		bRet = TRUE;
	}
	return bRet;
}


//++ NsisMessageCallback
UINT_PTR __cdecl NsisMessageCallback( enum NSPIM iMessage )
{
	switch ( iMessage )
	{
	case NSPIM_UNLOAD:
		TRACE( _T( "NSPIM_UNLOAD\n" ) );
		PluginUninit( FALSE );
		break;

	case NSPIM_GUIUNLOAD:
		TRACE( _T( "NSPIM_GUIUNLOAD\n" ) );
		break;
	}
	return 0;
}


//++ ParseRequestParameter
BOOL ParseRequestParameter(
	_In_ int string_size,
	_In_ LPTSTR psz,		/// Working buffer with the current parameter
	_In_ PQUEUE_REQUEST_PARAM pParam
	)
{
	BOOL bRet = TRUE;		/// Assume that the current argument is valid
	assert( string_size && psz && pParam );

	if (lstrcmpi( psz, _T( "/PRIORITY" ) ) == 0) {
		pParam->iPriority = popint();
	} else if (lstrcmpi( psz, _T( "/DEPEND" ) ) == 0) {
		pParam->iDependId = popint();
	} else if (lstrcmpi( psz, _T( "/METHOD" ) ) == 0) {
		if (popstring( psz ) == 0) {
			assert(
				lstrcmpi( psz, _T( "GET" ) ) == 0 ||
				lstrcmpi( psz, _T( "POST" ) ) == 0 ||
				lstrcmpi( psz, _T( "HEAD" ) ) == 0
				);
			if (*psz) {
				MyFree( pParam->pszMethod );
				MyStrDup( pParam->pszMethod, psz );
			}
		}
	} else if (lstrcmpi( psz, _T( "/URL" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszURL );
			MyStrDup( pParam->pszURL, psz );
		}
	} else if (lstrcmpi( psz, _T( "/LOCAL" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszLocalFile );
			if (lstrcmpi( psz, TEXT_LOCAL_NONE ) == 0) {
				pParam->iLocalType = REQUEST_LOCAL_NONE;
			} else if (lstrcmpi( psz, TEXT_LOCAL_MEMORY ) == 0) {
				pParam->iLocalType = REQUEST_LOCAL_MEMORY;
			} else {
				pParam->iLocalType = REQUEST_LOCAL_FILE;
				MyStrDup( pParam->pszLocalFile, psz );
			}
		}
	} else if (lstrcmpi( psz, _T( "/HEADERS" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszHeaders );
			MyStrDup( pParam->pszHeaders, psz );
		}
	} else if (lstrcmpi( psz, _T( "/DATA" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pData );
			pParam->iDataSize = 0;
#ifdef UNICODE
			pParam->pData = MyAlloc( string_size );
			if (pParam->pData) {
				WideCharToMultiByte( CP_ACP, 0, psz, -1, (LPSTR)pParam->pData, string_size, NULL, NULL );
				pParam->iDataSize = lstrlenA( (LPSTR)pParam->pData );	/// Don't trust what WideCharToMultiByte returns...
			}
#else
			MyStrDup( pParam->pData, psz );
			pParam->iDataSize = lstrlen( psz );
#endif
		}
	} else if (lstrcmpi( psz, _T( "/DATAFILE" ) ) == 0) {
		if (popstring( psz ) == 0) {
			HANDLE hFile = CreateFile( psz, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
			if (hFile != INVALID_HANDLE_VALUE) {
				ULONG iFileSize = GetFileSize( hFile, NULL );
				if (iFileSize != INVALID_FILE_SIZE || GetLastError() == ERROR_SUCCESS) {
					MyFree( pParam->pData );
					pParam->iDataSize = 0;
					pParam->pData = MyAlloc( iFileSize );
					if (pParam->pData) {
						if (!ReadFile( hFile, pParam->pData, iFileSize, &pParam->iDataSize, NULL )) {
							MyFree( pParam->pData );
							pParam->iDataSize = 0;
							assert( !"/DATAFILE: Failed to read" );
						}
					} else {
						assert( !"/DATAFILE: Failed to allocate memory" );
					}
				} else {
					assert( !"/DATAFILE: Failed to get size" );
				}
				CloseHandle( hFile );
			} else {
				assert( !"/DATAFILE: Failed to open" );
			}
		}
	} else if (lstrcmpi( psz, _T( "/TIMEOUTCONNECT" ) ) == 0) {
		pParam->iTimeoutConnect = popint();
	} else if (lstrcmpi( psz, _T( "/TIMEOUTRECONNECT" ) ) == 0) {
		pParam->iTimeoutReconnect = popint();
	} else if (lstrcmpi( psz, _T( "/OPTCONNECTRETRIES" ) ) == 0) {
		pParam->iOptConnectRetries = popint();
	} else if (lstrcmpi( psz, _T( "/OPTCONNECTTIMEOUT" ) ) == 0) {
		pParam->iOptConnectTimeout = popint();
	} else if (lstrcmpi( psz, _T( "/OPTRECEIVETIMEOUT" ) ) == 0) {
		pParam->iOptReceiveTimeout = popint();
	} else if (lstrcmpi( psz, _T( "/OPTSENDTIMEOUT" ) ) == 0) {
		pParam->iOptSendTimeout = popint();
	} else if (lstrcmpi( psz, _T( "/PROXY" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszProxy );
			MyStrDup( pParam->pszProxy, psz );
		}
	} else if (lstrcmpi( psz, _T( "/PROXYUSER" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszProxyUser );
			MyStrDup( pParam->pszProxyUser, psz );
		}
	} else if (lstrcmpi( psz, _T( "/PROXYPASS" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszProxyPass );
			MyStrDup( pParam->pszProxyPass, psz );
		}
	} else if (lstrcmpi( psz, _T( "/REFERER" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszReferrer );
			MyStrDup( pParam->pszReferrer, psz );
		}
	} else if (lstrcmpi( psz, _T( "/INTERNETFLAGS" ) ) == 0) {
		ULONG iFlags = (ULONG)popint_or();
		if (iFlags != 0)
			pParam->iHttpInternetFlags = iFlags;
	} else if (lstrcmpi( psz, _T( "/SECURITYFLAGS" ) ) == 0) {
		ULONG iFlags = (ULONG)popint_or();
		if (iFlags != 0)
			pParam->iHttpSecurityFlags = iFlags;
	} else {
		bRet = FALSE;	/// This parameter is not valid for Request
	}

	return bRet;
}


//++ Request
EXTERN_C __declspec(dllexport)
void __cdecl Request(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz = NULL;
	QUEUE_REQUEST_PARAM Param;
	PQUEUE_REQUEST pReq = NULL;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Request" ) );

	/// Receive unloading notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

	/// Working buffer
	psz = (LPTSTR)MyAlloc( string_size * sizeof(TCHAR) );
	assert( psz );

	/// Parameters
	RequestParamInit( Param );
	for (;;)
	{
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0)
			break;

		if (!ParseRequestParameter( string_size, psz, &Param )) {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}

	// Add to the queue
	QueueLock( &g_Queue );
	QueueAdd( &g_Queue, &Param, &pReq );
	pushint( pReq ? pReq->iId : 0 );	/// Return the request's ID
	QueueUnlock( &g_Queue );

	RequestParamDestroy( Param );
	MyFree( psz );
}


//++ QueryGlobal
EXTERN_C __declspec(dllexport)
void __cdecl QueryGlobal(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz;
	LPTSTR pParam[30];
	PQUEUE_REQUEST pReq;
	int iParamCount = 0, iDropCount = 0, i;

	ULONG iTotalThreads;
	ULONG iTotalCount = 0, iTotalCompleted = 0, iTotalDownloading = 0, iTotalWaiting = 0;
	ULONG64 iTotalRecvBytes = 0;
	ULONG iTotalSpeed = 0;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!QueryGlobal" ) );

	/// Allocate the working buffer
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	assert( psz );

	/// Parameters
	while (TRUE) {
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0)
			break;
		if (iParamCount < ARRAYSIZE( pParam )) {
			/// Remember this parameter
			MyStrDup( pParam[iParamCount], psz );
			iParamCount++;
		} else {
			/// Too many parameters
			iDropCount++;
		}
	}

	/// Statistics
	QueueLock( &g_Queue );
	iTotalThreads = (ULONG)g_Queue.iThreadCount;
	for (pReq = g_Queue.pHead; pReq; pReq = pReq->pNext) {
		iTotalCount++;
		if (pReq->iStatus == REQUEST_STATUS_DONE)
			iTotalCompleted++;
		if (pReq->iStatus == REQUEST_STATUS_DOWNLOADING)
			iTotalDownloading++;
		if (pReq->iStatus == REQUEST_STATUS_WAITING)
			iTotalWaiting++;
		iTotalRecvBytes += pReq->iRecvSize;
		if (pReq->iStatus == REQUEST_STATUS_DOWNLOADING)
			iTotalSpeed += pReq->Speed.iSpeed;
	}
	QueueUnlock( &g_Queue );

	/// Return empty strings for each dropped parameter
	for (i = 0; i < iDropCount; i++)
		pushstring( _T( "" ) );

	/// Iterate all parameters (in reverse order) and return their values
	for (i = iParamCount - 1; i >= 0; i--) {
		if (lstrcmpi( pParam[i], _T( "/TOTALCOUNT" ) ) == 0) {
			pushint( iTotalCount );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALWAITING" ) ) == 0) {
			pushint( iTotalWaiting );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALDOWNLOADING" ) ) == 0) {
			pushint( iTotalDownloading );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALCOMPLETED" ) ) == 0) {
			pushint( iTotalCompleted );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALRECVSIZE" ) ) == 0) {
			TCHAR szSize[50];
#ifdef UNICODE
			StrFormatByteSizeW( iTotalRecvBytes, szSize, ARRAYSIZE( szSize ) );
#else
			StrFormatByteSizeA( (ULONG)iTotalRecvBytes, szSize, ARRAYSIZE( szSize ) );
#endif
			pushstring( szSize );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALRECVSIZEBYTES" ) ) == 0) {
			pushintptr( iTotalSpeed );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALSPEED" ) ) == 0) {
			TCHAR szSpeed[50];
#ifdef UNICODE
			StrFormatByteSizeW( (LONGLONG)iTotalSpeed, szSpeed, ARRAYSIZE( szSpeed ) );
#else
			StrFormatByteSizeA( iTotalSpeed, szSpeed, ARRAYSIZE( szSpeed ) );
#endif
			lstrcat( szSpeed, TEXT_PER_SECOND );
			pushstring( szSpeed );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALSPEEDBYTES" ) ) == 0) {
			pushint( iTotalSpeed );
		} else if (lstrcmpi( pParam[i], _T( "/TOTALTHREADS" ) ) == 0) {
			pushint( iTotalThreads );
		} else if (lstrcmpi( pParam[i], _T( "/PLUGINNAME" ) ) == 0) {
			pushstring( PLUGINNAME );
		} else if (lstrcmpi( pParam[i], _T( "/PLUGINVERSION" ) ) == 0) {
			TCHAR szPath[MAX_PATH], szVer[50];
			szVer[0] = 0;
			if (GetModuleFileName( g_hInst, szPath, ARRAYSIZE( szPath ) ) > 0)
				ReadVersionInfoString( szPath, _T( "FileVersion" ), szVer, ARRAYSIZE( szVer ) );
			pushstring( szVer );
		} else if (lstrcmpi( pParam[i], _T( "/USERAGENT" ) ) == 0) {
			pushstring( TEXT_USERAGENT );
		} else {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), pParam[i] );
			pushstring( _T( "" ) );
		}
		MyFree( pParam[i] );
	}

	MyFree( psz );
}


//++ Query
EXTERN_C __declspec(dllexport)
void __cdecl Query(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz;
	ULONG iReqID = 0;
	PQUEUE_REQUEST pReq = NULL;

	LPTSTR pParam[30];
	int iParamCount = 0, iDropCount = 0, i;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Query" ) );

	/// Allocate the working buffer
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	assert( psz );

	/// Parameters
	while (TRUE) {
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0)
			break;

		if (lstrcmpi( psz, _T( "/ID" ) ) == 0) {
			iReqID = (ULONG)popint();
		} else {
			if (iParamCount < ARRAYSIZE( pParam )) {
				/// Remember this parameter
				MyStrDup( pParam[iParamCount], psz );
				iParamCount++;
			} else {
				/// Too many parameters
				iDropCount++;
			}
		}
	}

	/// Lock
	QueueLock( &g_Queue );

	/// Find the request ID
	pReq = QueueFind( &g_Queue, iReqID );

	/// Return empty strings for dropped parameters
	for (i = 0; i < iDropCount; i++)
		pushstring( _T( "" ) );

	/// Iterate all parameters (in reverse order) and return their values
	for (i = iParamCount - 1; i >= 0; i--) {
		if (pReq) {
			if (lstrcmpi( pParam[i], _T( "/PRIORITY" ) ) == 0) {
				pushint( pReq->iPriority );
			} else if (lstrcmpi( pParam[i], _T( "/DEPEND" ) ) == 0) {
				pushint( pReq->iDependId );
			} else if (lstrcmpi( pParam[i], _T( "/STATUS" ) ) == 0) {
				switch (pReq->iStatus) {
				case REQUEST_STATUS_WAITING:
					pushstring( TEXT_STATUS_WAITING );
					break;
				case REQUEST_STATUS_DOWNLOADING:
					pushstring( TEXT_STATUS_DOWNLOADING );
					break;
				case REQUEST_STATUS_DONE:
					pushstring( TEXT_STATUS_COMPLETED );
					break;
				default:
					pushstring( _T( "" ) );
				}
			} else if (lstrcmpi( pParam[i], _T( "/WININETSTATUS" ) ) == 0) {
				pushint( pReq->iLastCallbackStatus );
			} else if (lstrcmpi( pParam[i], _T( "/METHOD" ) ) == 0) {
				pushstring( pReq->szMethod );
			} else if (lstrcmpi( pParam[i], _T( "/URL" ) ) == 0) {
				pushstring( pReq->pszURL );
			} else if (lstrcmpi( pParam[i], _T( "/IP" ) ) == 0) {
				pushstring( pReq->pszSrvIP );
			} else if (lstrcmpi( pParam[i], _T( "/PROXY" ) ) == 0) {
				pushstring( pReq->pszProxy );
			} else if (lstrcmpi( pParam[i], _T( "/LOCAL" ) ) == 0) {
				switch (pReq->iLocalType) {
				case REQUEST_LOCAL_NONE:
					pushstring( TEXT_LOCAL_NONE );
					break;
				case REQUEST_LOCAL_FILE:
					pushstring( pReq->Local.pszFile );
					break;
				case REQUEST_LOCAL_MEMORY:
					pushstring( TEXT_LOCAL_MEMORY );
					break;
				default:
					pushstring( _T( "" ) );
				}
			} else if (lstrcmpi( pParam[i], _T( "/SENTHEADERS" ) ) == 0) {
				pushstring( pReq->pszHeaders );
			} else if (lstrcmpi( pParam[i], _T( "/RECVHEADERS" ) ) == 0) {
				pushstring( pReq->pszSrvHeaders );
			} else if (lstrcmpi( pParam[i], _T( "/FILESIZE" ) ) == 0) {
				if (pReq->iFileSize != INVALID_FILE_SIZE64) {
					TCHAR sz[30];
					wnsprintf( sz, ARRAYSIZE( sz ), _T( "%I64u" ), pReq->iFileSize );
					pushstring( sz );
				} else {
					pushstring( _T( "" ) );
				}
			} else if (lstrcmpi( pParam[i], _T( "/RECVSIZE" ) ) == 0) {
				TCHAR sz[30];
				wnsprintf( sz, ARRAYSIZE( sz ), _T( "%I64u" ), pReq->iRecvSize );
				pushstring( sz );
			} else if (lstrcmpi( pParam[i], _T( "/PERCENT" ) ) == 0) {
				pushint( RequestRecvPercent( pReq ) );
			} else if (lstrcmpi( pParam[i], _T( "/SPEEDBYTES" ) ) == 0) {
				pushint( pReq->Speed.iSpeed );
			} else if (lstrcmpi( pParam[i], _T( "/SPEED" ) ) == 0) {
				pushstring( pReq->Speed.szSpeed );
			} else if (lstrcmpi( pParam[i], _T( "/CONTENT" ) ) == 0) {
				RequestMemoryToString( pReq, psz, string_size );
				pushstring( psz );
			} else if (lstrcmpi( pParam[i], _T( "/DATA" ) ) == 0) {
				RequestDataToString( pReq, psz, string_size );
				pushstring( psz );
			} else if (lstrcmpi( pParam[i], _T( "/TIMEWAITING" ) ) == 0) {
				switch (pReq->iStatus) {
				case REQUEST_STATUS_WAITING:
				{
					FILETIME tmNow;
					GetLocalFileTime( &tmNow );
					pushint( MyTimeDiff( &tmNow, &pReq->tmEnqueue ) );
					break;
				}
				case REQUEST_STATUS_DOWNLOADING:
				case REQUEST_STATUS_DONE:
				{
					pushint( MyTimeDiff( &pReq->tmConnect, &pReq->tmEnqueue ) );
					break;
				}
				default:
					pushstring( _T( "" ) );
				}
			} else if (lstrcmpi( pParam[i], _T( "/TIMEDOWNLOADING" ) ) == 0) {
				switch (pReq->iStatus) {
				case REQUEST_STATUS_WAITING:
					pushstring( _T( "" ) );		/// No downloading time
					break;
				case REQUEST_STATUS_DOWNLOADING:
				{
					FILETIME tmNow;
					GetLocalFileTime( &tmNow );
					pushint( MyTimeDiff( &tmNow, &pReq->tmConnect ) );
					break;
				}
				case REQUEST_STATUS_DONE:
				{
					pushint( MyTimeDiff( &pReq->tmDisconnect, &pReq->tmConnect ) );
					break;
				}
				default:
					pushstring( _T( "" ) );
				}
			} else if (lstrcmpi( pParam[i], _T( "/CONNECTIONDROPS" ) ) == 0) {
				pushint( pReq->iConnectionDrops );
			} else if (lstrcmpi( pParam[i], _T( "/ERRORCODE" ) ) == 0) {
				pushint( pReq->iWin32Error == ERROR_SUCCESS ? pReq->iHttpStatus : pReq->iWin32Error );
			} else if (lstrcmpi( pParam[i], _T( "/ERRORTEXT" ) ) == 0) {
				pushstring( pReq->iWin32Error == ERROR_SUCCESS ? pReq->pszHttpStatus : pReq->pszWin32Error );
			} else {
				TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), pParam[i] );
				pushstring( _T( "" ) );
			}
		} else {
			TRACE( _T( "  [!] Transfer ID not found\n" ) );
			pushstring( _T( "" ) );
		}
		MyFree( pParam[i] );
	}

	/// Unlock
	QueueUnlock( &g_Queue );

	MyFree( psz );
}


//++ Set
EXTERN_C __declspec(dllexport)
void __cdecl Set(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	DWORD err = ERROR_SUCCESS;
	LPTSTR psz;

	ULONG iId = ANY_REQUEST_ID;		/// Selection parameter
	ULONG iPrio = ANY_PRIORITY;		/// Selection parameter

	ULONG iNewDependId = DEFAULT_VALUE;
	ULONG iNewPrio = DEFAULT_VALUE;
	BOOLEAN bRemove = FALSE;
	BOOLEAN bAbort = FALSE;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Set" ) );

	// Parameters
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	while (TRUE) {
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0) {
			break;
		} else if (lstrcmpi( psz, _T( "/ID" ) ) == 0) {
			iId = popint();
		} else if (lstrcmpi( psz, _T( "/PRIORITY" ) ) == 0) {
			iPrio = popint();
		} else if (lstrcmpi( psz, _T( "/SETPRIORITY" ) ) == 0) {
			iNewPrio = popint();
		} else if (lstrcmpi( psz, _T( "/SETDEPEND" ) ) == 0) {
			iNewDependId = popint();
		} else if (lstrcmpi( psz, _T( "/ABORT" ) ) == 0) {
			bAbort = TRUE;
		} else if (lstrcmpi( psz, _T( "/REMOVE" ) ) == 0) {
			bRemove = TRUE;
		} else {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}
	MyFree( psz );

	// Set
	if (TRUE) {

		int iThreadsToWake = 0;
		PQUEUE_REQUEST pReq;

		QueueLock( &g_Queue );
		for (pReq = g_Queue.pHead; pReq; pReq = pReq->pNext) {
			if (RequestMatched( pReq, iId, iPrio, ANY_STATUS )) {
				if (bRemove) {
					// Abort + Remove
					if (QueueAbort( &g_Queue, pReq, 10000 )) {
						if (QueueRemove( &g_Queue, pReq )) {
							///iRet++;
						}
					}
				} else if (bAbort) {
					// Abort
					if (QueueAbort( &g_Queue, pReq, 10000 )) {
						///iRet++;
					}
				} else {
					// Modify
					if (iNewPrio != DEFAULT_VALUE)
						pReq->iPriority = iNewPrio;
					if (iNewDependId != DEFAULT_VALUE) {
						pReq->iDependId = iNewDependId;
						iThreadsToWake++;
					}
				}
			}
		}
		QueueUnlock( &g_Queue );

		if (iThreadsToWake > 0)
			QueueWakeThreads( &g_Queue, iThreadsToWake );
	}

	pushint( err );
}


//++ Enumerate
EXTERN_C __declspec(dllexport)
void __cdecl Enumerate(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz;
	REQUEST_STATUS iStatus = ANY_STATUS;
	ULONG iPrio = ANY_PRIORITY;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Enumerate" ) );

	// Decide what requests to enumerate
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	while (TRUE) {
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0) {
			break;
		} else if (lstrcmpi( psz, _T( "/STATUS" ) ) == 0) {
			if (popstring( psz ) == 0) {
				if (lstrcmpi( psz, TEXT_STATUS_DOWNLOADING ) == 0) {
					iStatus = REQUEST_STATUS_DOWNLOADING;
				} else if (lstrcmpi( psz, TEXT_STATUS_WAITING ) == 0) {
					iStatus = REQUEST_STATUS_WAITING;
				} else if (lstrcmpi( psz, TEXT_STATUS_COMPLETED ) == 0) {
					iStatus = REQUEST_STATUS_DONE;
				//} else if (lstrcmpi( psz, _T( "paused" ) ) == 0) {
				//	iStatus = REQUEST_STATUS_PAUSED;
				}
			}
		} else if (lstrcmpi( psz, _T( "/PRIORITY" ) ) == 0) {
			iPrio = popint();
		} else {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}
	MyFree( psz );

	// Enumerate
	if (TRUE) {
		PQUEUE_REQUEST pReq;
		int iCount = 0;
		QueueLock( &g_Queue );
		for (pReq = g_Queue.pHead; pReq; pReq = pReq->pNext) {
			if (RequestMatched( pReq, ANY_REQUEST_ID, iPrio, iStatus )) {
				pushint( pReq->iId );
				iCount++;
			}
		}
		pushint( iCount );
		QueueUnlock( &g_Queue );
	}
}



//++ ParseWaitParameter
BOOL ParseWaitParameter(
	_In_ int string_size,
	_In_ LPTSTR psz,		/// Working buffer with the current parameter
	_In_ PGUI_WAIT_PARAM pParam
	)
{
	BOOL bRet = TRUE;		/// Assume that the current argument is valid
	assert( string_size && psz && pParam );

	if (lstrcmpi( psz, _T( "/ID" ) ) == 0) {
		pParam->iID = popint();
	} else if (lstrcmpi( psz, _T( "/PRIORITY" ) ) == 0) {
		pParam->iPriority = popint();
	} else if (lstrcmpi( psz, _T( "/MODE" ) ) == 0) {
		if (popstring( psz ) == 0) {
			if (lstrcmpi( psz, _T( "SILENT" ) ) == 0)
				pParam->iMode = GUI_MODE_SILENT;
			else if (lstrcmpi( psz, _T( "POPUP" ) ) == 0)
				pParam->iMode = GUI_MODE_POPUP;
			else if (lstrcmpi( psz, _T( "PAGE" ) ) == 0)
				pParam->iMode = GUI_MODE_PAGE;
			else {
				pParam->iMode = GUI_MODE_POPUP;		/// Default
				TRACE( _T( "  [!] Unknown GUI mode \"%s\"\n" ), psz );
			}
		}
	} else if (lstrcmpi( psz, _T( "/TITLEHWND" ) ) == 0) {
		pParam->hTitleWnd = (HWND)popintptr();
	} else if (lstrcmpi( psz, _T( "/STATUSHWND" ) ) == 0) {
		pParam->hStatusWnd = (HWND)popintptr();
	} else if (lstrcmpi( psz, _T( "/PROGRESSHWND" ) ) == 0) {
		pParam->hProgressWnd = (HWND)popintptr();
	} else if (lstrcmpi( psz, _T( "/TITLETEXT" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszTitleText );
			MyStrDup( pParam->pszTitleText, psz );
		}
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszTitleMultiText );
			MyStrDup( pParam->pszTitleMultiText, psz );
		}
	} else if (lstrcmpi( psz, _T( "/STATUSTEXT" ) ) == 0) {
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszStatusText );
			MyStrDup( pParam->pszStatusText, psz );
		}
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszStatusMultiText );
			MyStrDup( pParam->pszStatusMultiText, psz );
		}
	} else if (lstrcmpi( psz, _T( "/ABORT" ) ) == 0) {
		pParam->bAbort = TRUE;
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszAbortTitle );
			MyStrDup( pParam->pszAbortTitle, psz );
		}
		if (popstring( psz ) == 0) {
			MyFree( pParam->pszAbortMsg );
			MyStrDup( pParam->pszAbortMsg, psz );
		}
	} else {
		bRet = FALSE;	/// This parameter is not valid for Request
	}

	return bRet;
}


//++ Wait
EXTERN_C __declspec(dllexport)
void __cdecl Wait(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	INT_PTR iRet = 0;
	LPTSTR psz;
	GUI_WAIT_PARAM Param;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Wait" ) );

	/// Working buffer
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	assert( psz );

	/// Parameters
	GuiWaitParamInit( Param );
	for (;;)
	{
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0)
			break;

		if (!ParseWaitParameter( string_size, psz, &Param )) {
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}

	// Wait
	iRet = GuiWait( &Param );

	GuiWaitParamDestroy( Param );
	MyFree( psz );
	pushintptr( iRet );
}


//++ Transfer
EXTERN_C __declspec(dllexport)
void __cdecl Transfer(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR psz = NULL;
	QUEUE_REQUEST_PARAM ReqParam;
	GUI_WAIT_PARAM WaitParam;
	PQUEUE_REQUEST pReq = NULL;

	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Transfer" ) );

	/// Receive unloading notification
	extra->RegisterPluginCallback( g_hInst, NsisMessageCallback );

	/// Working buffer
	psz = (LPTSTR)MyAlloc( string_size * sizeof( TCHAR ) );
	assert( psz );

	/// Parameters
	RequestParamInit( ReqParam );
	GuiWaitParamInit( WaitParam );
	for (;;) {
		if (popstring( psz ) != 0)
			break;
		if (lstrcmpi( psz, _T( "/END" ) ) == 0)
			break;

		if (!ParseRequestParameter( string_size, psz, &ReqParam ) &&
			!ParseWaitParameter( string_size, psz, &WaitParam ))
		{
			TRACE( _T( "  [!] Unknown parameter \"%s\"\n" ), psz );
		}
	}

	// Add to the queue
	QueueLock( &g_Queue );
	QueueAdd( &g_Queue, &ReqParam, &pReq );
	QueueUnlock( &g_Queue );

	// Wait
	if (pReq) {
		WaitParam.iID = pReq->iId;
		WaitParam.iPriority = ANY_PRIORITY;
		GuiWait( &WaitParam );
	}

	// Return value
	if (pReq) {
		QueueLock( &g_Queue );
		if (pReq->iWin32Error == ERROR_SUCCESS) {
			if (pReq->iHttpStatus > 200 && pReq->iHttpStatus < 300)
				pushstring( _T( "OK" ) );			/// Convert any successful HTTP status to "OK"
			else
				pushstring( pReq->pszHttpStatus );	/// HTTP status
		} else {
			pushstring( pReq->pszWin32Error );	/// Win32 error
		}
		QueueUnlock( &g_Queue );
	} else {
		pushstring( _T( "Error" ) );
	}

	GuiWaitParamDestroy( WaitParam );
	RequestParamDestroy( ReqParam );
	MyFree( psz );
}


//++ Test
EXTERN_C __declspec(dllexport)
void __cdecl Test(
	HWND   parent,
	int    string_size,
	TCHAR   *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	EXDLL_INIT();
	EXDLL_VALIDATE();

	TRACE_CALL( stacktop, _T( "NSxfer!Test" ) );

	/*{
		ULONGLONG ull, ull2;
		double d;

		ull = 1234567890;
		d = MyUlonglongToDouble( ull );
		ull2 = MyDoubleToUlonglong( d );
		assert( ull == ull2 );
	}*/

	/*{
		ULONG i;
		FILETIME t1, t2;
		GetSystemTimeAsFileTime( &t1 );
		Sleep( 1234 );
		GetSystemTimeAsFileTime( &t2 );
		i = MyTimeDiff( &t2, &t1 );
		assert( i == 1234 );
	}*/
}


//++ _DllMainCRTStartup
EXTERN_C
BOOL WINAPI _DllMainCRTStartup(
	HMODULE hInst,
	UINT iReason,
	LPVOID lpReserved
	)
{
	if ( iReason == DLL_PROCESS_ATTACH ) {
		g_hInst = hInst;
		PluginInit();
	}
	else if ( iReason == DLL_PROCESS_DETACH ) {
		PluginUninit( TRUE );
	}
	return TRUE;
}
