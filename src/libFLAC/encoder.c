/* libFLAC - Free Lossless Audio Coder library
 * Copyright (C) 2000  Josh Coalson
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* for malloc() */
#include <string.h> /* for memcpy() */
#include "FLAC/encoder.h"
#include "private/bitbuffer.h"
#include "private/encoder_framing.h"
#include "private/fixed.h"
#include "private/lpc.h"

#ifdef min
#undef min
#endif
#define min(x,y) ((x)<(y)?(x):(y))

#ifdef max
#undef max
#endif
#define max(x,y) ((x)>(y)?(x):(y))

#ifdef RICE_BITS
#undef RICE_BITS
#endif
#define RICE_BITS(value, parameter) (2 + (parameter) + (((unsigned)((value) < 0? -(value) : (value))) >> (parameter)))

typedef struct FLAC__EncoderPrivate {
	unsigned input_capacity;                    /* current size (in samples) of the signal and residual buffers */
	int32 *integer_signal[FLAC__MAX_CHANNELS];  /* the integer version of the input signal */
	int32 *integer_signal_mid_side[2];          /* the integer version of the mid-side input signal (stereo only) */
	real *real_signal[FLAC__MAX_CHANNELS];      /* the floating-point version of the input signal */
	real *real_signal_mid_side[2];              /* the floating-point version of the mid-side input signal (stereo only) */
	int32 *residual[2];                         /* where the candidate and best subframe residual signals will be stored */
	unsigned best_residual;                     /* index into the above */
	FLAC__BitBuffer frame;                      /* the current frame being worked on */
	FLAC__BitBuffer frame_mid_side;             /* special parallel workspace for the mid-side coded version of the current frame */
	FLAC__BitBuffer frame_left_side;            /* special parallel workspace for the left-side coded version of the current frame */
	FLAC__BitBuffer frame_right_side;           /* special parallel workspace for the right-side coded version of the current frame */
	FLAC__SubframeHeader best_subframe, candidate_subframe;
	bool current_frame_can_do_mid_side;         /* encoder sets this false when any given sample of a frame's side channel exceeds 16 bits */
	FLAC__StreamMetaData metadata;
	unsigned current_sample_number;
	unsigned current_frame_number;
	FLAC__EncoderWriteStatus (*write_callback)(const FLAC__Encoder *encoder, const byte buffer[], unsigned bytes, unsigned samples, unsigned current_frame, void *client_data);
	void (*metadata_callback)(const FLAC__Encoder *encoder, const FLAC__StreamMetaData *metadata, void *client_data);
	void *client_data;
} FLAC__EncoderPrivate;

static bool encoder_resize_buffers_(FLAC__Encoder *encoder, unsigned new_size);
static bool encoder_process_frame_(FLAC__Encoder *encoder, bool is_last_frame);
static bool encoder_process_subframes_(FLAC__Encoder *encoder, bool is_last_frame, const FLAC__FrameHeader *frame_header, unsigned channels, const int32 *integer_signal[], const real *real_signal[], FLAC__BitBuffer *bitbuffer);
static unsigned encoder_evaluate_constant_subframe_(const int32 signal, unsigned bits_per_sample, FLAC__SubframeHeader *subframe);
static unsigned encoder_evaluate_fixed_subframe_(const int32 signal[], int32 residual[], unsigned blocksize, unsigned bits_per_sample, unsigned order, unsigned rice_parameter, unsigned max_partition_order, FLAC__SubframeHeader *subframe);
static unsigned encoder_evaluate_lpc_subframe_(const int32 signal[], int32 residual[], const real lp_coeff[], unsigned blocksize, unsigned bits_per_sample, unsigned order, unsigned qlp_coeff_precision, unsigned rice_parameter, unsigned max_partition_order, FLAC__SubframeHeader *subframe);
static unsigned encoder_evaluate_verbatim_subframe_(unsigned blocksize, unsigned bits_per_sample, FLAC__SubframeHeader *subframe);
static unsigned encoder_find_best_partition_order_(int32 residual[], unsigned residual_samples, unsigned predictor_order, unsigned rice_parameter, unsigned max_partition_order, unsigned *best_partition_order, unsigned best_parameters[]);
static bool encoder_generate_constant_subframe_(const FLAC__SubframeHeader *header, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer);
static bool encoder_generate_fixed_subframe_(const FLAC__SubframeHeader *header, int32 residual[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer);
static bool encoder_generate_lpc_subframe_(const FLAC__SubframeHeader *header, int32 residual[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer);
static bool encoder_generate_verbatim_subframe_(const FLAC__SubframeHeader *header, const int32 signal[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer);
static void encoder_promote_candidate_subframe_(FLAC__Encoder *encoder);
static bool encoder_set_partitioned_rice_(const int32 residual[], const unsigned residual_samples, const unsigned predictor_order, const unsigned rice_parameter, const unsigned partition_order, unsigned parameters[], unsigned *bits);

const char *FLAC__EncoderWriteStatusString[] = {
	"FLAC__ENCODER_WRITE_OK",
	"FLAC__ENCODER_WRITE_FATAL_ERROR"
};

const char *FLAC__EncoderStateString[] = {
	"FLAC__ENCODER_OK",
	"FLAC__ENCODER_UNINITIALIZED",
	"FLAC__ENCODER_INVALID_NUMBER_OF_CHANNELS",
	"FLAC__ENCODER_INVALID_BITS_PER_SAMPLE",
	"FLAC__ENCODER_INVALID_SAMPLE_RATE",
	"FLAC__ENCODER_INVALID_BLOCK_SIZE",
	"FLAC__ENCODER_INVALID_QLP_COEFF_PRECISION",
	"FLAC__ENCODER_MID_SIDE_CHANNELS_MISMATCH",
	"FLAC__ENCODER_MID_SIDE_SAMPLE_SIZE_MISMATCH",
	"FLAC__ENCODER_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER",
	"FLAC__ENCODER_NOT_STREAMABLE",
	"FLAC__ENCODER_FRAMING_ERROR",
	"FLAC__ENCODER_FATAL_ERROR_WHILE_ENCODING",
	"FLAC__ENCODER_FATAL_ERROR_WHILE_WRITING",
	"FLAC__ENCODER_MEMORY_ALLOCATION_ERROR"
};


bool encoder_resize_buffers_(FLAC__Encoder *encoder, unsigned new_size)
{
	bool ok;
	unsigned i;
	int32 *previous_is, *current_is;
	real *previous_rs, *current_rs;
	int32 *residual;

	assert(new_size > 0);
	assert(encoder->state == FLAC__ENCODER_OK);
	assert(encoder->guts->current_sample_number == 0);

	/* To avoid excessive malloc'ing, we only grow the buffer; no shrinking. */
	if(new_size <= encoder->guts->input_capacity)
		return true;

	ok = 1;
	if(ok) {
		for(i = 0; ok && i < encoder->channels; i++) {
			/* integer version of the signal */
			previous_is = encoder->guts->integer_signal[i];
			current_is = (int32*)malloc(sizeof(int32) * new_size);
			if(0 == current_is) {
				encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
				ok = 0;
			}
			else {
				encoder->guts->integer_signal[i] = current_is;
				if(previous_is != 0)
					free(previous_is);
			}
			/* real version of the signal */
			previous_rs = encoder->guts->real_signal[i];
			current_rs = (real*)malloc(sizeof(real) * new_size);
			if(0 == current_rs) {
				encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
				ok = 0;
			}
			else {
				encoder->guts->real_signal[i] = current_rs;
				if(previous_rs != 0)
					free(previous_rs);
			}
		}
	}
	if(ok) {
		for(i = 0; ok && i < 2; i++) {
			/* integer version of the signal */
			previous_is = encoder->guts->integer_signal_mid_side[i];
			current_is = (int32*)malloc(sizeof(int32) * new_size);
			if(0 == current_is) {
				encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
				ok = 0;
			}
			else {
				encoder->guts->integer_signal_mid_side[i] = current_is;
				if(previous_is != 0)
					free(previous_is);
			}
			/* real version of the signal */
			previous_rs = encoder->guts->real_signal_mid_side[i];
			current_rs = (real*)malloc(sizeof(real) * new_size);
			if(0 == current_rs) {
				encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
				ok = 0;
			}
			else {
				encoder->guts->real_signal_mid_side[i] = current_rs;
				if(previous_rs != 0)
					free(previous_rs);
			}
		}
	}
	if(ok) {
		for(i = 0; i < 2; i++) {
			residual = (int32*)malloc(sizeof(int32) * new_size);
			if(0 == residual) {
				encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
				ok = 0;
			}
			else {
				if(encoder->guts->residual[i] != 0)
					free(encoder->guts->residual[i]);
				encoder->guts->residual[i] = residual;
			}
		}
	}
	if(ok)
		encoder->guts->input_capacity = new_size;

	return ok;
}

FLAC__Encoder *FLAC__encoder_get_new_instance()
{
	FLAC__Encoder *encoder = (FLAC__Encoder*)malloc(sizeof(FLAC__Encoder));
	if(encoder != 0) {
		encoder->state = FLAC__ENCODER_UNINITIALIZED;
		encoder->guts = 0;
	}
	return encoder;
}

void FLAC__encoder_free_instance(FLAC__Encoder *encoder)
{
	assert(encoder != 0);
	free(encoder);
}

FLAC__EncoderState FLAC__encoder_init(FLAC__Encoder *encoder, FLAC__EncoderWriteStatus (*write_callback)(const FLAC__Encoder *encoder, const byte buffer[], unsigned bytes, unsigned samples, unsigned current_frame, void *client_data), void (*metadata_callback)(const FLAC__Encoder *encoder, const FLAC__StreamMetaData *metadata, void *client_data), void *client_data)
{
	unsigned i;

	assert(sizeof(int) >= 4); /* we want to die right away if this is not true */
	assert(encoder != 0);
	assert(write_callback != 0);
	assert(metadata_callback != 0);
	assert(encoder->state == FLAC__ENCODER_UNINITIALIZED);
	assert(encoder->guts == 0);

	encoder->state = FLAC__ENCODER_OK;

	if(encoder->channels == 0 || encoder->channels > FLAC__MAX_CHANNELS)
		return encoder->state = FLAC__ENCODER_INVALID_NUMBER_OF_CHANNELS;

	if(encoder->do_mid_side_stereo && encoder->channels != 2)
		return encoder->state = FLAC__ENCODER_MID_SIDE_CHANNELS_MISMATCH;

	if(encoder->do_mid_side_stereo && encoder->bits_per_sample > 16)
		return encoder->state = FLAC__ENCODER_MID_SIDE_SAMPLE_SIZE_MISMATCH;

	if(encoder->bits_per_sample == 0 || encoder->bits_per_sample > FLAC__MAX_BITS_PER_SAMPLE)
		return encoder->state = FLAC__ENCODER_INVALID_BITS_PER_SAMPLE;

	if(encoder->sample_rate == 0 || encoder->sample_rate > FLAC__MAX_SAMPLE_RATE)
		return encoder->state = FLAC__ENCODER_INVALID_SAMPLE_RATE;

	if(encoder->blocksize < FLAC__MIN_BLOCK_SIZE || encoder->blocksize > FLAC__MAX_BLOCK_SIZE)
		return encoder->state = FLAC__ENCODER_INVALID_BLOCK_SIZE;

	if(encoder->blocksize < encoder->max_lpc_order)
		return encoder->state = FLAC__ENCODER_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER;

	if(encoder->qlp_coeff_precision == 0) {
		if(encoder->bits_per_sample < 16) {
			/* @@@ need some data about how to set this here w.r.t. blocksize and sample rate */
			/* @@@ until then we'll make a guess */
			encoder->qlp_coeff_precision = max(5, 2 + encoder->bits_per_sample / 2);
		}
		else if(encoder->bits_per_sample == 16) {
			if(encoder->blocksize <= 192)
				encoder->qlp_coeff_precision = 7;
			else if(encoder->blocksize <= 384)
				encoder->qlp_coeff_precision = 8;
			else if(encoder->blocksize <= 576)
				encoder->qlp_coeff_precision = 9;
			else if(encoder->blocksize <= 1152)
				encoder->qlp_coeff_precision = 10;
			else if(encoder->blocksize <= 2304)
				encoder->qlp_coeff_precision = 11;
			else if(encoder->blocksize <= 4608)
				encoder->qlp_coeff_precision = 12;
			else
				encoder->qlp_coeff_precision = 13;
		}
		else {
			encoder->qlp_coeff_precision = min(13, 8*sizeof(int32) - encoder->bits_per_sample - 1);
		}
	}
	else if(encoder->qlp_coeff_precision < FLAC__MIN_QLP_COEFF_PRECISION || encoder->qlp_coeff_precision + encoder->bits_per_sample >= 8*sizeof(uint32))
		return encoder->state = FLAC__ENCODER_INVALID_QLP_COEFF_PRECISION;

	if(encoder->streamable_subset) {
		if(encoder->bits_per_sample != 8 && encoder->bits_per_sample != 12 && encoder->bits_per_sample != 16 && encoder->bits_per_sample != 20 && encoder->bits_per_sample != 24)
			return encoder->state = FLAC__ENCODER_NOT_STREAMABLE;
		if(encoder->sample_rate > 655350)
			return encoder->state = FLAC__ENCODER_NOT_STREAMABLE;
	}

	if(encoder->rice_optimization_level >= (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN))
		encoder->rice_optimization_level = (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN) - 1;

	encoder->guts = (FLAC__EncoderPrivate*)malloc(sizeof(FLAC__EncoderPrivate));
	if(encoder->guts == 0)
		return encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;

	encoder->guts->input_capacity = 0;
	for(i = 0; i < encoder->channels; i++) {
		encoder->guts->integer_signal[i] = 0;
		encoder->guts->real_signal[i] = 0;
	}
	for(i = 0; i < 2; i++) {
		encoder->guts->integer_signal_mid_side[i] = 0;
		encoder->guts->real_signal_mid_side[i] = 0;
	}
	encoder->guts->residual[0] = 0;
	encoder->guts->residual[1] = 0;
	encoder->guts->best_residual = 0;
	encoder->guts->current_frame_can_do_mid_side = true;
	encoder->guts->current_sample_number = 0;
	encoder->guts->current_frame_number = 0;

	if(!encoder_resize_buffers_(encoder, encoder->blocksize)) {
		/* the above function sets the state for us in case of an error */
		return encoder->state;
	}
	FLAC__bitbuffer_init(&encoder->guts->frame);
	encoder->guts->write_callback = write_callback;
	encoder->guts->metadata_callback = metadata_callback;
	encoder->guts->client_data = client_data;

	/*
	 * write the stream header
	 */
	if(!FLAC__bitbuffer_clear(&encoder->guts->frame))
		return encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;

	if(!FLAC__bitbuffer_write_raw_uint32(&encoder->guts->frame, FLAC__STREAM_SYNC, FLAC__STREAM_SYNC_LEN))
		return encoder->state = FLAC__ENCODER_FRAMING_ERROR;

	encoder->guts->metadata.type = FLAC__METADATA_TYPE_ENCODING;
	encoder->guts->metadata.is_last = true;
	encoder->guts->metadata.length = FLAC__STREAM_METADATA_ENCODING_LENGTH;
	encoder->guts->metadata.data.encoding.min_blocksize = encoder->blocksize; /* this encoder uses the same blocksize for the whole stream */
	encoder->guts->metadata.data.encoding.max_blocksize = encoder->blocksize;
	encoder->guts->metadata.data.encoding.min_framesize = 0; /* we don't know this yet; have to fill it in later */
	encoder->guts->metadata.data.encoding.max_framesize = 0; /* we don't know this yet; have to fill it in later */
	encoder->guts->metadata.data.encoding.sample_rate = encoder->sample_rate;
	encoder->guts->metadata.data.encoding.channels = encoder->channels;
	encoder->guts->metadata.data.encoding.bits_per_sample = encoder->bits_per_sample;
	encoder->guts->metadata.data.encoding.total_samples = 0; /* we don't know this yet; have to fill it in later */
	if(!FLAC__add_metadata_block(&encoder->guts->metadata, &encoder->guts->frame))
		return encoder->state = FLAC__ENCODER_FRAMING_ERROR;

	assert(encoder->guts->frame.bits == 0); /* assert that we're byte-aligned before writing */
	assert(encoder->guts->frame.total_consumed_bits == 0); /* assert that no reading of the buffer was done */
	if(encoder->guts->write_callback(encoder, encoder->guts->frame.buffer, encoder->guts->frame.bytes, 0, encoder->guts->current_frame_number, encoder->guts->client_data) != FLAC__ENCODER_WRITE_OK)
		return encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_WRITING;

	/* now that the metadata block is written, we can init this to an absurdly-high value */
	encoder->guts->metadata.data.encoding.min_framesize = (1u << FLAC__STREAM_METADATA_ENCODING_MIN_FRAME_SIZE_LEN) - 1;

	return encoder->state;
}

void FLAC__encoder_finish(FLAC__Encoder *encoder)
{
	unsigned i;

	assert(encoder != 0);
	if(encoder->state == FLAC__ENCODER_UNINITIALIZED)
		return;
	if(encoder->guts->current_sample_number != 0) {
		encoder->blocksize = encoder->guts->current_sample_number;
		encoder_process_frame_(encoder, true); /* true => is last frame */
	}
	encoder->guts->metadata_callback(encoder, &encoder->guts->metadata, encoder->guts->client_data);
	if(encoder->guts != 0) {
		for(i = 0; i < encoder->channels; i++) {
			if(encoder->guts->integer_signal[i] != 0) {
				free(encoder->guts->integer_signal[i]);
				encoder->guts->integer_signal[i] = 0;
			}
			if(encoder->guts->real_signal[i] != 0) {
				free(encoder->guts->real_signal[i]);
				encoder->guts->real_signal[i] = 0;
			}
		}
		for(i = 0; i < 2; i++) {
			if(encoder->guts->integer_signal_mid_side[i] != 0) {
				free(encoder->guts->integer_signal_mid_side[i]);
				encoder->guts->integer_signal_mid_side[i] = 0;
			}
			if(encoder->guts->real_signal_mid_side[i] != 0) {
				free(encoder->guts->real_signal_mid_side[i]);
				encoder->guts->real_signal_mid_side[i] = 0;
			}
		}
		for(i = 0; i < 2; i++) {
			if(encoder->guts->residual[i] != 0) {
				free(encoder->guts->residual[i]);
				encoder->guts->residual[i] = 0;
			}
		}
		FLAC__bitbuffer_free(&encoder->guts->frame);
		free(encoder->guts);
		encoder->guts = 0;
	}
	encoder->state = FLAC__ENCODER_UNINITIALIZED;
}

bool FLAC__encoder_process(FLAC__Encoder *encoder, const int32 *buf[], unsigned samples)
{
	unsigned i, j, channel;
	int32 x, mid, side;
	const bool ms = encoder->do_mid_side_stereo && encoder->channels == 2;
	const int32 min_side = -((int64)1 << (encoder->bits_per_sample-1));
	const int32 max_side =  ((int64)1 << (encoder->bits_per_sample-1)) - 1;

	assert(encoder != 0);
	assert(encoder->state == FLAC__ENCODER_OK);

	j = 0;
	do {
		for(i = encoder->guts->current_sample_number; i < encoder->blocksize && j < samples; i++, j++) {
			for(channel = 0; channel < encoder->channels; channel++) {
				x = buf[channel][j];
				encoder->guts->integer_signal[channel][i] = x;
				encoder->guts->real_signal[channel][i] = (real)x;
			}
			if(ms && encoder->guts->current_frame_can_do_mid_side) {
				side = buf[0][j] - buf[1][j];
				if(side < min_side || side > max_side) {
					encoder->guts->current_frame_can_do_mid_side = false;
				}
				else {
					mid = (buf[0][j] + buf[1][j]) >> 1; /* NOTE: not the same as divide-by-two ! */
					encoder->guts->integer_signal_mid_side[0][i] = mid;
					encoder->guts->integer_signal_mid_side[1][i] = side;
					encoder->guts->real_signal_mid_side[0][i] = (real)mid;
					encoder->guts->real_signal_mid_side[1][i] = (real)side;
				}
			}
			encoder->guts->current_sample_number++;
		}
		if(i == encoder->blocksize) {
			if(!encoder_process_frame_(encoder, false)) /* false => not last frame */
				return false;
		}
	} while(j < samples);

	return true;
}

/* 'samples' is channel-wide samples, e.g. for 1 second at 44100Hz, 'samples' = 44100 regardless of the number of channels */
bool FLAC__encoder_process_interleaved(FLAC__Encoder *encoder, const int32 buf[], unsigned samples)
{
	unsigned i, j, k, channel;
	int32 x, left = 0, mid, side;
	const bool ms = encoder->do_mid_side_stereo && encoder->channels == 2;
	const int32 min_side = -((int64)1 << (encoder->bits_per_sample-1));
	const int32 max_side =  ((int64)1 << (encoder->bits_per_sample-1)) - 1;

	assert(encoder != 0);
	assert(encoder->state == FLAC__ENCODER_OK);

	j = k = 0;
	do {
		for(i = encoder->guts->current_sample_number; i < encoder->blocksize && j < samples; i++, j++, k++) {
			for(channel = 0; channel < encoder->channels; channel++, k++) {
				x = buf[k];
				encoder->guts->integer_signal[channel][i] = x;
				encoder->guts->real_signal[channel][i] = (real)x;
				if(ms && encoder->guts->current_frame_can_do_mid_side) {
					if(channel == 0) {
						left = x;
					}
					else {
						side = left - x;
						if(side < min_side || side > max_side) {
							encoder->guts->current_frame_can_do_mid_side = false;
						}
						else {
							mid = (left + x) >> 1; /* NOTE: not the same as divide-by-two ! */
							encoder->guts->integer_signal_mid_side[0][i] = mid;
							encoder->guts->integer_signal_mid_side[1][i] = side;
							encoder->guts->real_signal_mid_side[0][i] = (real)mid;
							encoder->guts->real_signal_mid_side[1][i] = (real)side;
						}
					}
				}
			}
			encoder->guts->current_sample_number++;
		}
		if(i == encoder->blocksize) {
			if(!encoder_process_frame_(encoder, false)) /* false => not last frame */
				return false;
		}
	} while(j < samples);

	return true;
}

bool encoder_process_frame_(FLAC__Encoder *encoder, bool is_last_frame)
{
	FLAC__FrameHeader frame_header;
	FLAC__BitBuffer *smallest_frame;

	assert(encoder->state == FLAC__ENCODER_OK);

	/*
	 * First do a normal encoding pass
	 */
	frame_header.blocksize = encoder->blocksize;
	frame_header.sample_rate = encoder->sample_rate;
	frame_header.channels = encoder->channels;
	frame_header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT; /* the default unless the encoder determines otherwise */
	frame_header.bits_per_sample = encoder->bits_per_sample;
	frame_header.number.frame_number = encoder->guts->current_frame_number;

	if(!FLAC__bitbuffer_clear(&encoder->guts->frame)) {
		encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}
	if(!FLAC__frame_add_header(&frame_header, encoder->streamable_subset, is_last_frame, &encoder->guts->frame)) {
		encoder->state = FLAC__ENCODER_FRAMING_ERROR;
		return false;
	}

	if(!encoder_process_subframes_(encoder, is_last_frame, &frame_header, encoder->channels, encoder->guts->integer_signal, encoder->guts->real_signal, &encoder->guts->frame))
		return false;

	smallest_frame = &encoder->guts->frame;

	/*
	 * Now try a mid-side version if necessary; otherwise, just use the previous step's frame
	 */
	if(encoder->do_mid_side_stereo && encoder->guts->current_frame_can_do_mid_side) {
		int32 *integer_signal[2];
		real *real_signal[2];

		assert(encoder->channels == 2);

		/* mid-side */
		frame_header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_MID_SIDE;
		if(!FLAC__bitbuffer_clear(&encoder->guts->frame_mid_side)) {
			encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		if(!FLAC__frame_add_header(&frame_header, encoder->streamable_subset, is_last_frame, &encoder->guts->frame_mid_side)) {
			encoder->state = FLAC__ENCODER_FRAMING_ERROR;
			return false;
		}
		integer_signal[0] = encoder->guts->integer_signal_mid_side[0]; /* mid channel */
		integer_signal[1] = encoder->guts->integer_signal_mid_side[1]; /* side channel */
		real_signal[0] = encoder->guts->real_signal_mid_side[0]; /* mid channel */
		real_signal[1] = encoder->guts->real_signal_mid_side[1]; /* side channel */
		if(!encoder_process_subframes_(encoder, is_last_frame, &frame_header, encoder->channels, integer_signal, real_signal, &encoder->guts->frame_mid_side))
			return false;
		if(encoder->guts->frame_mid_side.total_bits < smallest_frame->total_bits)
			smallest_frame = &encoder->guts->frame_mid_side;

		/* left-side */
		frame_header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE;
		if(!FLAC__bitbuffer_clear(&encoder->guts->frame_left_side)) {
			encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		if(!FLAC__frame_add_header(&frame_header, encoder->streamable_subset, is_last_frame, &encoder->guts->frame_left_side)) {
			encoder->state = FLAC__ENCODER_FRAMING_ERROR;
			return false;
		}
		integer_signal[0] = encoder->guts->integer_signal[0]; /* left channel */
		integer_signal[1] = encoder->guts->integer_signal_mid_side[1]; /* side channel */
		real_signal[0] = encoder->guts->real_signal[0]; /* left channel */
		real_signal[1] = encoder->guts->real_signal_mid_side[1]; /* side channel */
		if(!encoder_process_subframes_(encoder, is_last_frame, &frame_header, encoder->channels, integer_signal, real_signal, &encoder->guts->frame_left_side))
			return false;
		if(encoder->guts->frame_left_side.total_bits < smallest_frame->total_bits)
			smallest_frame = &encoder->guts->frame_left_side;

		/* right-side */
		frame_header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE;
		if(!FLAC__bitbuffer_clear(&encoder->guts->frame_right_side)) {
			encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		if(!FLAC__frame_add_header(&frame_header, encoder->streamable_subset, is_last_frame, &encoder->guts->frame_right_side)) {
			encoder->state = FLAC__ENCODER_FRAMING_ERROR;
			return false;
		}
		integer_signal[0] = encoder->guts->integer_signal_mid_side[1]; /* side channel */
		integer_signal[1] = encoder->guts->integer_signal[1]; /* right channel */
		real_signal[0] = encoder->guts->real_signal_mid_side[1]; /* side channel */
		real_signal[1] = encoder->guts->real_signal[1]; /* right channel */
		if(!encoder_process_subframes_(encoder, is_last_frame, &frame_header, encoder->channels, integer_signal, real_signal, &encoder->guts->frame_right_side))
			return false;
		if(encoder->guts->frame_right_side.total_bits < smallest_frame->total_bits)
			smallest_frame = &encoder->guts->frame_right_side;
	}

	/*
	 * Zero-pad the frame to a byte_boundary
	 */
	if(!FLAC__bitbuffer_zero_pad_to_byte_boundary(smallest_frame)) {
		encoder->state = FLAC__ENCODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}

	/*
	 * Write it
	 */
	assert(smallest_frame->bits == 0); /* assert that we're byte-aligned before writing */
	assert(smallest_frame->total_consumed_bits == 0); /* assert that no reading of the buffer was done */
	if(encoder->guts->write_callback(encoder, smallest_frame->buffer, smallest_frame->bytes, encoder->blocksize, encoder->guts->current_frame_number, encoder->guts->client_data) != FLAC__ENCODER_WRITE_OK) {
		encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_WRITING;
		return false;
	}

	/*
	 * Get ready for the next frame
	 */
	encoder->guts->current_frame_can_do_mid_side = true;
	encoder->guts->current_sample_number = 0;
	encoder->guts->current_frame_number++;
	encoder->guts->metadata.data.encoding.total_samples += (uint64)encoder->blocksize;
	encoder->guts->metadata.data.encoding.min_framesize = min(smallest_frame->bytes, encoder->guts->metadata.data.encoding.min_framesize);
	encoder->guts->metadata.data.encoding.max_framesize = max(smallest_frame->bytes, encoder->guts->metadata.data.encoding.max_framesize);

	return true;
}

bool encoder_process_subframes_(FLAC__Encoder *encoder, bool is_last_frame, const FLAC__FrameHeader *frame_header, unsigned channels, const int32 *integer_signal[], const real *real_signal[], FLAC__BitBuffer *frame)
{
	real fixed_residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1];
	real lpc_residual_bits_per_sample;
	real autoc[FLAC__MAX_LPC_ORDER+1];
	real lp_coeff[FLAC__MAX_LPC_ORDER][FLAC__MAX_LPC_ORDER];
	real lpc_error[FLAC__MAX_LPC_ORDER];
	unsigned channel;
	unsigned min_lpc_order, max_lpc_order, lpc_order;
	unsigned min_fixed_order, max_fixed_order, guess_fixed_order, fixed_order;
	unsigned max_partition_order;
	unsigned min_qlp_coeff_precision, max_qlp_coeff_precision, qlp_coeff_precision;
	unsigned rice_parameter;
	unsigned candidate_bits, best_bits;

	if(is_last_frame) {
		max_partition_order = 0;
	}
	else {
		unsigned limit = 0, b = encoder->blocksize;
		while(!(b & 1)) {
			limit++;
			b >>= 1;
		}
		max_partition_order = min(encoder->rice_optimization_level, limit);
	}

	for(channel = 0; channel < channels; channel++) {
		/* verbatim subframe is the baseline against which we measure other compressed subframes */
		best_bits = encoder_evaluate_verbatim_subframe_(frame_header->blocksize, frame_header->bits_per_sample, &(encoder->guts->best_subframe));

		if(frame_header->blocksize >= FLAC__MAX_FIXED_ORDER) {
			/* check for constant subframe */
			guess_fixed_order = FLAC__fixed_compute_best_predictor(integer_signal[channel]+FLAC__MAX_FIXED_ORDER, frame_header->blocksize-FLAC__MAX_FIXED_ORDER, fixed_residual_bits_per_sample);
			if(fixed_residual_bits_per_sample[1] == 0.0) {
				/* the above means integer_signal[channel]+FLAC__MAX_FIXED_ORDER is constant, now we just have to check the warmup samples */
				unsigned i, signal_is_constant = true;
				for(i = 1; i <= FLAC__MAX_FIXED_ORDER; i++) {
					if(integer_signal[channel][0] != integer_signal[channel][i]) {
						signal_is_constant = false;
						break;
					}
				}
				if(signal_is_constant) {
					candidate_bits = encoder_evaluate_constant_subframe_(integer_signal[channel][0], frame_header->bits_per_sample, &(encoder->guts->candidate_subframe));
					if(candidate_bits < best_bits) {
						encoder_promote_candidate_subframe_(encoder);
						best_bits = candidate_bits;
					}
				}
			}
			else {
				/* encode fixed */
				if(encoder->do_exhaustive_model_search) {
					min_fixed_order = 0;
					max_fixed_order = FLAC__MAX_FIXED_ORDER;
				}
				else {
					min_fixed_order = max_fixed_order = guess_fixed_order;
				}
				for(fixed_order = min_fixed_order; fixed_order <= max_fixed_order; fixed_order++) {
					if(fixed_residual_bits_per_sample[fixed_order] >= (real)frame_header->bits_per_sample)
						continue; /* don't even try */
					rice_parameter = (fixed_residual_bits_per_sample[fixed_order] > 0.0)? (unsigned)(fixed_residual_bits_per_sample[fixed_order]+0.5) : 0;
					if(rice_parameter >= (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN))
						rice_parameter = (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN) - 1;
					candidate_bits = encoder_evaluate_fixed_subframe_(integer_signal[channel], encoder->guts->residual[!encoder->guts->best_residual], frame_header->blocksize, frame_header->bits_per_sample, fixed_order, rice_parameter, max_partition_order, &(encoder->guts->candidate_subframe));
					if(candidate_bits < best_bits) {
						encoder_promote_candidate_subframe_(encoder);
						best_bits = candidate_bits;
					}
				}

				/* encode lpc */
				if(encoder->max_lpc_order > 0) {
					if(encoder->max_lpc_order >= frame_header->blocksize)
						max_lpc_order = frame_header->blocksize-1;
					else
						max_lpc_order = encoder->max_lpc_order;
					if(max_lpc_order > 0) {
						FLAC__lpc_compute_autocorrelation(real_signal[channel], frame_header->blocksize, max_lpc_order+1, autoc);
						FLAC__lpc_compute_lp_coefficients(autoc, max_lpc_order, lp_coeff, lpc_error);
						if(encoder->do_exhaustive_model_search) {
							min_lpc_order = 1;
						}
						else {
							unsigned guess_lpc_order = FLAC__lpc_compute_best_order(lpc_error, max_lpc_order, frame_header->blocksize, frame_header->bits_per_sample);
							min_lpc_order = max_lpc_order = guess_lpc_order;
						}
						if(encoder->do_qlp_coeff_prec_search) {
							min_qlp_coeff_precision = FLAC__MIN_QLP_COEFF_PRECISION;
							max_qlp_coeff_precision = 32 - frame_header->bits_per_sample - 1;
						}
						else {
							min_qlp_coeff_precision = max_qlp_coeff_precision = encoder->qlp_coeff_precision;
						}
						for(lpc_order = min_lpc_order; lpc_order <= max_lpc_order; lpc_order++) {
							lpc_residual_bits_per_sample = FLAC__lpc_compute_expected_bits_per_residual_sample(lpc_error[lpc_order-1], frame_header->blocksize);
							if(lpc_residual_bits_per_sample >= (real)frame_header->bits_per_sample)
								continue; /* don't even try */
							rice_parameter = (lpc_residual_bits_per_sample > 0.0)? (unsigned)(lpc_residual_bits_per_sample+0.5) : 0;
							if(rice_parameter >= (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN))
								rice_parameter = (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN) - 1;
							for(qlp_coeff_precision = min_qlp_coeff_precision; qlp_coeff_precision <= max_qlp_coeff_precision; qlp_coeff_precision++) {
								candidate_bits = encoder_evaluate_lpc_subframe_(integer_signal[channel], encoder->guts->residual[!encoder->guts->best_residual], lp_coeff[lpc_order-1], frame_header->blocksize, frame_header->bits_per_sample, lpc_order, qlp_coeff_precision, rice_parameter, max_partition_order, &(encoder->guts->candidate_subframe));
								if(candidate_bits > 0) { /* if == 0, there was a problem quantizing the lpcoeffs */
									if(candidate_bits < best_bits) {
										encoder_promote_candidate_subframe_(encoder);
										best_bits = candidate_bits;
									}
								}
							}
						}
					}
				}
			}
		}

		/* add the best subframe */
		switch(encoder->guts->best_subframe.type) {
			case FLAC__SUBFRAME_TYPE_CONSTANT:
				if(!encoder_generate_constant_subframe_(&(encoder->guts->best_subframe), frame_header->bits_per_sample, frame)) {
					encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_ENCODING;
					return false;
				}
				break;
			case FLAC__SUBFRAME_TYPE_FIXED:
				if(!encoder_generate_fixed_subframe_(&(encoder->guts->best_subframe), encoder->guts->residual[encoder->guts->best_residual], frame_header->blocksize, frame_header->bits_per_sample, frame)) {
					encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_ENCODING;
					return false;
				}
				break;
			case FLAC__SUBFRAME_TYPE_LPC:
				if(!encoder_generate_lpc_subframe_(&(encoder->guts->best_subframe), encoder->guts->residual[encoder->guts->best_residual], frame_header->blocksize, frame_header->bits_per_sample, frame)) {
					encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_ENCODING;
					return false;
				}
				break;
			case FLAC__SUBFRAME_TYPE_VERBATIM:
				if(!encoder_generate_verbatim_subframe_(&(encoder->guts->best_subframe), integer_signal[channel], frame_header->blocksize, frame_header->bits_per_sample, frame)) {
					encoder->state = FLAC__ENCODER_FATAL_ERROR_WHILE_ENCODING;
					return false;
				}
				break;
		}
	}

	return true;
}

unsigned encoder_evaluate_constant_subframe_(const int32 signal, unsigned bits_per_sample, FLAC__SubframeHeader *subframe)
{
	subframe->type = FLAC__SUBFRAME_TYPE_CONSTANT;
	subframe->data.constant.value = signal;

	return 8 + bits_per_sample;
}

unsigned encoder_evaluate_fixed_subframe_(const int32 signal[], int32 residual[], unsigned blocksize, unsigned bits_per_sample, unsigned order, unsigned rice_parameter, unsigned max_partition_order, FLAC__SubframeHeader *subframe)
{
	unsigned i, residual_bits;
	const unsigned residual_samples = blocksize - order;

	FLAC__fixed_compute_residual(signal+order, residual_samples, order, residual);

	subframe->type = FLAC__SUBFRAME_TYPE_FIXED;

	subframe->data.fixed.entropy_coding_method.type = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE;

	residual_bits = encoder_find_best_partition_order_(residual, residual_samples, order, rice_parameter, max_partition_order, &subframe->data.fixed.entropy_coding_method.data.partitioned_rice.order, subframe->data.fixed.entropy_coding_method.data.partitioned_rice.parameters);

	subframe->data.fixed.order = order;
	for(i = 0; i < order; i++)
		subframe->data.fixed.warmup[i] = signal[i];

	return 8 + (order * bits_per_sample) + residual_bits;
}

unsigned encoder_evaluate_lpc_subframe_(const int32 signal[], int32 residual[], const real lp_coeff[], unsigned blocksize, unsigned bits_per_sample, unsigned order, unsigned qlp_coeff_precision, unsigned rice_parameter, unsigned max_partition_order, FLAC__SubframeHeader *subframe)
{
	int32 qlp_coeff[FLAC__MAX_LPC_ORDER];
	unsigned i, residual_bits;
	int quantization, ret;
	const unsigned residual_samples = blocksize - order;

	ret = FLAC__lpc_quantize_coefficients(lp_coeff, order, qlp_coeff_precision, bits_per_sample, qlp_coeff, &quantization);
	if(ret != 0)
		return 0; /* this is a hack to indicate to the caller that we can't do lp at this order on this subframe */

	FLAC__lpc_compute_residual_from_qlp_coefficients(signal+order, residual_samples, qlp_coeff, order, quantization, residual);

	subframe->type = FLAC__SUBFRAME_TYPE_LPC;

	subframe->data.lpc.entropy_coding_method.type = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE;

	residual_bits = encoder_find_best_partition_order_(residual, residual_samples, order, rice_parameter, max_partition_order, &subframe->data.lpc.entropy_coding_method.data.partitioned_rice.order, subframe->data.lpc.entropy_coding_method.data.partitioned_rice.parameters);

	subframe->data.lpc.order = order;
	subframe->data.lpc.qlp_coeff_precision = qlp_coeff_precision;
	subframe->data.lpc.quantization_level = quantization;
	memcpy(subframe->data.lpc.qlp_coeff, qlp_coeff, sizeof(int32)*FLAC__MAX_LPC_ORDER);
	for(i = 0; i < order; i++)
		subframe->data.lpc.warmup[i] = signal[i];

	return 8 + 9 + (order * (qlp_coeff_precision + bits_per_sample)) + residual_bits;
}

unsigned encoder_evaluate_verbatim_subframe_(unsigned blocksize, unsigned bits_per_sample, FLAC__SubframeHeader *subframe)
{
	subframe->type = FLAC__SUBFRAME_TYPE_VERBATIM;

	return 8 + (blocksize * bits_per_sample);
}

unsigned encoder_find_best_partition_order_(int32 residual[], unsigned residual_samples, unsigned predictor_order, unsigned rice_parameter, unsigned max_partition_order, unsigned *best_partition_order, unsigned best_parameters[])
{
	unsigned residual_bits, best_residual_bits = 0;
	unsigned partition_order;
	unsigned best_parameters_index = 0, parameters[2][1 << FLAC__MAX_RICE_PARTITION_ORDER];

	for(partition_order = 0; partition_order <= max_partition_order; partition_order++) {
		if(!encoder_set_partitioned_rice_(residual, residual_samples, predictor_order, rice_parameter, partition_order, parameters[!best_parameters_index], &residual_bits)) {
			assert(best_residual_bits != 0);
			break;
		}
		if(best_residual_bits == 0 || residual_bits < best_residual_bits) {
			best_residual_bits = residual_bits;
			*best_partition_order = partition_order;
			best_parameters_index = !best_parameters_index;
		}
	}
	memcpy(best_parameters, parameters[best_parameters_index], sizeof(unsigned)*(1<<(*best_partition_order)));

	return best_residual_bits;
}

bool encoder_generate_constant_subframe_(const FLAC__SubframeHeader *header, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer)
{
	assert(header->type == FLAC__SUBFRAME_TYPE_CONSTANT);
	return FLAC__subframe_add_constant(bits_per_sample, header, bitbuffer);
}

bool encoder_generate_fixed_subframe_(const FLAC__SubframeHeader *header, int32 residual[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer)
{
	assert(header->type == FLAC__SUBFRAME_TYPE_FIXED);
	return FLAC__subframe_add_fixed(residual, blocksize - header->data.fixed.order, bits_per_sample, header, bitbuffer);
}

bool encoder_generate_lpc_subframe_(const FLAC__SubframeHeader *header, int32 residual[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer)
{
	assert(header->type == FLAC__SUBFRAME_TYPE_LPC);
	return FLAC__subframe_add_lpc(residual, blocksize - header->data.lpc.order, bits_per_sample, header, bitbuffer);
}

bool encoder_generate_verbatim_subframe_(const FLAC__SubframeHeader *header, const int32 signal[], unsigned blocksize, unsigned bits_per_sample, FLAC__BitBuffer *bitbuffer)
{
	assert(header->type == FLAC__SUBFRAME_TYPE_VERBATIM);
#ifdef NDEBUG
	(void)header; /* silence compiler warning about unused parameter */
#endif
	return FLAC__subframe_add_verbatim(signal, blocksize, bits_per_sample, bitbuffer);
}

void encoder_promote_candidate_subframe_(FLAC__Encoder *encoder)
{
	assert(encoder->state == FLAC__ENCODER_OK);
	encoder->guts->best_subframe = encoder->guts->candidate_subframe;
	encoder->guts->best_residual = !encoder->guts->best_residual;
}

bool encoder_set_partitioned_rice_(const int32 residual[], const unsigned residual_samples, const unsigned predictor_order, const unsigned rice_parameter, const unsigned partition_order, unsigned parameters[], unsigned *bits)
{
	unsigned bits_ = 2 + 3;

	if(partition_order == 0) {
		unsigned i;
		parameters[0] = rice_parameter;
		bits_ += FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN;
		for(i = 0; i < residual_samples; i++)
			bits_ += RICE_BITS(residual[i], rice_parameter);
	}
	else {
		unsigned i, j, k = 0, k_last = 0, z;
		unsigned mean;
		unsigned parameter, partition_samples;
		const unsigned max_parameter = (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN) - 1;
		for(i = 0; i < (1u<<partition_order); i++) {
			partition_samples = (residual_samples+predictor_order) >> partition_order;
			if(i == 0) {
				if(partition_samples <= predictor_order)
					return false;
				else
					partition_samples -= predictor_order;
			}
			mean = partition_samples >> 1;
			for(j = 0; j < partition_samples; j++, k++)
				mean += ((residual[k] < 0)? (unsigned)(-residual[k]) : (unsigned)residual[k]);
			mean /= partition_samples;
			z = 0x80000000;
			for(j = 0; j < 32; j++, z >>= 1)
				if(mean & z)
					break;
			parameter = j > 31? 0 : 32 - j - 1;
			if(parameter > max_parameter)
				parameter = max_parameter;
			parameters[i] = parameter;
			bits_ += FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN;
			for(j = k_last; j < k; j++)
				bits_ += RICE_BITS(residual[j], parameter);
			k_last = k;
		}
	}

	*bits = bits_;
	return true;
}
