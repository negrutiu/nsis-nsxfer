#pragma once

#ifndef _DEBUG
	#if DBG || _DEBUG
		#define _DEBUG
	#endif
#endif

#define PLUGINNAME					_T( "NSxfer" )

#define COBJMACROS

#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0500
#endif
#ifndef _WIN32_IE
	#define _WIN32_IE    0x0600
#endif

#include <windows.h>
#include <wininet.h>
#include <Shlwapi.h>				/// for wvnsprintf, StrToInt64Ex
#include "nsiswapi/pluginapi.h"
#include <commctrl.h>
#include <Shobjidl.h>				/// ITaskbarList

extern HINSTANCE g_hInst;			/// Defined in main.c

#if _MSC_VER < 1600
	/// ...prior to Visual Studio 2010 (including WDK compilers)
	/// http://stackoverflow.com/questions/70013/how-to-detect-if-im-compiling-code-with-visual-studio-2008
	#pragma warning (push)
	#pragma warning (disable: 4005)
	#include "Tools\sal.h"
	#pragma warning (pop)
#endif
