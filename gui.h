#pragma once

typedef enum {
	GUI_MODE_SILENT,
	GUI_MODE_POPUP,
	GUI_MODE_PAGE
} GUI_MODE;


/// Returns Win32 error code
ULONG GuiWait(
	__in UINT iTransferID,					/// Can be ANY_TRANSFER_ID
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
	);