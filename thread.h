#pragma once


typedef struct _THREAD {
	LPVOID pQueue;					/// The QUEUE that owns this thread
	HANDLE hThread;					/// Thread handle
	ULONG iTID;						/// Cached TID
	TCHAR szName[50];				/// Thread name. Ex: MAIN01
	HANDLE hWakeEvent;				/// Wake a thread when new items are added to the queue
	HANDLE hTermEvent;				/// Terminate threads
} THREAD, *PTHREAD;


// Thread function passed to CreateThread
DWORD WINAPI ThreadProc( _In_ PTHREAD pThread );
