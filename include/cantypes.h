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

#ifndef __cantypes_h__
#define __cantypes_h__

#include <stdint.h>

/* Integers */
#define INTEGER8  int8_t
#define INTEGER16 int16_t
#define INTEGER24 int32_t
#define INTEGER32 int32_t
#define INTEGER40 int64_t
#define INTEGER48 int64_t
#define INTEGER56 int64_t
#define INTEGER64 int64_t

/* Unsigned integers */
#define UNS8   uint8_t
#define UNS16  uint16_t
#define UNS24  uint32_t
#define UNS32  uint32_t
#define UNS40  uint64_t
#define UNS48  uint64_t
#define UNS56  uint64_t
#define UNS64  uint64_t

/* Reals */
#define REAL32 float
#define REAL64 double

typedef void* CAN_HANDLE;

typedef void* CAN_PORT;

#endif /* __cantypes_h__ */
