/* test_libOggFLAC - Unit tester for libOggFLAC
 * Copyright (C) 2002,2003  Josh Coalson
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
 */

#include "decoders.h"
#include "file_utils.h"
#include "metadata_utils.h"
#include "FLAC/assert.h"
#include "OggFLAC/stream_decoder.h"
#include "share/grabbag.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	FILE *file;
	unsigned current_metadata_number;
	FLAC__bool ignore_errors;
	FLAC__bool error_occurred;
} stream_decoder_client_data_struct;

static FLAC__StreamMetadata streaminfo_, padding_, seektable_, application1_, application2_, vorbiscomment_, cuesheet_, unknown_;
static FLAC__StreamMetadata *expected_metadata_sequence_[8];
static unsigned num_expected_;
static const char *oggflacfilename_ = "metadata.ogg";
static unsigned oggflacfilesize_;

static FLAC__bool die_(const char *msg)
{
	printf("ERROR: %s\n", msg);
	return false;
}

static FLAC__bool die_s_(const char *msg, const OggFLAC__StreamDecoder *decoder)
{
	OggFLAC__StreamDecoderState state = OggFLAC__stream_decoder_get_state(decoder);

	if(msg)
		printf("FAILED, %s", msg);
	else
		printf("FAILED");

	printf(", state = %u (%s)\n", (unsigned)state, OggFLAC__StreamDecoderStateString[state]);
	if(state == OggFLAC__STREAM_DECODER_FLAC_STREAM_DECODER_ERROR) {
		FLAC__StreamDecoderState state_ = OggFLAC__stream_decoder_get_FLAC_stream_decoder_state(decoder);
		printf("      FLAC stream decoder state = %u (%s)\n", (unsigned)state_, FLAC__StreamDecoderStateString[state_]);
	}

	return false;
}

static void init_metadata_blocks_()
{
	mutils__init_metadata_blocks(&streaminfo_, &padding_, &seektable_, &application1_, &application2_, &vorbiscomment_, &cuesheet_, &unknown_);
}

static void free_metadata_blocks_()
{
	mutils__free_metadata_blocks(&streaminfo_, &padding_, &seektable_, &application1_, &application2_, &vorbiscomment_, &cuesheet_, &unknown_);
}

static FLAC__bool generate_file_()
{
	printf("\n\ngenerating Ogg FLAC file for decoder tests...\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!file_utils__generate_oggflacfile(oggflacfilename_, &oggflacfilesize_, 512 * 1024, &streaminfo_, expected_metadata_sequence_, num_expected_))
		return die_("creating the encoded file");

	return true;
}

static FLAC__StreamDecoderReadStatus stream_decoder_read_callback_(const OggFLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data)
{
	stream_decoder_client_data_struct *dcd = (stream_decoder_client_data_struct*)client_data;

	(void)decoder;

	if(0 == dcd) {
		printf("ERROR: client_data in read callback is NULL\n");
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	if(dcd->error_occurred)
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

	if(feof(dcd->file))
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	else if(*bytes > 0) {
		unsigned bytes_read = fread(buffer, 1, *bytes, dcd->file);
		if(bytes_read == 0) {
			if(feof(dcd->file))
				return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
			else
				return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
		else {
			*bytes = bytes_read;
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	}
	else
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT; /* abort to avoid a deadlock */
}

static FLAC__StreamDecoderWriteStatus stream_decoder_write_callback_(const OggFLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	stream_decoder_client_data_struct *dcd = (stream_decoder_client_data_struct*)client_data;

	(void)decoder, (void)buffer;

	if(0 == dcd) {
		printf("ERROR: client_data in write callback is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if(dcd->error_occurred)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if(
		(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER && frame->header.number.frame_number == 0) ||
		(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER && frame->header.number.sample_number == 0)
	) {
		printf("content... ");
		fflush(stdout);
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void stream_decoder_metadata_callback_(const OggFLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	stream_decoder_client_data_struct *dcd = (stream_decoder_client_data_struct*)client_data;

	(void)decoder;

	if(0 == dcd) {
		printf("ERROR: client_data in metadata callback is NULL\n");
		return;
	}

	if(dcd->error_occurred)
		return;

	printf("%d... ", dcd->current_metadata_number);
	fflush(stdout);

	if(dcd->current_metadata_number >= num_expected_) {
		(void)die_("got more metadata blocks than expected");
		dcd->error_occurred = true;
	}
	else {
		if(!mutils__compare_block(expected_metadata_sequence_[dcd->current_metadata_number], metadata)) {
			(void)die_("metadata block mismatch");
			dcd->error_occurred = true;
		}
	}
	dcd->current_metadata_number++;
}

static void stream_decoder_error_callback_(const OggFLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	stream_decoder_client_data_struct *dcd = (stream_decoder_client_data_struct*)client_data;

	(void)decoder;

	if(0 == dcd) {
		printf("ERROR: client_data in error callback is NULL\n");
		return;
	}

	if(!dcd->ignore_errors) {
		printf("ERROR: got error callback: err = %u (%s)\n", (unsigned)status, FLAC__StreamDecoderErrorStatusString[status]);
		dcd->error_occurred = true;
	}
}

static FLAC__bool stream_decoder_test_respond_(OggFLAC__StreamDecoder *decoder, stream_decoder_client_data_struct *dcd)
{
	if(!OggFLAC__stream_decoder_set_read_callback(decoder, stream_decoder_read_callback_))
		return die_s_("at OggFLAC__stream_decoder_set_read_callback(), returned false", decoder);

	if(!OggFLAC__stream_decoder_set_write_callback(decoder, stream_decoder_write_callback_))
		return die_s_("at OggFLAC__stream_decoder_set_write_callback(), returned false", decoder);

	if(!OggFLAC__stream_decoder_set_metadata_callback(decoder, stream_decoder_metadata_callback_))
		return die_s_("at OggFLAC__stream_decoder_set_metadata_callback(), returned false", decoder);

	if(!OggFLAC__stream_decoder_set_error_callback(decoder, stream_decoder_error_callback_))
		return die_s_("at OggFLAC__stream_decoder_set_error_callback(), returned false", decoder);

	if(!OggFLAC__stream_decoder_set_client_data(decoder, dcd))
		return die_s_("at OggFLAC__stream_decoder_set_client_data(), returned false", decoder);

	printf("testing OggFLAC__stream_decoder_init()... ");
	if(OggFLAC__stream_decoder_init(decoder) != OggFLAC__STREAM_DECODER_OK)
		return die_s_(0, decoder);
	printf("OK\n");

	dcd->current_metadata_number = 0;

	if(fseek(dcd->file, 0, SEEK_SET) < 0) {
		printf("FAILED rewinding input, errno = %d\n", errno);
		return false;
	}

	printf("testing OggFLAC__stream_decoder_process_until_end_of_stream()... ");
	if(!OggFLAC__stream_decoder_process_until_end_of_stream(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_finish()... ");
	OggFLAC__stream_decoder_finish(decoder);
	printf("OK\n");

	return true;
}

static FLAC__bool test_stream_decoder()
{
	OggFLAC__StreamDecoder *decoder;
	OggFLAC__StreamDecoderState state;
	stream_decoder_client_data_struct decoder_client_data;

	printf("\n+++ libOggFLAC unit test: OggFLAC__StreamDecoder\n\n");

	printf("testing OggFLAC__stream_decoder_new()... ");
	decoder = OggFLAC__stream_decoder_new();
	if(0 == decoder) {
		printf("FAILED, returned NULL\n");
		return false;
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_delete()... ");
	OggFLAC__stream_decoder_delete(decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_new()... ");
	decoder = OggFLAC__stream_decoder_new();
	if(0 == decoder) {
		printf("FAILED, returned NULL\n");
		return false;
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_init()... ");
	if(OggFLAC__stream_decoder_init(decoder) == OggFLAC__STREAM_DECODER_OK)
		return die_s_(0, decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_delete()... ");
	OggFLAC__stream_decoder_delete(decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;

	printf("testing OggFLAC__stream_decoder_new()... ");
	decoder = OggFLAC__stream_decoder_new();
	if(0 == decoder) {
		printf("FAILED, returned NULL\n");
		return false;
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_serial_number()... ");
	if(!OggFLAC__stream_decoder_set_serial_number(decoder, file_utils__serial_number))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_read_callback()... ");
	if(!OggFLAC__stream_decoder_set_read_callback(decoder, stream_decoder_read_callback_))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_write_callback()... ");
	if(!OggFLAC__stream_decoder_set_write_callback(decoder, stream_decoder_write_callback_))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_callback()... ");
	if(!OggFLAC__stream_decoder_set_metadata_callback(decoder, stream_decoder_metadata_callback_))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_error_callback()... ");
	if(!OggFLAC__stream_decoder_set_error_callback(decoder, stream_decoder_error_callback_))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_client_data()... ");
	if(!OggFLAC__stream_decoder_set_client_data(decoder, &decoder_client_data))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_init()... ");
	if(OggFLAC__stream_decoder_init(decoder) != OggFLAC__STREAM_DECODER_OK)
		return die_s_(0, decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_get_state()... ");
	state = OggFLAC__stream_decoder_get_state(decoder);
	printf("returned state = %u (%s)... OK\n", state, OggFLAC__StreamDecoderStateString[state]);

	decoder_client_data.current_metadata_number = 0;
	decoder_client_data.ignore_errors = false;
	decoder_client_data.error_occurred = false;

	printf("opening Ogg FLAC file... ");
	decoder_client_data.file = fopen(oggflacfilename_, "rb");
	if(0 == decoder_client_data.file) {
		printf("ERROR\n");
		return false;
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_process_until_end_of_metadata()... ");
	if(!OggFLAC__stream_decoder_process_until_end_of_metadata(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_process_single()... ");
	if(!OggFLAC__stream_decoder_process_single(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_flush()... ");
	if(!OggFLAC__stream_decoder_flush(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	decoder_client_data.ignore_errors = true;
	printf("testing OggFLAC__stream_decoder_process_single()... ");
	if(!OggFLAC__stream_decoder_process_single(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");
	decoder_client_data.ignore_errors = false;

	printf("testing OggFLAC__stream_decoder_process_until_end_of_stream()... ");
	if(!OggFLAC__stream_decoder_process_until_end_of_stream(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_get_channels()... ");
	{
		unsigned channels = OggFLAC__stream_decoder_get_channels(decoder);
		if(channels != streaminfo_.data.stream_info.channels) {
			printf("FAILED, returned %u, expected %u\n", channels, streaminfo_.data.stream_info.channels);
			return false;
		}
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_get_bits_per_sample()... ");
	{
		unsigned bits_per_sample = OggFLAC__stream_decoder_get_bits_per_sample(decoder);
		if(bits_per_sample != streaminfo_.data.stream_info.bits_per_sample) {
			printf("FAILED, returned %u, expected %u\n", bits_per_sample, streaminfo_.data.stream_info.bits_per_sample);
			return false;
		}
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_get_sample_rate()... ");
	{
		unsigned sample_rate = OggFLAC__stream_decoder_get_sample_rate(decoder);
		if(sample_rate != streaminfo_.data.stream_info.sample_rate) {
			printf("FAILED, returned %u, expected %u\n", sample_rate, streaminfo_.data.stream_info.sample_rate);
			return false;
		}
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_get_blocksize()... ");
	{
		unsigned blocksize = OggFLAC__stream_decoder_get_blocksize(decoder);
		/* value could be anything since we're at the last block, so accept any answer */
		printf("returned %u... OK\n", blocksize);
	}

	printf("testing OggFLAC__stream_decoder_get_channel_assignment()... ");
	{
		FLAC__ChannelAssignment ca = OggFLAC__stream_decoder_get_channel_assignment(decoder);
		printf("returned %u (%s)... OK\n", (unsigned)ca, FLAC__ChannelAssignmentString[ca]);
	}

	printf("testing OggFLAC__stream_decoder_reset()... ");
	if(!OggFLAC__stream_decoder_reset(decoder)) {
		state = OggFLAC__stream_decoder_get_state(decoder);
		printf("FAILED, returned false, state = %u (%s)\n", state, FLAC__StreamDecoderStateString[state]);
		return false;
	}
	printf("OK\n");

	decoder_client_data.current_metadata_number = 0;

	printf("rewinding input... ");
	if(fseek(decoder_client_data.file, 0, SEEK_SET) < 0) {
		printf("FAILED, errno = %d\n", errno);
		return false;
	}
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_process_until_end_of_stream()... ");
	if(!OggFLAC__stream_decoder_process_until_end_of_stream(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_finish()... ");
	OggFLAC__stream_decoder_finish(decoder);
	printf("OK\n");

	/*
	 * respond all
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * respond all, ignore VORBIS_COMMENT
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore(VORBIS_COMMENT)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * respond all, ignore APPLICATION
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore(APPLICATION)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore(decoder, FLAC__METADATA_TYPE_APPLICATION))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * respond all, ignore APPLICATION id of app#1
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application2_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * respond all, ignore APPLICATION id of app#1 & app#2
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_application(of app block #2)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_application(decoder, application2_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all, respond VORBIS_COMMENT
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond(VORBIS_COMMENT)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all, respond APPLICATION
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond(APPLICATION)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_APPLICATION))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all, respond APPLICATION id of app#1
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &application1_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all, respond APPLICATION id of app#1 & app#2
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_application(of app block #2)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_application(decoder, application2_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * respond all, ignore APPLICATION, respond APPLICATION id of app#1
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore(APPLICATION)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore(decoder, FLAC__METADATA_TYPE_APPLICATION))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/*
	 * ignore all, respond APPLICATION, ignore APPLICATION id of app#1
	 */

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_all()... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_all(decoder))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_respond(APPLICATION)... ");
	if(!OggFLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_APPLICATION))
		return die_s_("returned false", decoder);
	printf("OK\n");

	printf("testing OggFLAC__stream_decoder_set_metadata_ignore_application(of app block #1)... ");
	if(!OggFLAC__stream_decoder_set_metadata_ignore_application(decoder, application1_.data.application.id))
		return die_s_("returned false", decoder);
	printf("OK\n");

	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &application2_;

	if(!stream_decoder_test_respond_(decoder, &decoder_client_data))
		return false;

	/* done, now leave the sequence the way we found it... */
	num_expected_ = 0;
	expected_metadata_sequence_[num_expected_++] = &streaminfo_;
	expected_metadata_sequence_[num_expected_++] = &padding_;
	expected_metadata_sequence_[num_expected_++] = &seektable_;
	expected_metadata_sequence_[num_expected_++] = &application1_;
	expected_metadata_sequence_[num_expected_++] = &application2_;
	expected_metadata_sequence_[num_expected_++] = &vorbiscomment_;
	expected_metadata_sequence_[num_expected_++] = &cuesheet_;
	expected_metadata_sequence_[num_expected_++] = &unknown_;

	printf("testing OggFLAC__stream_decoder_delete()... ");
	OggFLAC__stream_decoder_delete(decoder);
	printf("OK\n");

	fclose(decoder_client_data.file);

	printf("\nPASSED!\n");

	return true;
}

FLAC__bool test_decoders()
{
	init_metadata_blocks_();
	if(!generate_file_())
		return false;

	if(!test_stream_decoder())
		return false;

	(void) grabbag__file_remove_file(oggflacfilename_);
	free_metadata_blocks_();

	return true;
}
