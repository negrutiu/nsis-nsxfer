#pragma once


typedef struct _THREAD {
	HANDLE hThread;					/// Thread handle
	ULONG iTID;						/// Cached TID
	HANDLE hWakeEvent;				/// Wake the thread, usually when new items are enqueued
	HANDLE hTermEvent;				/// Terminate the thread
	TCHAR szName[50];				/// Thread name. "MAIN" by default
} THREAD, *PTHREAD;


// Initialize
DWORD ThreadInitialize( _Inout_ PTHREAD pThread );
DWORD ThreadDestroy( _Inout_ PTHREAD pThread, _In_opt_ ULONG iTimeout );		/// If iTimeout is zero, the function will wait forever

// Wake up
VOID ThreadWake( _Inout_ PTHREAD pThread );


// The worker thread
extern THREAD g_Thread;