/* libOggFLAC++ - Free Lossless Audio Codec + Ogg library
 * Copyright (C) 2002,2003  Josh Coalson
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

#ifndef OggFLACPP__DECODER_H
#define OggFLACPP__DECODER_H

#include "export.h"

#include "OggFLAC/stream_decoder.h"
// we only need this for the state abstraction really...
#include "FLAC++/decoder.h"


/** \file include/OggFLAC++/decoder.h
 *
 *  \brief
 *  This module contains the classes which implement the various
 *  decoders.
 *
 *  See the detailed documentation in the
 *  \link oggflacpp_decoder decoder \endlink module.
 */

/** \defgroup oggflacpp_decoder OggFLAC++/decoder.h: decoder classes
 *  \ingroup oggflacpp
 *
 *  \brief
 *  This module describes the decoder layers provided by libOggFLAC++.
 *
 * The libOggFLAC++ decoder classes are object wrappers around their
 * counterparts in libOggFLAC.  Only the stream decoding layer in
 * libOggFLAC provided here.  The interface is very similar;
 * make sure to read the \link oggflac_decoder libOggFLAC decoder module \endlink.
 *
 * The only real difference here is that instead of passing in C function
 * pointers for callbacks, you inherit from the decoder class and provide
 * implementations for the callbacks in the derived class; because of this
 * there is no need for a 'client_data' property.
 */

namespace OggFLAC {
	namespace Decoder {

		// ============================================================
		//
		//  Equivalent: OggFLAC__StreamDecoder
		//
		// ============================================================

		/** \defgroup oggflacpp_stream_decoder OggFLAC++/decoder.h: stream decoder class
		 *  \ingroup oggflacpp_decoder
		 *
		 *  \brief
		 *  This class wraps the ::OggFLAC__StreamDecoder.
		 *
		 * See the \link oggflac_stream_decoder libOggFLAC stream decoder module \endlink.
		 *
		 * \{
		 */

		/** This class wraps the ::OggFLAC__StreamDecoder.
		 */
		class OggFLACPP_API Stream {
		public:
			class OggFLACPP_API State {
			public:
				inline State(::OggFLAC__StreamDecoderState state): state_(state) { }
				inline operator ::OggFLAC__StreamDecoderState() const { return state_; }
				inline const char *as_cstring() const { return ::OggFLAC__StreamDecoderStateString[state_]; }
			protected:
				::OggFLAC__StreamDecoderState state_;
			};

			Stream();
			virtual ~Stream();

			bool is_valid() const;
			inline operator bool() const { return is_valid(); }

			bool set_serial_number(long value);
			bool set_metadata_respond(::FLAC__MetadataType type);
			bool set_metadata_respond_application(const FLAC__byte id[4]);
			bool set_metadata_respond_all();
			bool set_metadata_ignore(::FLAC__MetadataType type);
			bool set_metadata_ignore_application(const FLAC__byte id[4]);
			bool set_metadata_ignore_all();

			State get_state() const;
			FLAC::Decoder::Stream::State get_FLAC_stream_decoder_state() const;
			unsigned get_channels() const;
			::FLAC__ChannelAssignment get_channel_assignment() const;
			unsigned get_bits_per_sample() const;
			unsigned get_sample_rate() const;
			unsigned get_blocksize() const;

			State init();

			void finish();

			bool flush();
			bool reset();

			bool process_single();
			bool process_until_end_of_metadata();
			bool process_until_end_of_stream();
		protected:
			virtual ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes) = 0;
			virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) = 0;
			virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) = 0;
			virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) = 0;

			::OggFLAC__StreamDecoder *decoder_;
		private:
			static ::FLAC__StreamDecoderReadStatus read_callback_(const ::OggFLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data);
			static ::FLAC__StreamDecoderWriteStatus write_callback_(const ::OggFLAC__StreamDecoder *decoder, const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
			static void metadata_callback_(const ::OggFLAC__StreamDecoder *decoder, const ::FLAC__StreamMetadata *metadata, void *client_data);
			static void error_callback_(const ::OggFLAC__StreamDecoder *decoder, ::FLAC__StreamDecoderErrorStatus status, void *client_data);

			// Private and undefined so you can't use them:
			Stream(const Stream &);
			void operator=(const Stream &);
		};

		/* \} */

	};
};

#endif
