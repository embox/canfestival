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

#include <stdio.h>

/* Definition of error and warning macros */
/* -------------------------------------- */
#define MSG(...) printf (__VA_ARGS__)

/* Definition of MSG_ERR */
/* --------------------- */
#ifdef DEBUG_ERR_CONSOLE_ON
#    define MSG_ERR(num, str, val)            \
          MSG("%s,%d : 0X%x %s 0X%x \n",__FILE__, __LINE__,num, str, val);
#else
#    define MSG_ERR(num, str, val)
#endif

/* Definition of MSG_WAR */
/* --------------------- */
#ifdef DEBUG_WAR_CONSOLE_ON
#    define MSG_WAR(num, str, val)          \
          MSG("%s,%d : 0X%x %s 0X%x \n",__FILE__, __LINE__,num, str, val);
#else
#    define MSG_WAR(num, str, val)
#endif

#endif /* __stackcfg_h__ */
