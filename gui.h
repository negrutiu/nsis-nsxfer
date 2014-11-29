#pragma once

typedef enum {
	GUI_MODE_SILENT,
	GUI_MODE_POPUP,
	GUI_MODE_PAGE
} GUI_MODE;


typedef struct _GUI_WAIT_PARAM {
	UINT iID;							/// Can be ANY_REQUEST_ID
	ULONG iPriority;					/// Can be ANY_PRIORITY
	GUI_MODE iMode;
	HWND hTitleWnd;						/// Can be NULL
	HWND hStatusWnd;					/// Can be NULL
	HWND hProgressWnd;					/// Can be NULL
	LPCTSTR pszTitleText;				/// Can be NULL
	LPCTSTR pszTitleMultiText;			/// Can be NULL
	LPCTSTR pszStatusText;				/// Can be NULL
	LPCTSTR pszStatusMultiText;			/// Can be NULL
	BOOLEAN bAbort;
	LPCTSTR pszAbortTitle;				/// Can be NULL
	LPCTSTR pszAbortMsg;				/// Can be NULL
} GUI_WAIT_PARAM, *PGUI_WAIT_PARAM;

#define GuiWaitParamInit(Wait) \
	MyZeroMemory( &Wait, sizeof( Wait ) ); \
	Wait.iID = ANY_REQUEST_ID; \
	Wait.iPriority = ANY_PRIORITY; \
	Wait.iMode = GUI_MODE_PAGE;

#define GuiWaitParamDestroy(Wait) \
	MyFree( Wait.pszTitleText ); \
	MyFree( Wait.pszTitleMultiText ); \
	MyFree( Wait.pszStatusText ); \
	MyFree( Wait.pszStatusMultiText ); \
	MyFree( Wait.pszAbortTitle ); \
	MyFree( Wait.pszAbortMsg ); \
	MyZeroMemory( &Wait, sizeof( Wait ) );


/// Returns Win32 error code
ULONG GuiWait(
	__in PGUI_WAIT_PARAM pParam
	);
