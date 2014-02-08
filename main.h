#pragma once

#ifndef _DEBUG
	#if DBG || DEBUG
		#define _DEBUG
	#endif
#endif

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <wininet.h>
#ifdef _DEBUG
	#include <Shlwapi.h>			/// for wvnsprintf
#endif
#include "nsiswapi/pluginapi.h"

extern HINSTANCE g_hInst;			/// Defined in main.c
