#include "main.h"
#include "gui.h"
#include "queue.h"
#include "utils.h"
#include "resource.h"

#define GUI_TIMER_REFRESH_ID	1
#define GUI_TIMER_REFRESH_TIME	500
#define GUI_OUTPUT_STRING_LEN	1024
#define TEXT_NA					_T( "n/a" )
#define PLUGINNAME				_T( "NSxfer" )

#define DEFAULT_TITLE_SINGLE	_T("{PERCENT}% - Downloading...")
#define DEFAULT_TITLE_MULTI		_T("Downloading {TOTALCOUNT} files...")
#define DEFAULT_STATUS_SINGLE	_T("Received {RECVSIZE}/{FILESIZE} @ {SPEED}, ETA: {TIMEREMAINING}")
#define DEFAULT_STATUS_MULTI	_T("Downloading {TOTALACTIVE}/{TOTALCOUNT} files. Received {TOTALRECVSIZE} @ {TOTALSPEED}")

#define MY_PBS_MARQUEE			0x08
#define MY_PBM_SETMARQUEE		(WM_USER+10)

#define LTWH(rc)				(rc).left, (rc).top, (rc).right - (rc).left, (rc).bottom - (rc).top

extern QUEUE g_Queue;		/// main.c

struct {
	UINT iTransferID;
	ULONG iPriority;
	GUI_MODE iMode;
	HWND hTaskbarWnd;
	HWND hTitleWnd;
	HWND hStatusWnd;
	HWND hProgressWnd;

	BOOLEAN bAbort;
	LPCTSTR pszAbortTitle;
	LPCTSTR pszAbortMsg;

	LPCTSTR pszTitleText;
	LPCTSTR pszTitleMultiText;

	LPCTSTR pszStatusText;
	LPCTSTR pszStatusMultiText;

	HICON hPopupIco;

	/// Runtime data
	BOOLEAN bFinished;		/// The transfers we're waiting for have all completed
	PQUEUE_ITEM pItem;		/// Valid when a single transfer is in progress, or the wait is performed on a specific transfer ID
	LONG iThreadCount;
	LONG iItemsTotal;
	LONG iItemsDone;
	LONG iItemsDownloading;
	LONG iItemsWaiting;
	ULONG64 iRecvSize;
	LONG iItemsSpeed;

	LPCTSTR pszOriginalTitleText;
	LPCTSTR pszOriginalStatusText;

	BOOL bRestoreProgressParams;
	LONG_PTR iOriginalProgressStyle;
	LONG_PTR iOriginalProgressStyleEx;
	PBRANGE OriginalProgressRange;
	int iOriginalProgressPos;

	BOOL bOriginalCancelEnabled;

	BOOLEAN bNsisAborted;
	WNDPROC fnOriginalNsisWndProc;

	/// Output strings
	ULONG iAnimationStep;
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

				szNewValue[0] = 255;	/// Special character
				szNewValue[1] = 0;

				// Item-specific keywords
				if (g_Gui.pItem) {
					if (IS_KEYWORD( pszKeywordStart, _T( "ID" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pItem->iId );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Status" ))) {
						switch (g_Gui.pItem->iStatus) {
						case ITEM_STATUS_WAITING: lstrcpyn( szNewValue, TEXT_STATUS_WAITING, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_STATUS_DOWNLOADING: lstrcpyn( szNewValue, TEXT_STATUS_DOWNLOADING, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_STATUS_DONE: lstrcpyn( szNewValue, TEXT_STATUS_COMPLETED, ARRAYSIZE( szNewValue ) ); break;
						default: assert( !"Unknown item status" );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "WininetStatus" ))) {
						if (g_Gui.pItem->iStatus != ITEM_STATUS_WAITING) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pItem->iLastCallbackStatus );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Method" ))) {
						lstrcpyn( szNewValue, g_Gui.pItem->szMethod, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "URL" ))) {
						lstrcpyn( szNewValue, g_Gui.pItem->pszURL, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "IP" ))) {
						lstrcpyn( szNewValue, g_Gui.pItem->pszSrvIP ? g_Gui.pItem->pszSrvIP : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Proxy" ))) {
						lstrcpyn( szNewValue, g_Gui.pItem->pszProxy ? g_Gui.pItem->pszProxy : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Local" ))) {
						switch (g_Gui.pItem->iLocalType) {
						case ITEM_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_LOCAL_FILE: lstrcpyn( szNewValue, g_Gui.pItem->Local.pszFile, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						default: assert( !"Unknown local type" );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "LocalFileDir" ))) {
						switch (g_Gui.pItem->iLocalType) {
						case ITEM_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_LOCAL_FILE: {
							LPTSTR psz, pszBkSlash;
							for (psz = pszBkSlash = g_Gui.pItem->Local.pszFile; *psz; psz++)
								if (*psz == _T( '\\' ))
									pszBkSlash = psz + 1;
							lstrcpyn( szNewValue, g_Gui.pItem->Local.pszFile, __min( ARRAYSIZE( szNewValue ), (ULONG)(pszBkSlash - g_Gui.pItem->Local.pszFile) ) );
							break;
						}
						case ITEM_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "LocalFileName" ))) {
						switch (g_Gui.pItem->iLocalType) {
						case ITEM_LOCAL_NONE: lstrcpyn( szNewValue, TEXT_LOCAL_NONE, ARRAYSIZE( szNewValue ) ); break;
						case ITEM_LOCAL_FILE: {
							LPTSTR psz, pszFname;
							for (psz = pszFname = g_Gui.pItem->Local.pszFile; *psz; psz++)
								if (*psz == _T( '\\' ))
									pszFname = psz + 1;
							lstrcpyn( szNewValue, pszFname, ARRAYSIZE( szNewValue ) );
							break;
						}
						case ITEM_LOCAL_MEMORY: lstrcpyn( szNewValue, TEXT_LOCAL_MEMORY, ARRAYSIZE( szNewValue ) ); break;
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "FileSize" ))) {
						if (g_Gui.pItem->iFileSize != INVALID_FILE_SIZE64) {
							StrFormatByteSize( g_Gui.pItem->iFileSize, szNewValue, ARRAYSIZE( szNewValue ) );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "FileSizeBytes" ))) {
						if (g_Gui.pItem->iFileSize != INVALID_FILE_SIZE64) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.pItem->iFileSize );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "RecvSize" ))) {
						StrFormatByteSize( g_Gui.pItem->iRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "RecvSizeBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.pItem->iRecvSize );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Percent" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%hu" ), (USHORT)ItemGetRecvPercent( g_Gui.pItem ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "Speed" ))) {
						lstrcpyn( szNewValue, *g_Gui.pItem->Speed.szSpeed ? g_Gui.pItem->Speed.szSpeed : TEXT_NA, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "SpeedBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.pItem->Speed.iSpeed );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TimeStart" ))) {
						if (g_Gui.pItem->iStatus != ITEM_STATUS_WAITING) {
							SYSTEMTIME st;
							FileTimeToSystemTime( &g_Gui.pItem->tmConnect, &st );
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
						if (g_Gui.pItem->iStatus != ITEM_STATUS_WAITING) {
							FILETIME tmNow;
							GetLocalFileTime( &tmNow );
							StrFromTimeInterval( szNewValue, ARRAYSIZE( szNewValue ), MyTimeDiff( &tmNow, &g_Gui.pItem->tmConnect ), 3 );
							StrTrim( szNewValue, _T( " " ) );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TimeRemaining" ))) {
						if (g_Gui.pItem->iStatus != ITEM_STATUS_WAITING) {
							if (g_Gui.pItem->iFileSize != INVALID_FILE_SIZE64) {
								ULONG iElapsedMs;
								FILETIME tmNow;
								GetLocalFileTime( &tmNow );
								iElapsedMs = MyTimeDiff( &tmNow, &g_Gui.pItem->tmConnect );
								iElapsedMs = __max( iElapsedMs, GUI_TIMER_REFRESH_TIME );
								if ((g_Gui.pItem->iRecvSize > 0) && (iElapsedMs >= 3000)) {
									ULONG iRemainingMs = (ULONG)MyMulDiv64( iElapsedMs, g_Gui.pItem->iFileSize, g_Gui.pItem->iRecvSize ) - iElapsedMs;
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
				if (szNewValue[0] == 255) {
					if (IS_KEYWORD( pszKeywordStart, _T( "TotalCount" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsTotal );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalWaiting" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsWaiting );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalActive" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsDownloading + g_Gui.iItemsDone );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalDownloading" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsDownloading );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalCompleted" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsDone );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalRecvSize" ))) {
						StrFormatByteSize( g_Gui.iRecvSize, szNewValue, ARRAYSIZE( szNewValue ) );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalRecvSizeBytes" ))) {
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%I64u" ), g_Gui.iRecvSize );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalSpeed" ))) {
						if (g_Gui.iItemsDownloading > 0) {
							StrFormatByteSize( g_Gui.iItemsSpeed, szNewValue, ARRAYSIZE( szNewValue ) );
							lstrcat( szNewValue, _T( "/s" ) );
						} else {
							lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );
						}
					} else if (IS_KEYWORD( pszKeywordStart, _T( "TotalSpeedBytes" ))) {
						if (g_Gui.iItemsDownloading > 0) {
							wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%u" ), g_Gui.iItemsSpeed );
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
						lstrcpyn( szNewValue, PLUGINNAME, ARRAYSIZE( szNewValue ) );	/// TODO
					} else if (IS_KEYWORD( pszKeywordStart, _T( "PluginVersion" ))) {
						lstrcpyn( szNewValue, TEXT_NA, ARRAYSIZE( szNewValue ) );		/// TODO
					} else if (IS_KEYWORD( pszKeywordStart, _T( "AnimLine" ))) {
						int i = iAnimationStep % 4;
						TCHAR szFrames[] = _T( "-\\|/" );
						wnsprintf( szNewValue, ARRAYSIZE( szNewValue ), _T( "%c" ), szFrames[i] );
					} else if (IS_KEYWORD( pszKeywordStart, _T( "AnimDots" ))) {
						int i, n;
						for (i = 0, n = iAnimationStep % 4; i < n; i++)
							szNewValue[i] = _T( '.' );
						szNewValue[i] = _T( '\0' );
					} else {
						/// Unrecognized keyword
						///StringCchCopyN( szNewValue, ARRAYSIZE( szNewValue ), pszKeywordStart, pszKeywordEnd - pszKeywordStart + 1 );
						len = __min( ARRAYSIZE( szNewValue ), pszKeywordEnd - pszKeywordStart + 1 );
						lstrcpyn( szNewValue, pszKeywordStart, len + 1 );
					}
				}
				
				if (szNewValue[0] == 255)
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


ULONG GuiRefreshData()
{
	ULONG err = ERROR_SUCCESS;
	PQUEUE_ITEM p;

	// Refresh transfer data
	QueueLock( &g_Queue );

	g_Gui.iThreadCount = g_Queue.iThreadCount;
	g_Gui.iItemsTotal = 0;
	g_Gui.iItemsDone = 0;
	g_Gui.iItemsDownloading = 0;
	g_Gui.iItemsWaiting = 0;
	g_Gui.iRecvSize = 0;
	g_Gui.iItemsSpeed = 0;

	for (p = g_Queue.pHead; p; p = p->pNext) {
		if (ItemMatched( p, g_Gui.iTransferID, g_Gui.iPriority, ANY_STATUS )) {
			if (p->iStatus == ITEM_STATUS_DOWNLOADING) {
				g_Gui.pItem = p;	/// Remember the last in-progress transfer
			}
			g_Gui.iRecvSize += p->iRecvSize;
			g_Gui.iItemsSpeed += p->Speed.iSpeed;

			g_Gui.iItemsTotal++;
			if (p->iStatus == ITEM_STATUS_DONE)
				g_Gui.iItemsDone++;
			if (p->iStatus == ITEM_STATUS_DOWNLOADING)
				g_Gui.iItemsDownloading++;
			if (p->iStatus == ITEM_STATUS_WAITING)
				g_Gui.iItemsWaiting++;
		}
	}
	if (g_Gui.iTransferID == ANY_TRANSFER_ID && g_Gui.iItemsDownloading > 1) {
		g_Gui.pItem = NULL;			/// Wait for multiple transfers
	}

	// All done?
	if (g_Gui.pItem) {
		g_Gui.bFinished = (g_Gui.pItem->iStatus == ITEM_STATUS_DONE);
	} else {
		g_Gui.bFinished = (g_Gui.iItemsDownloading == 0 && g_Gui.iItemsWaiting == 0);
	}

	// Title text
	if (g_Gui.hTitleWnd) {
		if (g_Gui.pItem) {
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
		if (g_Gui.pItem) {
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
		if (!g_Gui.pItem || !g_Gui.pItem->bConnected) {

			// Indeterminate progress
			if (!(iStyle & MY_PBS_MARQUEE)) {
				SetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE, iStyle | MY_PBS_MARQUEE );
				SendMessage( g_Gui.hProgressWnd, MY_PBM_SETMARQUEE, TRUE, 0 );

				if (g_Gui.hTaskbarWnd && g_Gui.pTaskbarList3)
					ITaskbarList3_SetProgressState( g_Gui.pTaskbarList3, g_Gui.hTaskbarWnd, TBPF_INDETERMINATE );
			}

		} else {

			int iProgress = ItemGetRecvPercent( g_Gui.pItem );
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


VOID GuiWaitAbort()
{
	PQUEUE_ITEM p;
	QueueLock( &g_Queue );
	for (p = g_Queue.pHead; p; p = p->pNext) {
		if (ItemMatched( p, g_Gui.iTransferID, g_Gui.iPriority, ANY_STATUS )) {
			QueueAbort( &g_Queue, p );
		}
	}
	QueueUnlock( &g_Queue );
}


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
			if (g_Gui.bAbort && (!g_Gui.pszAbortMsg || !*g_Gui.pszAbortMsg || MessageBox( hDlg, g_Gui.pszAbortMsg, g_Gui.pszAbortTitle, MB_YESNO | MB_ICONQUESTION ) == IDYES)) {
				GuiWaitAbort();
				EndDialog( hDlg, IDCANCEL );
			}
			return 0;
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
			/// Abort button
			if (g_Gui.bAbort && (!g_Gui.pszAbortMsg || !*g_Gui.pszAbortMsg || MessageBox( g_hwndparent, g_Gui.pszAbortMsg, g_Gui.pszAbortTitle, MB_YESNO | MB_ICONQUESTION ) == IDYES))
				g_Gui.bNsisAborted = TRUE;
			return 0;
		}
		break;
	}
	return CallWindowProc( g_Gui.fnOriginalNsisWndProc, hwnd, uMsg, wParam, lParam );
}


ULONG GuiWaitPage()
{
	ULONG err = ERROR_SUCCESS;
	BOOL bCustom = g_Gui.hTitleWnd || g_Gui.hStatusWnd || g_Gui.hProgressWnd;

	/// Original InstFiles page controls
	HWND hInstFilesPage = NULL, hStatus = NULL, hProgress = NULL, hDetailsBtn = NULL, hDetailsList = NULL;
	RECT rcStatus, rcProgress, rcDetailsBtn, rcDetailsList;
	LONG_PTR iStatusStyle, iStatusStyleEx;
	LONG_PTR iProgressStyle, iProgressStyleEx;
	HWND hCancelBtn = NULL;

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

				/// Cancel button
				hCancelBtn = GetDlgItem( g_hwndparent, IDCANCEL );
				if (hCancelBtn) {
					g_Gui.bOriginalCancelEnabled = IsWindowEnabled( hCancelBtn );
					if (g_Gui.bAbort) {
						EnableWindow( hCancelBtn, TRUE );
						/// Hook main NSIS window (Cancel's parent) to receive click notification
						g_Gui.fnOriginalNsisWndProc = (WNDPROC)SetWindowLongPtr( g_hwndparent, GWLP_WNDPROC, (LONG_PTR)GuiWaitPageWindowProc );
					} else {
						EnableWindow( hCancelBtn, FALSE );
					}
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
				g_Gui.hTitleWnd = NULL;
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
	}

	// Wait
	if (err == ERROR_SUCCESS) {
		while (!g_Gui.bNsisAborted && (GuiRefreshData() == ERROR_SUCCESS) && !g_Gui.bFinished)
			GuiSleep( GUI_TIMER_REFRESH_TIME );
		if (g_Gui.bNsisAborted)
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
		if (hCancelBtn) {
			EnableWindow( hCancelBtn, g_Gui.bOriginalCancelEnabled );
			if (g_Gui.fnOriginalNsisWndProc) {
				/// Destroy child dialogs (such as the aborting confirmation message box)
				EnumThreadWindows( GetWindowThreadProcessId( g_hwndparent, NULL ), GuiEndChildDialogCallback, (LPARAM)g_hwndparent );
				/// Process pending messages
				///GuiSleep( 0 );
				/// Unhook NSIS main window
				SetWindowLongPtr( g_hwndparent, GWLP_WNDPROC, (LONG_PTR)g_Gui.fnOriginalNsisWndProc );
			}
		}
	}

	return err;
}


ULONG GuiWait(
	__in UINT iTransferID,
	__in ULONG iPriority,
	__in GUI_MODE iMode,
	__in_opt HWND hTitleWnd,
	__in_opt HWND hStatusWnd,
	__in_opt HWND hProgressWnd,
	__in_opt LPCTSTR pszTitleText,
	__in_opt LPCTSTR pszTitleMultiText,
	__in_opt LPCTSTR pszStatusText,
	__in_opt LPCTSTR pszStatusMultiText,
	__in_opt BOOLEAN bAbort,
	__in_opt LPCTSTR pszAbortTitle,
	__in_opt LPCTSTR pszAbortMsg
	)
{
	ULONG err = ERROR_SUCCESS;

	MyZeroMemory( &g_Gui, sizeof( g_Gui ) );
	g_Gui.iTransferID = iTransferID;
	g_Gui.iPriority = iPriority;
	g_Gui.iMode = iMode;
	g_Gui.hTitleWnd = hTitleWnd;
	g_Gui.hStatusWnd = hStatusWnd;
	g_Gui.hProgressWnd = hProgressWnd;
	g_Gui.pszTitleText = pszTitleText ? pszTitleText : DEFAULT_TITLE_SINGLE;
	g_Gui.pszTitleMultiText = pszTitleMultiText ? pszTitleMultiText : DEFAULT_TITLE_MULTI;
	g_Gui.pszStatusText = pszStatusText ? pszStatusText : DEFAULT_STATUS_SINGLE;
	g_Gui.pszStatusMultiText = pszStatusMultiText ? pszStatusMultiText : DEFAULT_STATUS_MULTI;
	g_Gui.bAbort = bAbort;
	g_Gui.pszAbortTitle = pszAbortTitle && *pszAbortTitle ? pszAbortTitle : PLUGINNAME;
	g_Gui.pszAbortMsg = pszAbortMsg;

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
		g_Gui.bRestoreProgressParams = TRUE;
	}

	// Start
	switch (iMode) {
	case GUI_MODE_SILENT:
		err = GuiWaitSilent();
		break;
	case GUI_MODE_POPUP:
		err = GuiWaitPopup();
		break;
	case GUI_MODE_PAGE:
		err = GuiWaitPage();
		if (err == ERROR_NOT_SUPPORTED)
			err = GuiWaitPopup();
		break;
	default:
		err = ERROR_INVALID_PARAMETER;
	}

	/// Restore original values
	if (g_Gui.hTitleWnd && g_Gui.pszOriginalTitleText)
		SetWindowText( g_Gui.hTitleWnd, g_Gui.pszOriginalTitleText );
	if (g_Gui.hTitleWnd && g_Gui.pszOriginalStatusText)
		SetWindowText( g_Gui.hStatusWnd, g_Gui.pszOriginalStatusText );
	if (g_Gui.hProgressWnd && g_Gui.bRestoreProgressParams) {
		SetWindowLongPtr( g_Gui.hProgressWnd, GWL_STYLE, g_Gui.iOriginalProgressStyle );
		SetWindowLongPtr( g_Gui.hProgressWnd, GWL_EXSTYLE, g_Gui.iOriginalProgressStyleEx );
		SendMessage( g_Gui.hProgressWnd, PBM_SETRANGE32, g_Gui.OriginalProgressRange.iLow, g_Gui.OriginalProgressRange.iHigh );
		SendMessage( g_Gui.hProgressWnd, PBM_SETPOS, g_Gui.iOriginalProgressPos, 0 );
	}

	MyFree( g_Gui.pszOriginalTitleText );
	MyFree( g_Gui.pszOriginalStatusText );
	return err;
}
