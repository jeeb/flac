/* FLAC input plugin for Winamp3
 * Copyright (C) 2000,2001,2002,2003  Josh Coalson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * NOTE: this code is derived from the 'rawpcm' example by
 * Nullsoft; the original license for the 'rawpcm' example follows.
 */
/*

  Nullsoft WASABI Source File License

  Copyright 1999-2001 Nullsoft, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.


  Brennan Underwood
  brennan@nullsoft.com

*/

#include "cnv_flacpcm.h"
#include "flacpcm.h"

static WACNAME wac;
WAComponentClient *the = &wac;

#include "studio/services/servicei.h"

// {683FA153-4055-467c-ABEE-5E35FA03C51E}
static const GUID guid = 
{ 0x683fa153, 0x4055, 0x467c, { 0xab, 0xee, 0x5e, 0x35, 0xfa, 0x3, 0xc5, 0x1e } };

_int nch("# of channels", 2);
_int samplerate("Sample rate", 44100);
_int bps("Bits per second", 16);

WACNAME::WACNAME() : WAComponentClient("FLAC file support") {
	registerService(new waServiceT<svc_mediaConverter, FlacPcm>);
}

WACNAME::~WACNAME() {
}

GUID WACNAME::getGUID() {
	return guid;
}

void WACNAME::onRegisterServices() {
	api->core_registerExtension("*.flac","FLAC Files");

	registerAttribute(&nch);
	registerAttribute(&samplerate);
	registerAttribute(&bps);
}
