/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2001  Josh Coalson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#ifndef FLAC__PRIVATE__CPU_H
#define FLAC__PRIVATE__CPU_H

#include "FLAC/config.h"
#include "FLAC/ordinals.h"

typedef enum {
	FLAC__CPUINFO_TYPE_IA32,
	FLAC__CPUINFO_TYPE_UNKNOWN
} FLAC__CPUInfo_Type;

typedef struct {
	bool cmov;
	bool mmx;
	bool fxsr;
	bool sse;
	bool sse2;
} FLAC__CPUInfo_IA32;
extern const unsigned FLAC__CPUINFO_IA32_CPUID_CMOV;
extern const unsigned FLAC__CPUINFO_IA32_CPUID_MMX;
extern const unsigned FLAC__CPUINFO_IA32_CPUID_FXSR;
extern const unsigned FLAC__CPUINFO_IA32_CPUID_SSE;
extern const unsigned FLAC__CPUINFO_IA32_CPUID_SSE2;

typedef struct {
	bool use_asm;
	FLAC__CPUInfo_Type type;
	union {
		FLAC__CPUInfo_IA32 ia32;
	} data;
} FLAC__CPUInfo;

void FLAC__cpu_info(FLAC__CPUInfo *info);

#ifndef FLAC__NO_ASM
#ifdef FLAC__CPU_IA32
#ifdef FLAC__HAS_NASM
unsigned FLAC__cpu_info_asm_i386();
#endif
#endif
#endif

#endif
