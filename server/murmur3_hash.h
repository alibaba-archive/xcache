//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef _X_MURMURHASH3_H_
#define _X_MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros
#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------

uint32_t MurmurHash3_x86_32(const void *key, size_t length);

//-----------------------------------------------------------------------------

#endif // MURMURHASH3_H
