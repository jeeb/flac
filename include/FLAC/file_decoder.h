/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2000,2001,2002  Josh Coalson
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

#ifndef FLAC__FILE_DECODER_H
#define FLAC__FILE_DECODER_H

#include "stream_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \file include/FLAC/file_decoder.h
 *
 *  \brief
 *  This module contains the functions which implement the file
 *  decoder.
 *
 *  See the detailed documentation in the
 *  \link flac_file_decoder file decoder \endlink module.
 */

/** \defgroup flac_file_decoder FLAC/file_decoder.h: file decoder interface
 *  \ingroup flac_decoder
 *
 *  \brief
 *  This module contains the functions which implement the file
 *  decoder.
 *
 * The basic usage of this decoder is as follows:
 * - The program creates an instance of a decoder using
 *   FLAC__file_decoder_new().
 * - The program overrides the default settings and sets callbacks for
 *   writing, error reporting, and metadata reporting using
 *   FLAC__file_decoder_set_*() functions.
 * - The program initializes the instance to validate the settings and
 *   prepare for decoding using FLAC__file_decoder_init().
 * - The program calls the FLAC__file_decoder_process_*() functions
 *   to decode data, which subsequently calls the callbacks.
 * - The program finishes the decoding with FLAC__file_decoder_finish(),
 *   which flushes the input and output and resets the decoder to the
 *   uninitialized state.
 * - The instance may be used again or deleted with
 *   FLAC__file_decoder_delete().
 *
 * The file decoder is a trivial wrapper around the
 * \link flac_seekable_stream_decoder seekable stream decoder \endlink
 * meant to simplfy the process of decoding from a standard file.  The
 * file decoder supplies all but the Write/Metadata/Error callbacks.
 * The user needs only to provide the path to the file and the file
 * decoder handles the rest.
 *
 * Like the seekable stream decoder, seeking is exposed through the
 * FLAC__file_decoder_seek_absolute() method.  At any point after the file
 * decoder has been initialized, the user can call this function to seek to
 * an exact sample within the file.  Subsequently, the first time the write
 * callback is called it will be passed a (possibly partial) block starting
 * at that sample.
 *
 * The file decoder also inherits MD5 signature checking from the seekable
 * stream decoder.  If this is turned on before initialization,
 * FLAC__file_decoder_finish() will report when the decoded MD5 signature
 * does not match the one stored in the STREAMINFO block.  MD5 checking is
 * automatically turned off if there is no signature in the STREAMINFO
 * block or when a seek is attempted.
 *
 * \note
 * The "set" functions may only be called when the decoder is in the
 * state FLAC__FILE_DECODER_UNINITIALIZED, i.e. after
 * FLAC__file_decoder_new() or FLAC__file_decoder_finish(), but
 * before FLAC__file_decoder_init().  If this is the case they will
 * return \c true, otherwise \c false.
 *
 * \note
 * FLAC__file_decoder_finish() resets all settings to the constructor
 * defaults, including the callbacks.
 *
 * \{
 */


/** State values for a FLAC__FileDecoder
 *
 *  The decoder's state can be obtained by calling FLAC__file_decoder_get_state().
 */
typedef enum {
    FLAC__FILE_DECODER_OK = 0,
	FLAC__FILE_DECODER_END_OF_FILE,
    FLAC__FILE_DECODER_ERROR_OPENING_FILE,
    FLAC__FILE_DECODER_MEMORY_ALLOCATION_ERROR,
	FLAC__FILE_DECODER_SEEK_ERROR,
	FLAC__FILE_DECODER_SEEKABLE_STREAM_DECODER_ERROR,
    FLAC__FILE_DECODER_ALREADY_INITIALIZED,
    FLAC__FILE_DECODER_INVALID_CALLBACK,
    FLAC__FILE_DECODER_UNINITIALIZED
} FLAC__FileDecoderState;
extern const char * const FLAC__FileDecoderStateString[];

/***********************************************************************
 *
 * class FLAC__FileDecoder : public FLAC__StreamDecoder
 *
 ***********************************************************************/

struct FLAC__FileDecoderProtected;
struct FLAC__FileDecoderPrivate;
typedef struct {
	struct FLAC__FileDecoderProtected *protected_; /* avoid the C++ keyword 'protected' */
	struct FLAC__FileDecoderPrivate *private_; /* avoid the C++ keyword 'private' */
} FLAC__FileDecoder;

/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/

/*
 * Any parameters that are not set before FLAC__file_decoder_init()
 * will take on the defaults from the constructor, shown below.
 * For more on what the parameters mean, see the documentation.
 *
 * FLAC__bool  md5_checking                   (DEFAULT: false) MD5 checking will be turned off if a seek is requested
 *           (*write_callback)()              (DEFAULT: NULL ) The callbacks are the only values that MUST be set before FLAC__file_decoder_init()
 *           (*metadata_callback)()           (DEFAULT: NULL )
 *           (*error_callback)()              (DEFAULT: NULL )
 * void*       client_data                    (DEFAULT: NULL ) passed back through the callbacks
 *          metadata_respond/ignore        By default, only the STREAMINFO block is returned via metadata_callback()
 */
FLAC__FileDecoder *FLAC__file_decoder_new();
void FLAC__file_decoder_delete(FLAC__FileDecoder *);

/***********************************************************************
 *
 * Public class method prototypes
 *
 ***********************************************************************/

/*
 * Various "set" methods.  These may only be called when the decoder
 * is in the state FLAC__FILE_DECODER_UNINITIALIZED, i.e. after
 * FLAC__file_decoder_new() or FLAC__file_decoder_finish(), but
 * before FLAC__file_decoder_init().  If this is the case they will
 * return true, otherwise false.
 *
 * NOTE that these functions do not validate the values as many are
 * interdependent.  The FLAC__file_decoder_init() function will do
 * this, so make sure to pay attention to the state returned by
 * FLAC__file_decoder_init().
 *
 * Any parameters that are not set before FLAC__file_decoder_init()
 * will take on the defaults from the constructor.  NOTE that
 * FLAC__file_decoder_flush() or FLAC__file_decoder_reset() do
 * NOT reset the values to the constructor defaults.
 */
FLAC__bool FLAC__file_decoder_set_md5_checking(FLAC__FileDecoder *decoder, FLAC__bool value);
FLAC__bool FLAC__file_decoder_set_filename(FLAC__FileDecoder *decoder, const char *value); /* 'value' may not be 0; use "-" for stdin */
FLAC__bool FLAC__file_decoder_set_write_callback(FLAC__FileDecoder *decoder, FLAC__StreamDecoderWriteStatus (*value)(const FLAC__FileDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data));
FLAC__bool FLAC__file_decoder_set_metadata_callback(FLAC__FileDecoder *decoder, void (*value)(const FLAC__FileDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data));
FLAC__bool FLAC__file_decoder_set_error_callback(FLAC__FileDecoder *decoder, void (*value)(const FLAC__FileDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data));
FLAC__bool FLAC__file_decoder_set_client_data(FLAC__FileDecoder *decoder, void *value);
/*
 * See the comments for the equivalent functions in stream_decoder.h
 */
FLAC__bool FLAC__file_decoder_set_metadata_respond(FLAC__FileDecoder *decoder, FLAC__MetadataType type);
FLAC__bool FLAC__file_decoder_set_metadata_respond_application(FLAC__FileDecoder *decoder, const FLAC__byte id[4]);
FLAC__bool FLAC__file_decoder_set_metadata_respond_all(FLAC__FileDecoder *decoder);
FLAC__bool FLAC__file_decoder_set_metadata_ignore(FLAC__FileDecoder *decoder, FLAC__MetadataType type);
FLAC__bool FLAC__file_decoder_set_metadata_ignore_application(FLAC__FileDecoder *decoder, const FLAC__byte id[4]);
FLAC__bool FLAC__file_decoder_set_metadata_ignore_all(FLAC__FileDecoder *decoder);

/*
 * Various "get" methods
 */
FLAC__FileDecoderState FLAC__file_decoder_get_state(const FLAC__FileDecoder *decoder);
FLAC__bool FLAC__file_decoder_get_md5_checking(const FLAC__FileDecoder *decoder);
/*
 * Methods to return the current number of channels, channel assignment
 * bits-per-sample, sample rate in Hz, and blocksize in samples.  These
 * will only be valid after decoding has started.
 */
unsigned FLAC__file_decoder_get_channels(const FLAC__FileDecoder *decoder);
FLAC__ChannelAssignment FLAC__file_decoder_get_channel_assignment(const FLAC__FileDecoder *decoder);
unsigned FLAC__file_decoder_get_bits_per_sample(const FLAC__FileDecoder *decoder);
unsigned FLAC__file_decoder_get_sample_rate(const FLAC__FileDecoder *decoder);
unsigned FLAC__file_decoder_get_blocksize(const FLAC__FileDecoder *decoder);

/*
 * Initialize the instance; should be called after construction and
 * 'set' calls but before any of the 'process' or 'seek' calls.  Will
 * set and return the decoder state, which will be FLAC__FILE_DECODER_OK
 * if initialization succeeded.
 */
FLAC__FileDecoderState FLAC__file_decoder_init(FLAC__FileDecoder *decoder);

/*
 * Flush the decoding buffer, release resources, and return the decoder
 * state to FLAC__FILE_DECODER_UNINITIALIZED.  Only returns false if
 * md5_checking is set AND the stored MD5 sum is non-zero AND the stored
 * MD5 sum and computed MD5 sum do not match.
 */
FLAC__bool FLAC__file_decoder_finish(FLAC__FileDecoder *decoder);

/*
 * Methods for decoding the data
 */
FLAC__bool FLAC__file_decoder_process_whole_file(FLAC__FileDecoder *decoder);
FLAC__bool FLAC__file_decoder_process_metadata(FLAC__FileDecoder *decoder);
FLAC__bool FLAC__file_decoder_process_one_frame(FLAC__FileDecoder *decoder);
FLAC__bool FLAC__file_decoder_process_remaining_frames(FLAC__FileDecoder *decoder);

FLAC__bool FLAC__file_decoder_seek_absolute(FLAC__FileDecoder *decoder, FLAC__uint64 sample);

/* \} */

#ifdef __cplusplus
}
#endif

#endif
