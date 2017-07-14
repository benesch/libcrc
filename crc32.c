// Copyright 2017 The Cockroach Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations under
// the License.
//
// Portions of this work are derived from the hash/crc32 implementation in the
// Go Programming Language and are thus additionally subject to the terms of the
// Go license in LICENSE-GO.
//
// Author: Nikhil Benesch (nikhil.benesch@gmail.com)

#include "include/crc32.h"

#include <cpuid.h>
#include <stdbool.h>
#include <stdio.h>

static const size_t slicing_cutoff = 16;

static const uint32_t poly_ieee = 0xedb88320;

static uint32_t table_ieee[8][256];

crc32_fn_t *crc32ieee = NULL;

static uint32_t
crc32ieee_slow(uint32_t crc, const char* buf, size_t len) {
	// Slicing by eight.
	if (len >= slicing_cutoff) {
		crc = ~crc;
		while (len > 8) {
			crc ^= buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
			crc = table_ieee[0][buf[7]] ^ table_ieee[1][buf[6]] ^
			      table_ieee[2][buf[5]] ^ table_ieee[3][buf[4]] ^
			      table_ieee[4][crc >> 24] ^ table_ieee[5][(crc >> 16) & 0xff] ^
			      table_ieee[6][(crc >> 8) & 0xff] ^ table_ieee[7][crc & 0xff];
			buf += 8;
			len -= 8;
		}
		crc = ~crc;
	}
	if (len == 0) {
		return crc;
	}

	// Byte by byte.
	crc = ~crc;
	for (int i = 0; i < len; i++) {
		crc = table_ieee[0][(crc & 0xff) ^ buf[i]] ^ (crc >> 8);
	}
	return ~crc;
}

// Implemented in assembly. See crc32_amd64.S.
extern crc32_fn_t crc32ieee_fast_impl;

static uint32_t
crc32ieee_fast(uint32_t crc, const char* buf, size_t len) {
	if (len >= 64) {
		// size_t skip = (uintptr_t) buf & 0xf;
		// crc = crc32ieee_slow(crc, buf, skip);
		// buf += skip;
		// len -= skip;
		crc = ~crc32ieee_fast_impl(~crc, buf, len);
		buf += len - (len & 15);
		len &= 15;
	}
	if (len == 0) {
		return crc;
	}
	return crc32ieee_slow(crc, buf, len);
}

// Inspect CPUID for PCLMULQDQ and SSE4.1 feature flags.
// For details, see Wikipedia: https://en.wikipedia.org/wiki/CPUID
static bool
crc32ieee_fast_supported(void) {
	unsigned eax, ebx, ecx, edx;
	return __get_cpuid(1, &eax, &ebx, &ecx, &edx) && (ecx & 0x80002);
}

__attribute__((constructor)) void
crc32ieee_init() {
	for (int i = 0; i < 256; i++) {
		uint32_t crc = i;
		for (int j = 0; j < 8; j++) {
			if ((crc & 1) == 1) {
				crc = (crc >> 1) ^ poly_ieee;
			} else {
				crc >>= 1;
			}
		}
		table_ieee[0][i] = crc;
	}
	for (int i = 0; i < 256; i++) {
		uint32_t crc = table_ieee[0][i];
		for (int j = 1; j < 8; j++) {
			crc = table_ieee[0][crc & 0xff] ^ (crc >> 8);
			table_ieee[j][i] = crc;
		}
	}

	crc32ieee = crc32ieee_fast_supported() ? crc32ieee_fast : crc32ieee_slow;
}
