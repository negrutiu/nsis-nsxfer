#pragma once

#if DBG || _DEBUG || DEBUG
	#define PLUGIN_DEBUG
#endif

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <wininet.h>
#ifdef PLUGIN_DEBUG
	#include <Shlwapi.h>			/// for wvnsprintf
#endif
#include "nsiswapi/pluginapi.h"

extern HINSTANCE g_hInst;			/// Defined in main.c
