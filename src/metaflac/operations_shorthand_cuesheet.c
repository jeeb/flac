/* metaflac - Command-line FLAC metadata editor
 * Copyright (C) 2001,2002  Josh Coalson
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

#include "options.h"
#include "utils.h"
#include "FLAC/assert.h"
#include "share/grabbag.h"
#include <string.h>

static FLAC__bool import_cs_from(const char *filename, FLAC__StreamMetadata **cuesheet, const Argument_Filename *cs_filename, FLAC__bool *needs_write, FLAC__uint64 lead_out_offset, FLAC__StreamMetadata *seektable);
static FLAC__bool export_cs_to(const char *filename, FLAC__StreamMetadata *cuesheet, const Argument_Filename *cs_filename);

FLAC__bool do_shorthand_operation__cuesheet(const char *filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write, FLAC__bool cued_seekpoints)
{
	FLAC__bool ok = true;
	FLAC__StreamMetadata *cuesheet = 0, *seektable = 0;
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
	FLAC__uint64 lead_out_offset;

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	do {
		FLAC__StreamMetadata *block = FLAC__metadata_iterator_get_block(iterator);
		if(block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			lead_out_offset = block->data.stream_info.total_samples;
			if(lead_out_offset == 0) {
				fprintf(stderr, "%s: ERROR: FLAC file must have total_samples set in STREAMINFO in order to import/export cuesheet\n", filename);
				FLAC__metadata_iterator_delete(iterator);
				return false;
			}
			if(block->data.stream_info.sample_rate != 44100) {
				fprintf(stderr, "%s: ERROR: FLAC stream must currently be 44.1kHz in order to import/export cuesheet\n", filename);
				FLAC__metadata_iterator_delete(iterator);
				return false;
			}
		}
		else if(block->type == FLAC__METADATA_TYPE_CUESHEET)
			cuesheet = block;
		else if(block->type == FLAC__METADATA_TYPE_SEEKTABLE)
			seektable = block;
	} while(FLAC__metadata_iterator_next(iterator));

	switch(operation->type) {
		case OP__IMPORT_CUESHEET_FROM:
			if(0 != cuesheet) {
				fprintf(stderr, "%s: ERROR: FLAC file already has CUESHEET block\n", filename);
				ok = false;
			}
			else {
				/* create a new SEEKTABLE block if necessary */
				if(cued_seekpoints && 0 == seektable) {
					seektable = FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE);
					if(0 == seektable)
						die("out of memory allocating SEEKTABLE block");
					while(FLAC__metadata_iterator_prev(iterator))
						;
					if(!FLAC__metadata_iterator_insert_block_after(iterator, seektable)) {
						fprintf(stderr, "%s: ERROR: adding new SEEKTABLE block to metadata, status =\"%s\"\n", filename, FLAC__Metadata_ChainStatusString[FLAC__metadata_chain_status(chain)]);
						FLAC__metadata_object_delete(seektable);
						FLAC__metadata_object_delete(cuesheet);
						ok = false;
					}
				}
				else
					seektable = 0;
				ok = import_cs_from(filename, &cuesheet, &operation->argument.filename, needs_write, lead_out_offset, seektable);
				if(!ok && 0 != seektable)
					FLAC__metadata_object_delete(seektable);
				else {
					/* append CUESHEET block */
					while(FLAC__metadata_iterator_next(iterator))
						;
					if(!FLAC__metadata_iterator_insert_block_after(iterator, cuesheet)) {
						fprintf(stderr, "%s: ERROR: adding new CUESHEET block to metadata, status =\"%s\"\n", filename, FLAC__Metadata_ChainStatusString[FLAC__metadata_chain_status(chain)]);
						FLAC__metadata_object_delete(cuesheet);
						ok = false;
					}
				}
			}
			break;
		case OP__EXPORT_CUESHEET_TO:
			if(0 == cuesheet) {
				fprintf(stderr, "%s: ERROR: FLAC file has no CUESHEET block\n", filename);
				ok = false;
			}
			else
				ok = export_cs_to(filename, cuesheet, &operation->argument.filename);
			break;
		default:
			ok = false;
			FLAC__ASSERT(0);
			break;
	};

	FLAC__metadata_iterator_delete(iterator);
	return ok;
}

/*
 * local routines
 */

FLAC__bool import_cs_from(const char *filename, FLAC__StreamMetadata **cuesheet, const Argument_Filename *cs_filename, FLAC__bool *needs_write, FLAC__uint64 lead_out_offset, FLAC__StreamMetadata *seektable)
{
	FILE *f;
	const char *error_message;
	unsigned last_line_read;

	if(0 == cs_filename->value || strlen(cs_filename->value) == 0) {
		fprintf(stderr, "%s: ERROR: empty import file name\n", filename);
		return false;
	}
	if(0 == strcmp(cs_filename->value, "-"))
		f = stdin;
	else
		f = fopen(cs_filename->value, "r");

	if(0 == f) {
		fprintf(stderr, "%s: ERROR: can't open import file %s\n", filename, cs_filename->value);
		return false;
	}

	*cuesheet = grabbag__cuesheet_parse(f, &error_message, &last_line_read, /*is_cdda=*/true, lead_out_offset);

	if(f != stdin)
		fclose(f);

	if(0 == *cuesheet) {
		fprintf(stderr, "%s: ERROR: while parsing cuesheet, line %u, %s\n", filename, last_line_read, error_message);
		return false;
	}

	/* add seekpoints for each index point if required */
	if(0 != seektable) {
		//@@@@
	}

	*needs_write = true;
	return true;
}

FLAC__bool export_cs_to(const char *filename, FLAC__StreamMetadata *cuesheet, const Argument_Filename *cs_filename)
{
	FILE *f;

	if(0 == cs_filename->value || strlen(cs_filename->value) == 0) {
		fprintf(stderr, "%s: ERROR: empty export file name\n", filename);
		return false;
	}
	if(0 == strcmp(cs_filename->value, "-"))
		f = stdout;
	else
		f = fopen(cs_filename->value, "w");

	if(0 == f) {
		fprintf(stderr, "%s: ERROR: can't open export file %s\n", filename, cs_filename->value);
		return false;
	}

	grabbag__cuesheet_emit(f, cuesheet, "DUMMY.WAV", /*is_cdda=*/true);

	if(f != stdout)
		fclose(f);

	return true;
}
