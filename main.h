#pragma once

#ifndef _DEBUG
	#if DBG || _DEBUG
		#define _DEBUG
	#endif
#endif

#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <wininet.h>
#include <Shlwapi.h>				/// for wvnsprintf
#include "nsiswapi/pluginapi.h"

extern HINSTANCE g_hInst;			/// Defined in main.c
