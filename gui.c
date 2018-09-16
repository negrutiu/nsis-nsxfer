
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2014/10/25

#include "main.h"
#include "gui.h"
#include "queue.h"
#include "utils.h"
#include "resource.h"
#include "Tools\ITaskbarList.h"

#define GUI_TIMER_REFRESH_ID	1
#define GUI_TIMER_REFRESH_TIME	500
#define GUI_OUTPUT_STRING_LEN	1024
#define TEXT_NA					_T( "n/a" )
#define WM_ABORT_CLOSED			WM_USER + 0x266

#define DEFAULT_TITLE_SINGLE	_T("{PERCENT}% - Downloading...")
#define DEFAULT_TITLE_MULTI		_T("Downloading {TOTALCOUNT} files...")
#define DEFAULT_STATUS_SINGLE	_T("Downloading {RECVSIZE}/{FILESIZE} @ {SPEED}, ETA: {TIMEREMAINING}")
#define DEFAULT_STATUS_MULTI	_T("Downloading {TOTALACTIVE}/{TOTALCOUNT} files. Received {TOTALRECVSIZE} @ {TOTALSPEED}")

#define MY_PBS_MARQUEE			0x08
#define MY_PBM_SETMARQUEE		(WM_USER+10)

#define LTWH(rc)				(rc).left, (rc).top, (rc).right - (rc).left, (rc).bottom - (rc).top

extern QUEUE g_Queue;			/// main.c

/// Prototypes
void GuiAbortShow( __in HWND hParent, __in HWND hCallbackWnd, __in UINT iCallbackMsg );


struct {

	/// Input
	UINT iID;
	ULONG iPriority;
	GUI_MODE iMode;
	HWND hTitleWnd;
	HWND hStatusWnd;
	HWND hProgressWnd;

	BOOLEAN bAbort;			/// Abortion is supported
	LPCTSTR pszAbortTitle;
	LPCTSTR pszAbortMsg;

	LPCTSTR pszTitleText;
	LPCTSTR pszTitleMultiText;

	LPCTSTR pszStatusText;
	LPCTSTR pszStatusMultiText;


	/// Runtime
	HWND hTaskbarWnd;
	HICON hPopupIco;

	BOOLEAN bFinished;		/// All transfers finished
	PQUEUE_REQUEST pReq;	/// Available when a single transfer request is in progress
	LONG iThreadCount;
	LONG iTotalCount;
	LONG iTotalDone;
	LONG iTotalDownloading;
	LONG iTotalWaiting;
	ULONG64 iTotalRecvSize;
	LONG iTotalSpeed;

	LPCTSTR pszOriginalTitleText;
	LPCTSTR pszOriginalStatusText;

	BOOL bRestoreProgressStyle;
	LONG_PTR iOriginalProgressStyle;
	LONG_PTR iOriginalProgressStyleEx;
	BOOL bRestoreProgressPos;
	PBRANGE OriginalProgressRange;
	int iOriginalProgressPos;

	BOOLEAN bAborted;		/// The transfer was aborted. Abortion is in progress...
	HWND hAbortWnd;
	BOOL bOriginalAbortEnabled;
	WNDPROC fnOriginalAbortParentWndProc;

	/// Output strings
	ULONG iAnimationStep;
	TCHAR szPluginVer[128];
	TCHAR szTitle[GUI_OUTPUT_STRING_LEN];
	TCHAR szStatus[GUI_OUTPUT_STRING_LEN];

	/// Taskbar
	ITaskbarList3* pTaskbarList3;
} g_Gui;


//++ GuiExpandKeywords
void GuiExpandKeywords(
	__in LPCTSTR pszTextFmt,
	__inout LPTSTR pszTextOut,
	__in size_t iTextOutLen,
	__in int iAnimationStep			/// Auto incremented tick count. Required for keeping all animations in sync...
	)
{
#define FIND_KEYWORD_START(pszStart) \
	for ( ; *(pszStart) != _T( '\0' ) && *(pszStart) != _T( '{' ); (pszStart)++ );

#define FIND_KEYWORD_END(pszStart, pszEnd) \
	for ( (pszEnd) = (pszStart); *((pszEnd) + 1) != _T( '\0' ) && *(pszEnd) != _T( '}' ) && *((pszEnd) + 1) != _T( '{' ); (pszEnd)++ );

#define IS_KEYWORD(pszStart, pszKeyword)\
	CompareString( 0, NORM_IGNORECASE, pszStart, (sizeof(pszKeyword) - 1) / sizeof(TCHAR) + 2, _T( "{" ) pszKeyword _T( "}" ), (sizeof(pszKeyword) - 1) / sizeof(TCHAR) + 2 ) == CSTR_EQUAL


	if (pszTextOut && iTextOutLen)
		pszTextOut[0] = _T( '\0' );

	if (pszTextFmt && *pszTextFmt && pszTextOut && iTextOutLen) {

		LPCTSTR pszKeywordStart, pszKeywordEnd, pszPreviousKeywordEnd;
		TCHAR szNewValue[512];
		size_t len;

		pszKeywordStart = pszTextFmt;
		pszPreviousKeywordEnd = pszTextFmt - 1;
		while (*pszKeywordStart) {

			FIND_KEYWORD_START( pszKeywordStart );
			FIND_KEYWORD_END( pszKeywordStart, pszKeywordEnd );

			///StringCchCopyNEx( pszTextOut, iTextOutLen, pszPreviousKeywordEnd + 1, (pszKeywordStart - pszPreviousKeywordEnd - 1), &pszTextOut, &iTextOutLen, 0 );
			len = pszKeywordStart - pszPreviousKeywordEnd - 1;
			len = __min( len, iTextOutLen );
			if (lstrcpyn( pszTextOut, pszPreviousKeywordEnd + 1, len + 1 ))
				pszTextOut += len, iTextOutLen -= len;

			if (*pszKeywordStart) {

				#define MARKER_CHAR (TCHAR)255

				szNewValue[0] = MARKER_CHAR;	/// Special character
				szNewValue[1] = 0;

				// Keywords
				if (g_Gui.pReq) {
					if (IS_KEYWORD( pszKeywordStart, _T( "ID" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pReq->iId );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Status" ))) {
						switch (g_Gui.pReq->iStatus) {
						case REQUEST_STATUS_WAITING: lstrcpyn( szNewValue, TEXT_STATUS_WAITING, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_STATUS_DOWNLOADING: lstrcpyn( szNewValue, TEXT_STATUS_DOWNLOADING, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_STATUS_DONE: lstrcpyn( szNewValue, TEXT_STATUS_COMPLETED, ARRAYSIZE( szNewValue ) ); break;
						default: assert( !"Unknown request status" );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "WininetStatus" ))) {
						if (g_Gui.pReq->iStatus != REQUEST_STATUS_WAITING) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pReq->iLastCallbackStatus );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Method" ))) {
						lstrcpyn( szNewValue, g_Gui.pReq->szMethod, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "URL" ))) {
						lstrcpyn( szNewValue, g_Gui.pReq->pszURL, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "IP" ))) {
						lstrcpyn( szNewValue, g_Gui.pReq->pszSrvIP ? g_Gui.pReq->pszSrvIP : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Proxy" ))) {
						lstrcpyn( szNewValue, g_Gui.pReq->pszProxy ? g_Gui.pReq->pszProxy : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Local" ))) {
						switch (g_Gui.pReq->iLocalType) {
						case REQUEST_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_LOCAL_FILE: lstrcpyn( szNewValue, g_Gui.pReq->Local.pszFile, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						default: assert( !"Unknown local type" );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "LocalFileDir" ))) {
						switch (g_Gui.pReq->iLocalType) {
						case REQUEST_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_LOCAL_FILE: {
							LPTSTR psz, pszBkSlash;
							for (psz = pszBkSlash = g_Gui.pReq->Local.pszFile; *psz; psz++)
								if (*psz == _T( '\\' ))
									pszBkSlash = psz + 1;
							lstrcpyn( szNewValue, g_Gui.pReq->Local.pszFile, __min( ARRAYSIZE( szNewValue ), (ULONG)(pszBkSlash - g_Gui.pReq->Local.pszFile) ) );
							break;
						}
						case REQUEST_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "LocalFileName" ))) {
						switch (g_Gui.pReq->iLocalType) {
						case REQUEST_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case REQUEST_LOCAL_FILE: {
							LPTSTR psz, pszFname;
							for (psz = pszFname = g_Gui.pReq->Local.pszFile; *psz; psz++)
								if (*psz == _T( '\\' ))
									pszFname = psz + 1;
							lstrcpyn( szNewValue, pszFname, ARRAYSIZE( szNewValue ) );
							break;
						}
						case REQUEST_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "FileSize" ))) {
						if (g_Gui.pReq->iFileSize != INVALID_FILE_SIZE64) {
#ifdef UNICODE
							StrFormatByteSizeW( g_Gui.pReq->iFileSize, szNewValue, ARRAYSIZE( szNewValue ) );
#else
							StrFormatByteSizeA( (ULONG)g_Gui.pReq->iFileSize, szNewValue, ARRAYSIZE( szNewValue ) );
#endif
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "FileSizeBytes" ))) {
						if (g_Gui.pReq->iFileSize != INVALID_FILE_SIZE64) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.pReq->iFileSize );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "RecvSize" ))) {
#ifdef UNICODE
						StrFormatByteSizeW( g_Gui.pReq->iRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
#else
						StrFormatByteSizeA( (ULONG)g_Gui.pReq->iRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
#endif
					} else if (IS_KEYWORD( pszKeywordStart, _T( "RecvSizeBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.pReq->iRecvSize );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Percent" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%hu" ), (USHORT)RequestRecvPercent( g_Gui.pReq ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Speed" ))) {
						lstrcpyn( szNewValue, *g_Gui.pReq->Speed.szSpeed ? g_Gui.pReq->Speed.szSpeed : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "SpeedBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pReq->Speed.iSpeed );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TimeStart" ))) {
						if (g_Gui.pReq->iStatus != REQUEST_STATUS_WAITING) {
							SYSTEMTIME st;
							FileTimeToSystemTime( &g_Gui.pReq->tmConnect, &st );
							wnsprintf(
								szNewValue, ARRAYSIZE( szNewValue ),
								_T( "%u/%02hu/%02hu %02hu:%02hu:%02hu" ),
								st.wYear, st.wMonth, st.wDay,
								st.wHour, st.wMinute, st.wSecond
								);
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TimeElapsed" ))) {
						if (g_Gui.pReq->iStatus != REQUEST_STATUS_WAITING) {
							FILETIME tmNow;
							GetLocalFileTime( &tmNow );
							StrFromTimeInterval( szNewValue, ARRAYSIZE( szNewValue ), MyTimeDiff( &tmNow, &g_Gui.pReq->tmConnect ), 3 );
							StrTrim( szNewValue, _T( " " ) );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TimeRemaining" ))) {
						if (g_Gui.pReq->iStatus != REQUEST_STATUS_WAITING) {
							if (g_Gui.pReq->iFileSize != INVALID_FILE_SIZE64) {
								ULONG iElapsedMs;
								FILETIME tmNow;
								GetLocalFileTime( &tmNow );
								iElapsedMs = MyTimeDiff( &tmNow, &g_Gui.pReq->tmConnect );
								iElapsedMs = __max( iElapsedMs, GUI_TIMER_REFRESH_TIME );
								if ((g_Gui.pReq->iRecvSize > 0) && (iElapsedMs >= 3000)) {
									ULONG iRemainingMs = (ULONG)MyMulDiv64( iElapsedMs, g_Gui.pReq->iFileSize, g_Gui.pReq->iRecvSize ) - iElapsedMs;
									StrFromTimeInterval( szNewValue, ARRAYSIZE( szNewValue ), iRemainingMs, 3 );
									StrTrim( szNewValue, _T( " " ) );
								} else {
									lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
								}
							} else {
								lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
							}
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					}
				}

				// General keywords
				if (szNewValue[0] == MARKER_CHAR) {
					if (IS_KEYWORD( pszKeywordStart, _T( "TotalCount" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalCount );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalWaiting" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalWaiting );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalActive" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalDownloading + g_Gui.iTotalDone );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalDownloading" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalDownloading );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalCompleted" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalDone );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalRecvSize" ))) {
#ifdef UNICODE
						StrFormatByteSizeW( g_Gui.iTotalRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
#else
						StrFormatByteSizeA( (ULONG)g_Gui.iTotalRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
#endif
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalRecvSizeBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.iTotalRecvSize );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalSpeed" ))) {
						if (g_Gui.iTotalDownloading > 0) {
#ifdef UNICODE
							StrFormatByteSizeW( g_Gui.iTotalSpeed, szNewValue, ARRAYSIZE( szNewValue ) );
#else
							StrFormatByteSizeA( (ULONG)g_Gui.iTotalSpeed, szNewValue, ARRAYSIZE( szNewValue ) );
#endif
							lstrcat( szNewValue, TEXT_PER_SECOND );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalSpeedBytes" ))) {
						if (g_Gui.iTotalDownloading > 0) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iTotalSpeed );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalThreads" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iThreadCount );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "OriginalTitle" ))) {
						lstrcpyn( szNewValue, g_Gui.pszOriginalTitleText ? g_Gui.pszOriginalTitleText : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "OriginalStatus" ))) {
						lstrcpyn( szNewValue, g_Gui.pszOriginalStatusText ? g_Gui.pszOriginalStatusText : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "PluginName" ))) {
						lstrcpyn( szNewValue, PLUGINNAME, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "PluginVersion" ))) {
						if (!*g_Gui.szPluginVer) {
							TCHAR szPath[MAX_PATH];
							if (GetModuleFileName( g_hInst, szPath, ARRAYSIZE( szPath ) ) > 0)
								ReadVersionInfoString( szPath, _T( "FileVersion" ), g_Gui.szPluginVer, ARRAYSIZE( g_Gui.szPluginVer ) );
						}
						lstrcpyn( szNewValue, *g_Gui.szPluginVer ? g_Gui.szPluginVer : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "AnimLine" ))) {
						int i = iAnimationStep % 4;
						TCHAR szFrames[] = _T( "-\\|/" );
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%c" ), szFrames[i] );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "AnimDots" ))) {
						int i, n;
						for (i = 0, n = iAnimationStep % 4; i < n; i++)
							if (i < n)	/// Trick: We'll add this (useless) condition inside the 'for' loop, to prevent the compiler from replacing the entire loop with a _memset call (happens when building directly with cl.exe, using ANSI code page)
								szNewValue[i] = _T( '.' );
						szNewValue[i] = _T( '\0' );
					} else {
						/// Unrecognized keyword
						///StringCchCopyN( szNewValue, ARRAYSIZE( szNewValue ), pszKeywordStart, pszKeywordEnd - pszKeywordStart + 1 );
						len = __min( ARRAYSIZE( szNewValue ), pszKeywordEnd - pszKeywordStart + 1 );
						lstrcpyn( szNewValue, pszKeywordStart, len + 1 );
					}
				}
				
				if (szNewValue[0] == MARKER_CHAR)
					szNewValue[0] = 0;

				///StringCchCopyEx( pszTextOut, iTextOutLen, szNewValue, &pszTextOut, &iTextOutLen, 0 );
				len = lstrlen( szNewValue );
				len = __min( len, iTextOutLen );
				if (lstrcpyn( pszTextOut, szNewValue, len + 1 ))
					pszTextOut += len, iTextOutLen -= len;

				pszPreviousKeywordEnd = pszKeywordEnd;
				pszKeywordStart = pszKeywordEnd + 1;
			}
		}
	}
}


//++ GuiRefreshData
ULONG GuiRefreshData()
{
	ULONG err = ERROR_SUCCESS;
	PQUEUE_REQUEST p;

	// Refresh transfer data
	QueueLock( &g_Queue );

	g_Gui.iThreadCount = g_Queue.iThreadCount;
	g_Gui.iTotalCount = 0;
	g_Gui.iTotalDone = 0;
	g_Gui.iTotalDownloading = 0;
	g_Gui.iTotalWaiting = 0;
	g_Gui.iTotalRecvSize = 0;
	g_Gui.iTotalSpeed = 0;

	for (p = g_Queue.pHead; p; p = p->pNext) {
		if (RequestMatched( p, g_Gui.iID, g_Gui.iPriority, ANY_STATUS )) {
			if (p->iStatus == REQUEST_STATUS_DOWNLOADING) {
				g_Gui.pReq = p;		/// Remember the last request in progress
			}
			g_Gui.iTotalRecvSize += p->iRecvSize;
			g_Gui.iTotalSpeed += p->Speed.iSpeed;

			g_Gui.iTotalCount++;
			if (p->iStatus == REQUEST_STATUS_DONE)
				g_Gui.iTotalDone++;
			if (p->iStatus == REQUEST_STATUS_DOWNLOADING)
				g_Gui.iTotalDownloading++;
			if (p->iStatus == REQUEST_STATUS_WAITING)
				g_Gui.iTotalWaiting++;
		}
	}
	if (g_Gui.iID == ANY_REQUEST_ID && g_Gui.iTotalDownloading > 1) {
		g_Gui.pReq = NULL;			/// Wait for multiple requests
	}

	// All done?
	if (g_Gui.pReq) {
		g_Gui.bFinished = (g_Gui.pReq->iStatus == REQUEST_STATUS_DONE);
	} else {
		g_Gui.bFinished = (g_Gui.iTotalDownloading == 0 && g_Gui.iTotalWaiting == 0);
	}

	// Title text
	if (g_Gui.hTitleWnd) {
		if (g_Gui.pReq) {
			/// Single
			GuiExpandKeywords( g_Gui.pszTitleText, g_Gui.szTitle, ARRAYSIZE( g_Gui.szTitle ), g_Gui.iAnimationStep );
		} else {
			/// Multi
			GuiExpandKeywords( g_Gui.pszTitleMultiText, g_Gui.szTitle, ARRAYSIZE( g_Gui.szTitle ), g_Gui.iAnimationStep );
		}
		SetWindowText( g_Gui.hTitleWnd, g_Gui.szTitle );
	}

	// Status text
	if (g_Gui.hStatusWnd) {
		if (g_Gui.pReq) {
			/// Single
			GuiExpandKeywords( g_Gui.pszStatusText, g_Gui.szStatus, ARRAYSIZE( g_Gui.szStatus ), g_Gui.iAnimationStep );
		} else {
			/// Multi
			GuiExpandKeywords( g_Gui.pszStatusMultiText, g_Gui.szStatus, ARRAYSIZE( g_Gui.szStatus ), g_Gui.iAnimationStep );
		}
		SetWindowText( g_Gui.hStatusWnd, g_Gui.szStatus );
	}

	// Progress bar
	if (g_Gui.hProgressWnd) {

		LONG_PTR iStyle = GetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE );
		if (!g_Gui.pReq || !g_Gui.pReq->bConnected) {

			// Indeterminate progress
			if (!(iStyle & MY_PBS_MARQUEE)) {
				SetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE, iStyle | MY_PBS_MARQUEE );
				SendMessage( g_Gui.hProgressWnd, MY_PBM_SETMARQUEE, TRUE, 0 );

				if (g_Gui.hTaskbarWnd && g_Gui.pTaskbarList3)
					ITaskbarList3_SetProgressState( g_Gui.pTaskbarList3, g_Gui.hTaskbarWnd, TBPF_INDETERMINATE );
			}

		} else {

			int iProgress = RequestRecvPercent( g_Gui.pReq );
			if (iStyle & MY_PBS_MARQUEE) {
				SetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE, iStyle & ~MY_PBS_MARQUEE );
				SendMessage( g_Gui.hProgressWnd, MY_PBM_SETMARQUEE, FALSE, 0 );
				///if (g_Gui.hTaskbarWnd && g_Gui.pTaskbarList3)
				///	ITaskbarList3_SetProgressState( g_Gui.pTaskbarList3, g_Gui.hTaskbarWnd, TBPF_NORMAL );
			}
			SendMessage( g_Gui.hProgressWnd, PBM_SETPOS, iProgress, 0 );

			if (g_Gui.hTaskbarWnd && g_Gui.pTaskbarList3)
				ITaskbarList3_SetProgressValue( g_Gui.pTaskbarList3, g_Gui.hTaskbarWnd, iProgress, 100 );
		}
	}

	// Animation
	g_Gui.iAnimationStep++;

	QueueUnlock( &g_Queue );
	return err;
}


//++ GuiWaitAbort
VOID GuiWaitAbort()
{
	PQUEUE_REQUEST p;
	QueueLock( &g_Queue );
	for (p = g_Queue.pHead; p; p = p->pNext) {
		if (RequestMatched( p, g_Gui.iID, g_Gui.iPriority, ANY_STATUS )) {
			QueueAbort( &g_Queue, p, 10000 );
		}
	}
	QueueUnlock( &g_Queue );
}


//++ GuiEndChildDialogCallback
BOOL CALLBACK GuiEndChildDialogCallback( __in HWND hwnd, __in LPARAM lParam )
{
	HWND hRootOwner = (HWND)lParam;

	if (!(GetWindowLongPtr( hwnd, GWL_STYLE ) & WS_CHILDWINDOW)) {
		HWND hOwner = GetWindow( hwnd, GW_OWNER );
		if (hOwner == hRootOwner) {
			TCHAR szClass[50];
			GetClassName( hwnd, szClass, ARRAYSIZE( szClass ) );
			if (lstrcmpi( szClass, _T( "#32770" ) ) == 0) {
				BOOL b = EndDialog( hwnd, IDCANCEL );
				TRACE( _T( "  Th:GUI EndDialog( 0x%p \"%s\", IDCANCEL ) == %u\n" ), hwnd, szClass, b );
			} else {
				TRACE( _T( "  Th:GUI SkipChild( 0x%p \"%s\" )\n" ), hwnd, szClass );
			}
		}
	}
	return TRUE;
}


//++ GuiSleep
VOID GuiSleep( _In_ ULONG iMillisec )
{
	MSG msg;
	DWORD dwTime = GetTickCount();
	while (TRUE) {
		while (PeekMessage( &msg, NULL, 0, 0, PM_REMOVE )) {
			if (!IsDialogMessage( g_hwndparent, &msg )) {
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
		}
		if (GetTickCount() - dwTime < iMillisec) {
			Sleep( 50 );
		} else {
			break;
		}
	}
}


/*
	SILENT mode
*/
ULONG GuiWaitSilent()
{
	while ((GuiRefreshData() == ERROR_SUCCESS) && !g_Gui.bFinished)
		GuiSleep( GUI_TIMER_REFRESH_TIME );
	return ERROR_SUCCESS;
}


/*
	POPUP mode
*/
INT_PTR CALLBACK GuiWaitPopupDialogProc( _In_ HWND hDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam )
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		HRESULT hr;

		// Add WS_EX_APPWINDOW. We want our popup window to appear on the taskbar...
		SetWindowLongPtr( hDlg, GWL_EXSTYLE, GetWindowLongPtr( hDlg, GWL_EXSTYLE ) | WS_EX_APPWINDOW );

		// Window handles
		g_Gui.hTaskbarWnd = hDlg;
		g_Gui.hTitleWnd = hDlg;
		g_Gui.hStatusWnd = GetDlgItem( hDlg, IDC_POPUP_STATUS );
		g_Gui.hProgressWnd = GetDlgItem( hDlg, IDC_POPUP_PROGRESS );

		// Abortion
		if (g_Gui.bAbort) {
			EnableMenuItem( GetSystemMenu( hDlg, FALSE ), SC_CLOSE, MF_BYCOMMAND | MF_ENABLED );
		} else {
			EnableMenuItem( GetSystemMenu( hDlg, FALSE ), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED );
		}

		// Icon
		g_Gui.hPopupIco = LoadImage( GetModuleHandle( NULL ), MAKEINTRESOURCE( 103 ), IMAGE_ICON, 32, 32, 0 );
		if (g_Gui.hPopupIco)
			SendDlgItemMessage( hDlg, IDC_POPUP_ICON, STM_SETICON, (WPARAM)g_Gui.hPopupIco, 0 );

		// Taskbar button
		hr = CoCreateInstance( &CLSID_TaskbarList, NULL, CLSCTX_ALL, &IID_ITaskbarList3, (void**)&g_Gui.pTaskbarList3 );
		if (SUCCEEDED( hr )) {
			ITaskbarList3_HrInit( g_Gui.pTaskbarList3 );
		}

		// Timer
		SetTimer( hDlg, GUI_TIMER_REFRESH_ID, GUI_TIMER_REFRESH_TIME, NULL );
		SendMessage( hDlg, WM_TIMER, GUI_TIMER_REFRESH_ID, 0 );		/// First shot

		return TRUE;	/// Focus (HWND)wParam
	}

	case WM_DESTROY:

		// Timer
		KillTimer( hDlg, GUI_TIMER_REFRESH_ID );

		// Taskbar button
		if (g_Gui.pTaskbarList3) {
			ITaskbarList3_Release( g_Gui.pTaskbarList3 );
			g_Gui.pTaskbarList3 = NULL;
		}

		// Cleanup
		if (g_Gui.hPopupIco) {
			DestroyIcon( g_Gui.hPopupIco );
			g_Gui.hPopupIco = NULL;
		}
		g_Gui.hTaskbarWnd = NULL;
		g_Gui.hTitleWnd = NULL;
		g_Gui.hStatusWnd = NULL;
		g_Gui.hProgressWnd = NULL;
		break;

	case WM_TIMER:
		if (wParam == GUI_TIMER_REFRESH_ID) {
			GuiRefreshData();
			if (g_Gui.bFinished) {
				/// Destroy child dialogs (such as the aborting confirmation message box)
				EnumThreadWindows( GetWindowThreadProcessId( hDlg, NULL ), GuiEndChildDialogCallback, (LPARAM)hDlg );
				EndDialog( hDlg, IDOK );
			}
		}
		break;

	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE) {
			/// [X] button
			/// Display confirmation dialog, if necessary
			/// Later, we'll receive a message with the user's decision
			GuiAbortShow( hDlg, hDlg, WM_ABORT_CLOSED );
			return 0;
		}
		break;

	case WM_ABORT_CLOSED:
		/// The abort was closed
		/// Apply user's decision
		if (wParam != FALSE) {
			GuiWaitAbort();
			EndDialog( hDlg, IDCANCEL );
		}
		break;
	}

	return FALSE;	/// Default dialog procedure
}

ULONG GuiWaitPopup()
{
	DialogBox( g_hInst, MAKEINTRESOURCE( IDD_POPUP ), g_hwndparent, GuiWaitPopupDialogProc );
	return GetLastError();
}


/*
	PAGE mode
*/
LRESULT CALLBACK GuiWaitPageWindowProc( _In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam )
{
	switch (uMsg) {
	case WM_COMMAND:
		if (LOWORD( wParam ) == IDCANCEL) {
			/// Display confirmation dialog, if necessary
			/// Later, we'll receive a message with the user's decision
			GuiAbortShow( g_hwndparent, g_hwndparent, WM_ABORT_CLOSED );
			return 0;
		}
		break;

	case WM_ABORT_CLOSED:
		/// The abort was closed
		/// Apply user's decision
		if (wParam != FALSE)
			g_Gui.bAborted = TRUE;
		break;
	}
	return CallWindowProc( g_Gui.fnOriginalAbortParentWndProc, hwnd, uMsg, wParam, lParam );
}


ULONG GuiWaitPage()
{
	ULONG err = ERROR_SUCCESS;
	BOOL bCustom = /*g_Gui.hTitleWnd ||*/ g_Gui.hStatusWnd || g_Gui.hProgressWnd;

	/// Original InstFiles page controls
	HWND hInstFilesPage = NULL, hStatus = NULL, hProgress = NULL, hDetailsBtn = NULL, hDetailsList = NULL;
	RECT rcStatus, rcProgress, rcDetailsBtn, rcDetailsList;
	LONG_PTR iStatusStyle, iStatusStyleEx;
	LONG_PTR iProgressStyle, iProgressStyleEx;
	HWND hAbortBtn = NULL;

	/// New controls
	HWND hNewStatus = NULL, hNewProgress = NULL;
	RECT rcNewStatus, rcNewProgress;

	int iDetailsOffsetY;

	// Controls
	if (!bCustom) {
		hInstFilesPage = FindWindowEx( g_hwndparent, NULL, _T( "#32770" ), NULL );
		if (hInstFilesPage) {

			/// Status and Progress controls must exist
			hStatus = GetDlgItem( hInstFilesPage, 1006 );
			hProgress = GetDlgItem( hInstFilesPage, 1004 );
			if (hStatus && hProgress) {

				/// InstFiles page status control
				GetWindowRect( hStatus, &rcStatus );
				ScreenToClient( hInstFilesPage, (LPPOINT)&rcStatus.left );
				ScreenToClient( hInstFilesPage, (LPPOINT)&rcStatus.right );
				iStatusStyle = GetWindowLongPtr( hStatus, GWL_STYLE );
				iStatusStyleEx = GetWindowLongPtr( hStatus, GWL_EXSTYLE );

				/// InstFiles page progress bar
				GetWindowRect( hProgress, &rcProgress );
				ScreenToClient( hInstFilesPage, (LPPOINT)&rcProgress.left );
				ScreenToClient( hInstFilesPage, (LPPOINT)&rcProgress.right );
				iProgressStyle = GetWindowLongPtr( hProgress, GWL_STYLE );
				iProgressStyleEx = GetWindowLongPtr( hProgress, GWL_EXSTYLE );

				/// InstFiles page details button
				hDetailsBtn = GetDlgItem( hInstFilesPage, 1027 );
				if (hDetailsBtn) {
					GetWindowRect( hDetailsBtn, &rcDetailsBtn );
					ScreenToClient( hInstFilesPage, (LPPOINT)&rcDetailsBtn.left );
					ScreenToClient( hInstFilesPage, (LPPOINT)&rcDetailsBtn.right );
				}

				/// InstFiles page details list
				hDetailsList = GetDlgItem( hInstFilesPage, 1016 );
				if (hDetailsList) {
					GetWindowRect( hDetailsList, &rcDetailsList );
					ScreenToClient( hInstFilesPage, (LPPOINT)&rcDetailsList.left );
					ScreenToClient( hInstFilesPage, (LPPOINT)&rcDetailsList.right );
				}

				/// New status control
				CopyRect( &rcNewStatus, &rcStatus );
				OffsetRect( &rcNewStatus, 0, rcProgress.bottom + rcProgress.top - rcNewStatus.top );
				hNewStatus = CreateWindowEx( iStatusStyleEx, _T( "STATIC" ), _T( "" ), iStatusStyle, LTWH( rcNewStatus ), hInstFilesPage, NULL, NULL, NULL );
				SendMessage( hNewStatus, WM_SETFONT, (WPARAM)SendMessage( hStatus, WM_GETFONT, 0, 0 ), MAKELPARAM( FALSE, 0 ) );

				/// New progress bar
				CopyRect( &rcNewProgress, &rcProgress );
				OffsetRect( &rcNewProgress, 0, rcNewStatus.bottom + (rcStatus.bottom - rcProgress.top) - rcNewProgress.top );
				hNewProgress = CreateWindowEx( iProgressStyleEx, PROGRESS_CLASS, _T( "" ), iProgressStyle, LTWH( rcNewProgress ), hInstFilesPage, NULL, NULL, NULL );

				iDetailsOffsetY = rcNewProgress.bottom + (rcDetailsList.top - rcProgress.bottom) - rcDetailsBtn.top;

				/// Move details button
				if (hDetailsBtn) {
					OffsetRect( &rcDetailsBtn, 0, iDetailsOffsetY );
					SetWindowPos( hDetailsBtn, NULL, LTWH( rcDetailsBtn ), SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME );
				}

				/// Move details list
				if (hDetailsList) {
					rcDetailsList.top += iDetailsOffsetY;
					SetWindowPos( hDetailsList, NULL, LTWH( rcDetailsList ), SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME );
				}

				/// Use the new controls
				g_Gui.hStatusWnd = hNewStatus;
				g_Gui.hProgressWnd = hNewProgress;

			} else {
				/// Status and/or Progress controls not found
				err = ERROR_NOT_SUPPORTED;
			}
		} else {
			/// Page not found
			err = ERROR_NOT_SUPPORTED;
		}
	} else {
		/// Custom page
		g_Gui.bRestoreProgressStyle = TRUE;
		g_Gui.bRestoreProgressPos = FALSE;
	}

	/// Abort button
	if (err == ERROR_SUCCESS) {
		hAbortBtn = GetDlgItem( g_hwndparent, IDCANCEL );
		if (hAbortBtn) {
			g_Gui.bOriginalAbortEnabled = IsWindowEnabled( hAbortBtn );
			if (g_Gui.bAbort) {
				EnableWindow( hAbortBtn, TRUE );
				/// Hook Abort's parent (main NSIS window) to receive click notification
				g_Gui.fnOriginalAbortParentWndProc = (WNDPROC)SetWindowLongPtr( g_hwndparent, GWLP_WNDPROC, (LONG_PTR)GuiWaitPageWindowProc );
			} else {
				EnableWindow( hAbortBtn, FALSE );
			}
		}
	}

	// Wait
	if (err == ERROR_SUCCESS) {
		while (!g_Gui.bAborted && (GuiRefreshData() == ERROR_SUCCESS) && !g_Gui.bFinished)
			GuiSleep( GUI_TIMER_REFRESH_TIME );
		if (g_Gui.bAborted)
			GuiWaitAbort();
	}

	// Cleanup
	if (!bCustom) {
		if (hInstFilesPage) {
			if (hNewStatus)
				DestroyWindow( hNewStatus );
			if (hNewProgress)
				DestroyWindow( hNewProgress );
			if (hDetailsBtn) {
				OffsetRect( &rcDetailsBtn, 0, -iDetailsOffsetY );
				SetWindowPos( hDetailsBtn, NULL, LTWH( rcDetailsBtn ), SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME );
			}
			if (hDetailsList) {
				rcDetailsList.top -= iDetailsOffsetY;
				SetWindowPos( hDetailsList, NULL, LTWH( rcDetailsList ), SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME );
			}
		}
	}

	if (hAbortBtn) {
		EnableWindow( hAbortBtn, g_Gui.bOriginalAbortEnabled );
		if (g_Gui.fnOriginalAbortParentWndProc) {
			/// Destroy child dialogs (such as the aborting confirmation message box)
			EnumThreadWindows( GetWindowThreadProcessId( g_hwndparent, NULL ), GuiEndChildDialogCallback, (LPARAM)g_hwndparent );
			/// Process pending messages
			///GuiSleep( 0 );
			/// Unhook NSIS main window
			SetWindowLongPtr( g_hwndparent, GWLP_WNDPROC, (LONG_PTR)g_Gui.fnOriginalAbortParentWndProc );
		}
	}

	return err;
}


ULONG GuiWait( __in PGUI_WAIT_PARAM pParam )
{
	ULONG err = ERROR_SUCCESS;
	assert( pParam );

	MyZeroMemory( &g_Gui, sizeof( g_Gui ) );
	g_Gui.iID = pParam->iID;
	g_Gui.iPriority = pParam->iPriority;
	g_Gui.iMode = pParam->iMode;
	g_Gui.hTitleWnd = pParam->hTitleWnd;
	g_Gui.hStatusWnd = pParam->hStatusWnd;
	g_Gui.hProgressWnd = pParam->hProgressWnd;
	g_Gui.pszTitleText = pParam->pszTitleText && *pParam->pszTitleText ? pParam->pszTitleText : DEFAULT_TITLE_SINGLE;
	g_Gui.pszTitleMultiText = pParam->pszTitleMultiText && *pParam->pszTitleMultiText ? pParam->pszTitleMultiText : DEFAULT_TITLE_MULTI;
	g_Gui.pszStatusText = pParam->pszStatusText && *pParam->pszStatusText ? pParam->pszStatusText : DEFAULT_STATUS_SINGLE;
	g_Gui.pszStatusMultiText = pParam->pszStatusMultiText && *pParam->pszStatusMultiText ? pParam->pszStatusMultiText : DEFAULT_STATUS_MULTI;
	g_Gui.bAbort = pParam->bAbort;
	g_Gui.pszAbortTitle = pParam->pszAbortTitle && *pParam->pszAbortTitle ? pParam->pszAbortTitle : PLUGINNAME;
	g_Gui.pszAbortMsg = pParam->pszAbortMsg;

	/// Remember original values
	if (g_Gui.hTitleWnd) {
		TCHAR sz[256];
		GetWindowText( g_Gui.hTitleWnd, sz, ARRAYSIZE( sz ) );
		MyStrDup( g_Gui.pszOriginalTitleText, sz );
	}

	if (g_Gui.hStatusWnd) {
		TCHAR sz[256];
		GetWindowText( g_Gui.hStatusWnd, sz, ARRAYSIZE( sz ) );
		MyStrDup( g_Gui.pszOriginalStatusText, sz );
	}

	if (g_Gui.hProgressWnd) {
		g_Gui.iOriginalProgressStyle = GetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE );
		g_Gui.iOriginalProgressStyleEx = GetWindowLongPtr( g_Gui.hProgressWnd, GWL_EXSTYLE );
		SendMessage( g_Gui.hProgressWnd, PBM_GETRANGE, 0, (WPARAM)&g_Gui.OriginalProgressRange );
		g_Gui.iOriginalProgressPos = SendMessage( g_Gui.hProgressWnd, PBM_GETPOS, 0, 0 );
		g_Gui.bRestoreProgressStyle = TRUE;
		g_Gui.bRestoreProgressPos = TRUE;
	}

	// Start
	switch (pParam->iMode) {
	case GUI_MODE_SILENT:
		err = GuiWaitSilent();
		break;
	case GUI_MODE_POPUP:
		err = GuiWaitPopup();
		break;
	case GUI_MODE_PAGE:
		err = GuiWaitPage();
		if (err == ERROR_NOT_SUPPORTED)
			err = GuiWaitSilent();
		break;
	default:
		err = ERROR_INVALID_PARAMETER;
	}

	/// Restore original values
	if (g_Gui.hTitleWnd && g_Gui.pszOriginalTitleText)
		SetWindowText( g_Gui.hTitleWnd, g_Gui.pszOriginalTitleText );
	if (g_Gui.hTitleWnd && g_Gui.pszOriginalStatusText)
		SetWindowText( g_Gui.hStatusWnd, g_Gui.pszOriginalStatusText );
	if (g_Gui.hProgressWnd && g_Gui.bRestoreProgressStyle) {
		if (GetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE ) != g_Gui.iOriginalProgressStyle)		/// Don't set identical style, because it'll reset the position
			SetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE, g_Gui.iOriginalProgressStyle );
		if (GetWindowLongPtr( g_Gui.hProgressWnd, GWL_EXSTYLE ) != g_Gui.iOriginalProgressStyleEx)
			SetWindowLongPtr( g_Gui.hProgressWnd, GWL_EXSTYLE, g_Gui.iOriginalProgressStyleEx );
	}
	if (g_Gui.hProgressWnd && g_Gui.bRestoreProgressPos) {
		SendMessage( g_Gui.hProgressWnd, PBM_SETRANGE32, g_Gui.OriginalProgressRange.iLow, g_Gui.OriginalProgressRange.iHigh );
		SendMessage( g_Gui.hProgressWnd, PBM_SETPOS, g_Gui.iOriginalProgressPos, 0 );
	}

	MyFree( g_Gui.pszOriginalTitleText );
	MyFree( g_Gui.pszOriginalStatusText );
	return err;
}


INT_PTR CALLBACK GuiAbortDialogProc( _In_ HWND hDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam )
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		TCHAR szText[128];
		HMODULE hUser32 = GetModuleHandle( _T( "user32" ) );

		/// Icon
		HICON hIco = LoadImage( hUser32, MAKEINTRESOURCE( 102 ), IMAGE_ICON, 32, 32, 0 );
		assert( hIco );
		if (hIco) {
			SendDlgItemMessage( hDlg, IDC_POPUP_ICON, STM_SETICON, (WPARAM)hIco, 0 );
			SetProp( hDlg, _T( "MyIcon" ), hIco );
		}

		/// Text
		if (g_Gui.pszAbortTitle)
			SetWindowText( hDlg, g_Gui.pszAbortTitle );
		if (g_Gui.pszAbortMsg)
			SetDlgItemText( hDlg, IDC_STATIC_TEXT, g_Gui.pszAbortMsg );

		if (LoadString( hUser32, 805, szText, ARRAYSIZE( szText ) ) > 0)
			SetDlgItemText( hDlg, IDYES, szText );
		if (LoadString( hUser32, 806, szText, ARRAYSIZE( szText ) ) > 0)
			SetDlgItemText( hDlg, IDNO, szText );
		break;
	}

	case WM_DESTROY:
	{
		HICON hIco = (HICON)GetProp( hDlg, _T( "MyIcon" ) );
		if (hIco) {
			RemoveProp( hDlg, _T( "MyIcon" ) );
			DestroyIcon( hIco );
		}
		if (TRUE) {
			HWND hCallbackWnd = (HWND)GetProp( hDlg, _T( "CallbackWnd" ) );
			UINT iCallbackMsg = HandleToULong( GetProp( hDlg, _T( "CallbackMsg" ) ) );
			BOOL bAnswerYes = (BOOL)HandleToUlong( GetProp( hDlg, _T( "AnswerYes" ) ) );
			RemoveProp( hDlg, _T( "CallbackWnd" ) );
			RemoveProp( hDlg, _T( "CallbackMsg" ) );
			RemoveProp( hDlg, _T( "AnswerYes" ) );
			if (hCallbackWnd)
				PostMessage( hCallbackWnd, iCallbackMsg, (WPARAM)bAnswerYes, 0 );
		}
		g_Gui.hAbortWnd = NULL;
		break;
	}

	case WM_COMMAND:
	{
		switch (LOWORD( wParam ))
		{
		case IDYES:
			SetProp( hDlg, _T( "AnswerYes" ), (HANDLE)TRUE );
			DestroyWindow( hDlg );
			break;

		case IDNO:
		case IDCANCEL:
			DestroyWindow( hDlg );
			break;
		}
		break;
	}

	case WM_SYSCOMMAND:
	{
		if (wParam == SC_CLOSE)
			DestroyWindow( hDlg );
		break;
	}
	}

	return FALSE;		/// Default dialog procedure
}


void GuiAbortShow( __in HWND hParent, __in HWND hCallbackWnd, __in UINT iCallbackMsg )
{
	if (g_Gui.bAbort) {		/// Abortion permitted?
		if (!g_Gui.bAborted) {	/// Already aborted?
			if (g_Gui.pszAbortMsg && *g_Gui.pszAbortMsg) {	/// Abortion message available?
				if (!g_Gui.hAbortWnd) {		/// Confirmation dialog already visible?
					g_Gui.hAbortWnd = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_CONFIRMATION ), hParent, GuiAbortDialogProc, 0 );
					if (g_Gui.hAbortWnd) {
						SetProp( g_Gui.hAbortWnd, _T( "CallbackWnd" ), (HANDLE)hCallbackWnd );
						SetProp( g_Gui.hAbortWnd, _T( "CallbackMsg" ), ULongToHandle( iCallbackMsg ) );
						ShowWindow( g_Gui.hAbortWnd, SW_SHOW );
					}
				}
			} else {
				/// No message is available. Abort without warning
				if (hCallbackWnd)
					PostMessage( hCallbackWnd, iCallbackMsg, (WPARAM)TRUE, 0 );
			}
		}
	}
}
