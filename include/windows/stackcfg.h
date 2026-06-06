/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __stackcfg_h__
#define __stackcfg_h__

#include <windows.h>
#include <stdio.h>

/// Definition of error and warning macros
// --------------------------------------

#ifndef _MSC_VER
#define MSG(...) \
  do{printf(__VA_ARGS__);fflush(stdout);}while(0)
#elif (_MSC_VER >= 1400)
//Visual Studio 2005 and above
#ifdef UNICODE
#define MSG(...) \
  do{char msg[300];\
  sprintf(msg, __VA_ARGS__); \
   OutputDebugStringA(msg);}while(0)
#else
#define MSG(...) \
do{char msg[300];\
   sprintf(msg,##__VA_ARGS__);\
   OutputDebugString(msg);}while(0)
#endif
#else //(_MSC_VER < 1400)
//For Visual Studio 2003 and below, without VA_ARGS
#ifdef UNICODE
#define MSG(text) \
  do{wchar_t msg[300];\
   vswprintf(msg,L##text);\
   OutputDebugString(msg);}while(0)
#else
#define MSG(text) \
  do{printf text;fflush(stdout);}while(0)
#endif

#endif //_MSC_VER

#define CANFESTIVAL_DEBUG_MSG(num, str, val)\
  {unsigned long value = val;\
   MSG("%s(%d) : 0x%X %s 0x%lX\n",__FILE__, __LINE__,num, str, value); \
   }

#define CANFESTIVAL_DEBUG_DRV_MSG(...)\
  MSG(__VA_ARGS__);

/// Definition of MSG_WAR
// ---------------------
#ifdef DEBUG_WAR_CONSOLE_ON
    #define MSG_WAR(num, str, val) CANFESTIVAL_DEBUG_MSG(num, str, val)
#else
#    define MSG_WAR(num, str, val)
#endif

/// Definition of MSG_ERR
// ---------------------
#ifdef DEBUG_ERR_CONSOLE_ON
#    define MSG_ERR(num, str, val) CANFESTIVAL_DEBUG_MSG(num, str, val)
#else
#    define MSG_ERR(num, str, val)
#endif

#ifdef DEBUG_ERR_DRIVER_CONSOLE_ON
#    define MSG_ERR_DRV(...) CANFESTIVAL_DEBUG_DRV_MSG(__VA_ARGS__)
#else
#    define MSG_ERR_DRV(...)
#endif

#endif /* __stackcfg_h__ */
