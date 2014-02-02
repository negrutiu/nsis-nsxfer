#pragma once

//+ TRACE
#ifdef PLUGIN_DEBUG
VOID TRACE( __in LPCTSTR pszFormat, ... );
#else
#define TRACE(...)
#endif

//+ Memory
#define MyAlloc(_size)		GlobalLock(GlobalAlloc(GMEM_FIXED, _size))
#define MyFree(_ptr)		{ GlobalFree((HGLOBAL)(_ptr)); (_ptr) = NULL; }
