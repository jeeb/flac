/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2001,2002,2003  Josh Coalson
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

#ifndef FLAC__METADATA_H
#define FLAC__METADATA_H

#include "export.h"
#include "format.h"

/******************************************************************************
	(For an example of how all these routines are used, see the source
	code for the unit tests in src/test_libFLAC/metadata_*.c, or metaflac
	in src/metaflac/)
******************************************************************************/

/** \file include/FLAC/metadata.h
 *
 *  \brief
 *  This module provides functions for creating and manipulating FLAC
 *  metadata blocks in memory, and three progressively more powerful
 *  interfaces for traversing and editing metadata in FLAC files.
 *
 *  See the detailed documentation for each interface in the
 *  \link flac_metadata metadata \endlink module.
 */

/** \defgroup flac_metadata FLAC/metadata.h: metadata interfaces
 *  \ingroup flac
 *
 *  \brief
 *  This module provides functions for creating and manipulating FLAC
 *  metadata blocks in memory, and three progressively more powerful
 *  interfaces for traversing and editing metadata in FLAC files.
 *
 *  There are three metadata interfaces of increasing complexity:
 *
 *  Level 0:
 *  Read-only access to the STREAMINFO block.
 *
 *  Level 1:
 *  Read-write access to all metadata blocks.  This level is write-
 *  efficient in most cases (more on this below), and uses less memory
 *  than level 2.
 *
 *  Level 2:
 *  Read-write access to all metadata blocks.  This level is write-
 *  efficient in all cases, but uses more memory since all metadata for
 *  the whole file is read into memory and manipulated before writing
 *  out again.
 *
 *  What do we mean by efficient?  When writing metadata back to a FLAC
 *  file it is possible to grow or shrink the metadata such that the entire
 *  file must be rewritten.  However, if the size remains the same during
 *  changes or PADDING blocks are utilized, only the metadata needs to be
 *  overwritten, which is much faster.
 *
 *  Efficient means the whole file is rewritten at most one time, and only
 *  when necessary.  Level 1 is not efficient only in the case that you
 *  cause more than one metadata block to grow or shrink beyond what can
 *  be accomodated by padding.  In this case you should probably use level
 *  2, which allows you to edit all the metadata for a file in memory and
 *  write it out all at once.
 *
 *  All levels know how to skip over and not disturb an ID3v2 tag at the
 *  front of the file.
 *
 *  In addition to the three interfaces, this module defines functions for
 *  creating and manipulating various metadata objects in memory.  As we see
 *  from the Format module, FLAC metadata blocks in memory are very primitive
 *  structures for storing information in an efficient way.  Reading
 *  information from the structures is easy but creating or modifying them
 *  directly is more complex.  The metadata object routines here facilitate
 *  this by taking care of the consistency and memory management drudgery.
 *
 *  Unless you will be using the level 1 or 2 interfaces to modify existing
 *  metadata however, you will not probably not need these.
 *
 *  From a dependency standpoint, none of the encoders or decoders require
 *  the metadata module.  This is so that embedded users can strip out the
 *  metadata module from libFLAC to reduce the size and complexity.
 */

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup flac_metadata_level0 FLAC/metadata.h: metadata level 0 interface
 *  \ingroup flac_metadata
 *
 *  \brief
 *  The level 0 interface consists of a single routine to read the
 *  STREAMINFO block.
 *
 *  It skips any ID3v2 tag at the head of the file.
 *
 * \{
 */

/** Read the STREAMINFO metadata block of the given FLAC file.  This function
 *  will skip any ID3v2 tag at the head of the file.
 *
 * \param filename    The path to the FLAC file to read.
 * \param streaminfo  A pointer to space for the STREAMINFO block.
 * \assert
 *    \code filename != NULL \endcode
 *    \code streaminfo != NULL \endcode
 * \retval FLAC__bool
 *    \c true if a valid STREAMINFO block was read from \a filename.  Returns
 *    \c false if there was a memory allocation error, a file decoder error,
 *    or the file contained no STREAMINFO block.
 */
FLAC_API FLAC__bool FLAC__metadata_get_streaminfo(const char *filename, FLAC__StreamMetadata *streaminfo);

/* \} */


/** \defgroup flac_metadata_level1 FLAC/metadata.h: metadata level 1 interface
 *  \ingroup flac_metadata
 *
 * \brief
 * The level 1 interface provides read-write access to FLAC file metadata and
 * operates directly on the FLAC file.
 *
 * The general usage of this interface is:
 *
 * - Create an iterator using FLAC__metadata_simple_iterator_new()
 * - Attach it to a file using FLAC__metadata_simple_iterator_init() and check
 *   the exit code.  Call FLAC__metadata_simple_iterator_is_writable() to
 *   see if the file is writable, or read-only access is allowed.
 * - Use FLAC__metadata_simple_iterator_next() and
 *   FLAC__metadata_simple_iterator_prev() to move around the blocks.
 *   This is does not read the actual blocks themselves.
 *   FLAC__metadata_simple_iterator_next() is relatively fast.
 *   FLAC__metadata_simple_iterator_prev() is slower since it needs to search
 *   forward from the front of the file.
 * - Use FLAC__metadata_simple_iterator_get_block_type() or
 *   FLAC__metadata_simple_iterator_get_block() to access the actual data at
 *   the current iterator position.  The returned object is yours to modify
 *   and free.
 * - Use FLAC__metadata_simple_iterator_set_block() to write a modified block
 *   back.  You must have write permission to the original file.  Make sure to
 *   read the whole comment to FLAC__metadata_simple_iterator_set_block()
 *   below.
 * - Use FLAC__metadata_simple_iterator_insert_block_after() to add new blocks.
 *   Use the object creation functions from
 *   \link flac_metadata_object here \endlink to generate new objects.
 * - Use FLAC__metadata_simple_iterator_delete_block() to remove the block
 *   currently referred to by the iterator, or replace it with padding.
 * - Destroy the iterator with FLAC__metadata_simple_iterator_delete() when
 *   finished.
 *
 * \note
 * The FLAC file remains open the whole time between
 * FLAC__metadata_simple_iterator_init() and
 * FLAC__metadata_simple_iterator_delete(), so make sure you are not altering
 * the file during this time.
 *
 * \note
 * Do not modify the \a is_last, \a length, or \a type fields of returned
 * FLAC__MetadataType objects.  These are managed automatically.
 *
 * \note
 * If any of the modification functions
 * (FLAC__metadata_simple_iterator_set_block(),
 * FLAC__metadata_simple_iterator_delete_block(),
 * FLAC__metadata_simple_iterator_insert_block_after(), etc.) return \c false,
 * you should delete the iterator as it may no longer be valid.
 *
 * \{
 */

struct FLAC__Metadata_SimpleIterator;
/** The opaque structure definition for the level 1 iterator type.
 *  See the
 *  \link flac_metadata_level1 metadata level 1 module \endlink
 *  for a detailed description.
 */
typedef struct FLAC__Metadata_SimpleIterator FLAC__Metadata_SimpleIterator;

/** Status type for FLAC__Metadata_SimpleIterator.
 *
 *  The iterator's current status can be obtained by calling FLAC__metadata_simple_iterator_status().
 */
typedef enum {

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_OK = 0,
	/**< The iterator is in the normal OK state */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ILLEGAL_INPUT,
	/**< The data passed into a function violated the function's usage criteria */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ERROR_OPENING_FILE,
	/**< The iterator could not open the target file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_NOT_A_FLAC_FILE,
	/**< The iterator could not find the FLAC signature at the start of the file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_NOT_WRITABLE,
	/**< The iterator tried to write to a file that was not writable */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_BAD_METADATA,
	/**< The iterator encountered input that does not conform to the FLAC metadata specification */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_READ_ERROR,
	/**< The iterator encountered an error while reading the FLAC file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_SEEK_ERROR,
	/**< The iterator encountered an error while seeking in the FLAC file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_WRITE_ERROR,
	/**< The iterator encountered an error while writing the FLAC file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_RENAME_ERROR,
	/**< The iterator encountered an error renaming the FLAC file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_UNLINK_ERROR,
	/**< The iterator encountered an error removing the temporary file */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_MEMORY_ALLOCATION_ERROR,
	/**< Memory allocation failed */

	FLAC__METADATA_SIMPLE_ITERATOR_STATUS_INTERNAL_ERROR
	/**< The caller violated an assertion or an unexpected error occurred */

} FLAC__Metadata_SimpleIteratorStatus;

/** Maps a FLAC__Metadata_SimpleIteratorStatus to a C string.
 *
 *  Using a FLAC__Metadata_SimpleIteratorStatus as the index to this array
 *  will give the string equivalent.  The contents should not be modified.
 */
extern FLAC_API const char * const FLAC__Metadata_SimpleIteratorStatusString[];


/** Create a new iterator instance.
 *
 * \retval FLAC__Metadata_SimpleIterator*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__Metadata_SimpleIterator *FLAC__metadata_simple_iterator_new();

/** Free an iterator instance.  Deletes the object pointed to by \a iterator.
 *
 * \param iterator  A pointer to an existing iterator.
 * \assert
 *    \code iterator != NULL \endcode
 */
FLAC_API void FLAC__metadata_simple_iterator_delete(FLAC__Metadata_SimpleIterator *iterator);

/** Get the current status of the iterator.  Call this after a function
 *  returns \c false to get the reason for the error.  Also resets the status
 *  to FLAC__METADATA_SIMPLE_ITERATOR_STATUS_OK.
 *
 * \param iterator  A pointer to an existing iterator.
 * \assert
 *    \code iterator != NULL \endcode
 * \retval FLAC__Metadata_SimpleIteratorStatus
 *    The current status of the iterator.
 */
FLAC_API FLAC__Metadata_SimpleIteratorStatus FLAC__metadata_simple_iterator_status(FLAC__Metadata_SimpleIterator *iterator);

/** Initialize the iterator to point to the first metadata block in the
 *  given FLAC file.
 *
 * \param iterator             A pointer to an existing iterator.
 * \param filename             The path to the FLAC file.
 * \param read_only            If \c true, the FLAC file will be opened
 *                             in read-only mode; if \c false, the FLAC
 *                             file will be opened for edit even if no
 *                             edits are performed.
 * \param preserve_file_stats  If \c true, the owner and modification
 *                             time will be preserved even if the FLAC
 *                             file is written to.
 * \assert
 *    \code iterator != NULL \endcode
 *    \code filename != NULL \endcode
 * \retval FLAC__bool
 *    \c false if a memory allocation error occurs, the file can't be
 *    opened, or another error occurs, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_init(FLAC__Metadata_SimpleIterator *iterator, const char *filename, FLAC__bool read_only, FLAC__bool preserve_file_stats);

/** Returns \c true if the FLAC file is writable.  If \c false, calls to
 *  FLAC__metadata_simple_iterator_set_block() and
 *  FLAC__metadata_simple_iterator_insert_block_after() will fail.
 *
 * \param iterator             A pointer to an existing iterator.
 * \assert
 *    \code iterator != NULL \endcode
 * \retval FLAC__bool
 *    See above.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_is_writable(const FLAC__Metadata_SimpleIterator *iterator);

/** Moves the iterator forward one metadata block, returning \c false if
 *  already at the end.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 * \retval FLAC__bool
 *    \c false if already at the last metadata block of the chain, else
 *    \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_next(FLAC__Metadata_SimpleIterator *iterator);

/** Moves the iterator backward one metadata block, returning \c false if
 *  already at the beginning.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 * \retval FLAC__bool
 *    \c false if already at the first metadata block of the chain, else
 *    \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_prev(FLAC__Metadata_SimpleIterator *iterator);

/** Get the type of the metadata block at the current position.  This
 *  avoids reading the actual block data which can save time for large
 *  blocks.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 * \retval FLAC__MetadataType
 *    The type of the metadata block at the current iterator position.
 */

FLAC_API FLAC__MetadataType FLAC__metadata_simple_iterator_get_block_type(const FLAC__Metadata_SimpleIterator *iterator);

/** Get the metadata block at the current position.  You can modify the
 *  block but must use FLAC__metadata_simple_iterator_set_block() to
 *  write it back to the FLAC file.
 *
 *  You must call FLAC__metadata_object_delete() on the returned object
 *  when you are finished with it.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 * \retval FLAC__StreamMetadata*
 *    The current metadata block.
 */
FLAC_API FLAC__StreamMetadata *FLAC__metadata_simple_iterator_get_block(FLAC__Metadata_SimpleIterator *iterator);

/** Write a block back to the FLAC file.  This function tries to be
 *  as efficient as possible; how the block is actually written is
 *  shown by the following:
 *
 *  Existing block is a STREAMINFO block and the new block is a
 *  STREAMINFO block: the new block is written in place.  Make sure
 *  you know what you're doing when changing the values of a
 *  STREAMINFO block.
 *
 *  Existing block is a STREAMINFO block and the new block is a
 *  not a STREAMINFO block: this is an error since the first block
 *  must be a STREAMINFO block.  Returns \c false without altering the
 *  file.
 *
 *  Existing block is not a STREAMINFO block and the new block is a
 *  STREAMINFO block: this is an error since there may be only one
 *  STREAMINFO block.  Returns \c false without altering the file.
 *
 *  Existing block and new block are the same length: the existing
 *  block will be replaced by the new block, written in place.
 *
 *  Existing block is longer than new block: if use_padding is \c true,
 *  the existing block will be overwritten in place with the new
 *  block followed by a PADDING block, if possible, to make the total
 *  size the same as the existing block.  Remember that a padding
 *  block requires at least four bytes so if the difference in size
 *  between the new block and existing block is less than that, the
 *  entire file will have to be rewritten, using the new block's
 *  exact size.  If use_padding is \c false, the entire file will be
 *  rewritten, replacing the existing block by the new block.
 *
 *  Existing block is shorter than new block: if use_padding is \c true,
 *  the function will try and expand the new block into the following
 *  PADDING block, if it exists and doing so won't shrink the PADDING
 *  block to less than 4 bytes.  If there is no following PADDING
 *  block, or it will shrink to less than 4 bytes, or use_padding is
 *  \c false, the entire file is rewritten, replacing the existing block
 *  with the new block.  Note that in this case any following PADDING
 *  block is preserved as is.
 *
 *  After writing the block, the iterator will remain in the same
 *  place, i.e. pointing to the new block.
 *
 * \param iterator     A pointer to an existing initialized iterator.
 * \param block        The block to set.
 * \param use_padding  See above.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 *    \code block != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_set_block(FLAC__Metadata_SimpleIterator *iterator, FLAC__StreamMetadata *block, FLAC__bool use_padding);

/** This is similar to FLAC__metadata_simple_iterator_set_block()
 *  except that instead of writing over an existing block, it appends
 *  a block after the existing block.  \a use_padding is again used to
 *  tell the function to try an expand into following padding in an
 *  attempt to avoid rewriting the entire file.
 *
 *  This function will fail and return \c false if given a STREAMINFO
 *  block.
 *
 *  After writing the block, the iterator will be pointing to the
 *  new block.
 *
 * \param iterator     A pointer to an existing initialized iterator.
 * \param block        The block to set.
 * \param use_padding  See above.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 *    \code block != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_insert_block_after(FLAC__Metadata_SimpleIterator *iterator, FLAC__StreamMetadata *block, FLAC__bool use_padding);

/** Deletes the block at the current position.  This will cause the
 *  entire FLAC file to be rewritten, unless \a use_padding is \c true,
 *  in which case the block will be replaced by an equal-sized PADDING
 *  block.  The iterator will be left pointing to the block before the
 *  one just deleted.
 *
 *  You may not delete the STREAMINFO block.
 *
 * \param iterator     A pointer to an existing initialized iterator.
 * \param use_padding  See above.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_simple_iterator_init()
 * \retval FLAC__bool
 *    \c true if successful, else \c false.
 */
FLAC_API FLAC__bool FLAC__metadata_simple_iterator_delete_block(FLAC__Metadata_SimpleIterator *iterator, FLAC__bool use_padding);

/* \} */


/** \defgroup flac_metadata_level2 FLAC/metadata.h: metadata level 2 interface
 *  \ingroup flac_metadata
 *
 * \brief
 * The level 2 interface provides read-write access to FLAC file metadata;
 * all metadata is read into memory, operated on in memory, and then written
 * to file, which is more efficient than level 1 when editing multiple blocks.
 *
 * The general usage of this interface is:
 *
 * - Create a new chain using FLAC__metadata_chain_new().  A chain is a
 *   linked list of FLAC metadata blocks.
 * - Read all metadata into the the chain from a FLAC file using
 *   FLAC__metadata_chain_read() and check the status.
 * - Optionally, consolidate the padding using
 *   FLAC__metadata_chain_merge_padding() or
 *   FLAC__metadata_chain_sort_padding().
 * - Create a new iterator using FLAC__metadata_iterator_new()
 * - Initialize the iterator to point to the first element in the chain
 *   using FLAC__metadata_iterator_init()
 * - Traverse the chain using FLAC__metadata_iterator_next and
 *   FLAC__metadata_iterator_prev().
 * - Get a block for reading or modification using
 *   FLAC__metadata_iterator_get_block().  The pointer to the object
 *   inside the chain is returned, so the block is yours to modify.
 *   Changes will be reflected in the FLAC file when you write the
 *   chain.  You can also add and delete blocks (see functions below).
 * - When done, write out the chain using FLAC__metadata_chain_write().
 *   Make sure to read the whole comment to the function below.
 * - Delete the chain using FLAC__metadata_chain_delete().
 *
 * \note
 * Even though the FLAC file is not open while the chain is being
 * manipulated, you must not alter the file externally during
 * this time.  The chain assumes the FLAC file will not change
 * between the time of FLAC__metadata_chain_read() and
 * FLAC__metadata_chain_write().
 *
 * \note
 * Do not modify the is_last, length, or type fields of returned
 * FLAC__MetadataType objects.  These are managed automatically.
 *
 * \note
 * The metadata objects returned by FLAC__metadata_iterator_get_block()
 * are owned by the chain; do not FLAC__metadata_object_delete() them.
 * In the same way, blocks passed to FLAC__metadata_iterator_set_block()
 * become owned by the chain and they will be deleted when the chain is
 * deleted.
 *
 * \{
 */

struct FLAC__Metadata_Chain;
/** The opaque structure definition for the level 2 chain type.
 */
typedef struct FLAC__Metadata_Chain FLAC__Metadata_Chain;

struct FLAC__Metadata_Iterator;
/** The opaque structure definition for the level 2 iterator type.
 */
typedef struct FLAC__Metadata_Iterator FLAC__Metadata_Iterator;

typedef enum {
	FLAC__METADATA_CHAIN_STATUS_OK = 0,
	/**< The chain is in the normal OK state */

	FLAC__METADATA_CHAIN_STATUS_ILLEGAL_INPUT,
	/**< The data passed into a function violated the function's usage criteria */

	FLAC__METADATA_CHAIN_STATUS_ERROR_OPENING_FILE,
	/**< The chain could not open the target file */

	FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE,
	/**< The chain could not find the FLAC signature at the start of the file */

	FLAC__METADATA_CHAIN_STATUS_NOT_WRITABLE,
	/**< The chain tried to write to a file that was not writable */

	FLAC__METADATA_CHAIN_STATUS_BAD_METADATA,
	/**< The chain encountered input that does not conform to the FLAC metadata specification */

	FLAC__METADATA_CHAIN_STATUS_READ_ERROR,
	/**< The chain encountered an error while reading the FLAC file */

	FLAC__METADATA_CHAIN_STATUS_SEEK_ERROR,
	/**< The chain encountered an error while seeking in the FLAC file */

	FLAC__METADATA_CHAIN_STATUS_WRITE_ERROR,
	/**< The chain encountered an error while writing the FLAC file */

	FLAC__METADATA_CHAIN_STATUS_RENAME_ERROR,
	/**< The chain encountered an error renaming the FLAC file */

	FLAC__METADATA_CHAIN_STATUS_UNLINK_ERROR,
	/**< The chain encountered an error removing the temporary file */

	FLAC__METADATA_CHAIN_STATUS_MEMORY_ALLOCATION_ERROR,
	/**< Memory allocation failed */

	FLAC__METADATA_CHAIN_STATUS_INTERNAL_ERROR
	/**< The caller violated an assertion or an unexpected error occurred */

} FLAC__Metadata_ChainStatus;

/** Maps a FLAC__Metadata_ChainStatus to a C string.
 *
 *  Using a FLAC__Metadata_ChainStatus as the index to this array
 *  will give the string equivalent.  The contents should not be modified.
 */
extern FLAC_API const char * const FLAC__Metadata_ChainStatusString[];

/*********** FLAC__Metadata_Chain ***********/

/** Create a new chain instance.
 *
 * \retval FLAC__Metadata_Chain*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__Metadata_Chain *FLAC__metadata_chain_new();

/** Free a chain instance.  Deletes the object pointed to by \a chain.
 *
 * \param chain  A pointer to an existing chain.
 * \assert
 *    \code chain != NULL \endcode
 */
FLAC_API void FLAC__metadata_chain_delete(FLAC__Metadata_Chain *chain);

/** Get the current status of the chain.  Call this after a function
 *  returns \c false to get the reason for the error.  Also resets the
 *  status to FLAC__METADATA_CHAIN_STATUS_OK.
 *
 * \param chain    A pointer to an existing chain.
 * \assert
 *    \code chain != NULL \endcode
 * \retval FLAC__Metadata_ChainStatus
 *    The current status of the chain.
 */
FLAC_API FLAC__Metadata_ChainStatus FLAC__metadata_chain_status(FLAC__Metadata_Chain *chain);

/** Read all metadata from a FLAC file into the chain.
 *
 * \param chain    A pointer to an existing chain.
 * \param filename The path to the FLAC file to read.
 * \assert
 *    \code chain != NULL \endcode
 *    \code filename != NULL \endcode
 * \retval FLAC__bool
 *    \c true if a valid list of metadata blocks was read from
 *    \a filename, else \c false.  On failure, check the status with
 *    FLAC__metadata_chain_status().
 */
FLAC_API FLAC__bool FLAC__metadata_chain_read(FLAC__Metadata_Chain *chain, const char *filename);

/** Write all metadata out to the FLAC file.  This function tries to be as
 *  efficient as possible; how the metadata is actually written is shown by
 *  the following:
 *
 *  If the current chain is the same size as the existing metadata, the new
 *  data is written in place.
 *
 *  If the current chain is longer than the existing metadata, and
 *  \a use_padding is \c true, and the last block is a PADDING block of
 *  sufficient length, the function will truncate the final padding block
 *  so that the overall size of the metadata is the same as the existing
 *  metadata, and then just rewrite the metadata.  Otherwise, if not all of
 *  the above conditions are met, the entire FLAC file must be rewritten.
 *  If you want to use padding this way it is a good idea to call
 *  FLAC__metadata_chain_sort_padding() first so that you have the maximum
 *  amount of padding to work with, unless you need to preserve ordering
 *  of the PADDING blocks for some reason.
 *
 *  If the current chain is shorter than the existing metadata, and
 *  \a use_padding is \c true, and the final block is a PADDING block, the padding
 *  is extended to make the overall size the same as the existing data.  If
 *  \a use_padding is \c true and the last block is not a PADDING block, a new
 *  PADDING block is added to the end of the new data to make it the same
 *  size as the existing data (if possible, see the note to
 *  FLAC__metadata_simple_iterator_set_block() about the four byte limit)
 *  and the new data is written in place.  If none of the above apply or
 *  \a use_padding is \c false, the entire FLAC file is rewritten.
 *
 *  If \a preserve_file_stats is \c true, the owner and modification time will
 *  be preserved even if the FLAC file is written.
 *
 * \param chain               A pointer to an existing chain.
 * \param use_padding         See above.
 * \param preserve_file_stats See above.
 * \assert
 *    \code chain != NULL \endcode
 * \retval FLAC__bool
 *    \c true if the write succeeded, else \c false.  On failure,
 *    check the status with FLAC__metadata_chain_status().
 */
FLAC_API FLAC__bool FLAC__metadata_chain_write(FLAC__Metadata_Chain *chain, FLAC__bool use_padding, FLAC__bool preserve_file_stats);

/** Merge adjacent PADDING blocks into a single block.
 *
 * \note This function does not write to the FLAC file, it only
 * modifies the chain.
 *
 * \warning Any iterator on the current chain will become invalid after this
 * call.  You should delete the iterator and get a new one.
 *
 * \param chain               A pointer to an existing chain.
 * \assert
 *    \code chain != NULL \endcode
 */
FLAC_API void FLAC__metadata_chain_merge_padding(FLAC__Metadata_Chain *chain);

/** This function will move all PADDING blocks to the end on the metadata,
 *  then merge them into a single block.
 *
 * \note This function does not write to the FLAC file, it only
 * modifies the chain.
 *
 * \warning Any iterator on the current chain will become invalid after this
 * call.  You should delete the iterator and get a new one.
 *
 * \param chain  A pointer to an existing chain.
 * \assert
 *    \code chain != NULL \endcode
 */
FLAC_API void FLAC__metadata_chain_sort_padding(FLAC__Metadata_Chain *chain);


/*********** FLAC__Metadata_Iterator ***********/

/** Create a new iterator instance.
 *
 * \retval FLAC__Metadata_Iterator*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__Metadata_Iterator *FLAC__metadata_iterator_new();

/** Free an iterator instance.  Deletes the object pointed to by \a iterator.
 *
 * \param iterator  A pointer to an existing iterator.
 * \assert
 *    \code iterator != NULL \endcode
 */
FLAC_API void FLAC__metadata_iterator_delete(FLAC__Metadata_Iterator *iterator);

/** Initialize the iterator to point to the first metadata block in the
 *  given chain.
 *
 * \param iterator  A pointer to an existing iterator.
 * \param chain     A pointer to an existing and initialized (read) chain.
 * \assert
 *    \code iterator != NULL \endcode
 *    \code chain != NULL \endcode
 */
FLAC_API void FLAC__metadata_iterator_init(FLAC__Metadata_Iterator *iterator, FLAC__Metadata_Chain *chain);

/** Moves the iterator forward one metadata block, returning \c false if
 *  already at the end.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__bool
 *    \c false if already at the last metadata block of the chain, else
 *    \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_next(FLAC__Metadata_Iterator *iterator);

/** Moves the iterator backward one metadata block, returning \c false if
 *  already at the beginning.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__bool
 *    \c false if already at the first metadata block of the chain, else
 *    \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_prev(FLAC__Metadata_Iterator *iterator);

/** Get the type of the metadata block at the current position.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__MetadataType
 *    The type of the metadata block at the current iterator position.
 */
FLAC_API FLAC__MetadataType FLAC__metadata_iterator_get_block_type(const FLAC__Metadata_Iterator *iterator);

/** Get the metadata block at the current position.  You can modify
 *  the block in place but must write the chain before the changes
 *  are reflected to the FLAC file.  You do not need to call
 *  FLAC__metadata_iterator_set_block() to reflect the changes;
 *  the pointer returned by FLAC__metadata_iterator_get_block()
 *  points directly into the chain.
 *
 * \warning
 * Do not call FLAC__metadata_object_delete() on the returned object;
 * to delete a block use FLAC__metadata_iterator_delete_block().
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__StreamMetadata*
 *    The current metadata block.
 */
FLAC_API FLAC__StreamMetadata *FLAC__metadata_iterator_get_block(FLAC__Metadata_Iterator *iterator);

/** Set the metadata block at the current position, replacing the existing
 *  block.  The new block passed in becomes owned by the chain and it will be
 *  deleted when the chain is deleted.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \param block     A pointer to a metadata block.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 *    \code block != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the conditions in the above description are not met, or
 *    a memory allocation error occurs, otherwise \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_set_block(FLAC__Metadata_Iterator *iterator, FLAC__StreamMetadata *block);

/** Removes the current block from the chain.  If \a replace_with_padding is
 *  \c true, the block will instead be replaced with a padding block of equal
 *  size.  You can not delete the STREAMINFO block.  The iterator will be
 *  left pointing to the block before the one just "deleted", even if
 *  \a replace_with_padding is \c true.
 *
 * \param iterator              A pointer to an existing initialized iterator.
 * \param replace_with_padding  See above.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__bool
 *    \c false if the conditions in the above description are not met,
 *    otherwise \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_delete_block(FLAC__Metadata_Iterator *iterator, FLAC__bool replace_with_padding);

/** Insert a new block before the current block.  You cannot insert a block
 *  before the first STREAMINFO block.  You cannot insert a STREAMINFO block
 *  as there can be only one, the one that already exists at the head when you
 *  read in a chain.  The chain takes ownership of the new block and it will be
 *  deleted when the chain is deleted.  The iterator will be left pointing to
 *  the new block.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \param block     A pointer to a metadata block to insert.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__bool
 *    \c false if the conditions in the above description are not met, or
 *    a memory allocation error occurs, otherwise \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_insert_block_before(FLAC__Metadata_Iterator *iterator, FLAC__StreamMetadata *block);

/** Insert a new block after the current block.  You cannot insert a STREAMINFO
 *  block as there can be only one, the one that already exists at the head when
 *  you read in a chain.  The chain takes ownership of the new block and it will
 *  be deleted when the chain is deleted.  The iterator will be left pointing to
 *  the new block.
 *
 * \param iterator  A pointer to an existing initialized iterator.
 * \param block     A pointer to a metadata block to insert.
 * \assert
 *    \code iterator != NULL \endcode
 *    \a iterator has been successfully initialized with
 *    FLAC__metadata_iterator_init()
 * \retval FLAC__bool
 *    \c false if the conditions in the above description are not met, or
 *    a memory allocation error occurs, otherwise \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_iterator_insert_block_after(FLAC__Metadata_Iterator *iterator, FLAC__StreamMetadata *block);

/* \} */


/** \defgroup flac_metadata_object FLAC/metadata.h: metadata object methods
 *  \ingroup flac_metadata
 *
 * \brief
 * This module contains methods for manipulating FLAC metadata objects.
 *
 * Since many are variable length we have to be careful about the memory
 * management.  We decree that all pointers to data in the object are
 * owned by the object and memory-managed by the object.
 *
 * Use the FLAC__metadata_object_new() and FLAC__metadata_object_delete()
 * functions to create all instances.  When using the
 * FLAC__metadata_object_set_*() functions to set pointers to data, set
 * \a copy to \c true to have the function make it's own copy of the data, or
 * to \c false to give the object ownership of your data.  In the latter case
 * your pointer must be freeable by free() and will be free()d when the object
 * is FLAC__metadata_object_delete()d.  It is legal to pass a null pointer as
 * the data pointer to a FLAC__metadata_object_set_*() function as long as
 * the length argument is 0 and the \a copy argument is \c false.
 *
 * The FLAC__metadata_object_new() and FLAC__metadata_object_clone() function
 * will return \c NULL in the case of a memory allocation error, otherwise a new
 * object.  The FLAC__metadata_object_set_*() functions return \c false in the
 * case of a memory allocation error.
 *
 * We don't have the convenience of C++ here, so note that the library relies
 * on you to keep the types straight.  In other words, if you pass, for
 * example, a FLAC__StreamMetadata* that represents a STREAMINFO block to
 * FLAC__metadata_object_application_set_data(), you will get an assertion
 * failure.
 *
 * There is no need to recalculate the length field on metadata blocks you
 * have modified.  They will be calculated automatically before they  are
 * written back to a file.
 *
 * \{
 */


/** Create a new metadata object instance of the given type.
 *
 *  The object will be "empty"; i.e. values and data pointers will be \c 0,
 *  with the exception of FLAC__METADATA_TYPE_VORBIS_COMMENT, which will have
 *  the vendor string set (but zero comments).
 *
 *  Do not pass in a value greater than or equal to
 *  \a FLAC__METADATA_TYPE_UNDEFINED unless you really know what you're
 *  doing.
 *
 * \param type  Type of object to create
 * \retval FLAC__StreamMetadata*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__StreamMetadata *FLAC__metadata_object_new(FLAC__MetadataType type);

/** Create a copy of an existing metadata object.
 *
 *  The copy is a "deep" copy, i.e. dynamically allocated data within the
 *  object is also copied.  The caller takes ownership of the new block and
 *  is responsible for freeing it with FLAC__metadata_object_delete().
 *
 * \param object  Pointer to object to copy.
 * \assert
 *    \code object != NULL \endcode
 * \retval FLAC__StreamMetadata*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__StreamMetadata *FLAC__metadata_object_clone(const FLAC__StreamMetadata *object);

/** Free a metadata object.  Deletes the object pointed to by \a object.
 *
 *  The delete is a "deep" delete, i.e. dynamically allocated data within the
 *  object is also deleted.
 *
 * \param object  A pointer to an existing object.
 * \assert
 *    \code object != NULL \endcode
 */
FLAC_API void FLAC__metadata_object_delete(FLAC__StreamMetadata *object);

/** Compares two metadata objects.
 *
 *  The compare is "deep", i.e. dynamically allocated data within the
 *  object is also compared.
 *
 * \param block1  A pointer to an existing object.
 * \param block2  A pointer to an existing object.
 * \assert
 *    \code block1 != NULL \endcode
 *    \code block2 != NULL \endcode
 * \retval FLAC__bool
 *    \c true if objects are identical, else \c false.
 */
FLAC_API FLAC__bool FLAC__metadata_object_is_equal(const FLAC__StreamMetadata *block1, const FLAC__StreamMetadata *block2);

/** Sets the application data of an APPLICATION block.
 *
 *  If \a copy is \c true, a copy of the data is stored; otherwise, the object
 *  takes ownership of the pointer.  Returns \c false if \a copy == \c true
 *  and malloc fails.
 *
 * \param object  A pointer to an existing APPLICATION object.
 * \param data    A pointer to the data to set.
 * \param length  The length of \a data in bytes.
 * \param copy    See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_APPLICATION \endcode
 *    \code (data != NULL && length > 0) ||
 * (data == NULL && length == 0 && copy == false) \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_application_set_data(FLAC__StreamMetadata *object, FLAC__byte *data, unsigned length, FLAC__bool copy);

/** Resize the seekpoint array.
 *
 *  If the size shrinks, elements will truncated; if it grows, new placeholder
 *  points will be added to the end.
 *
 * \param object          A pointer to an existing SEEKTABLE object.
 * \param new_num_points  The desired length of the array; may be \c 0.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 *    \code (object->data.seek_table.points == NULL && object->data.seek_table.num_points == 0) ||
 * (object->data.seek_table.points != NULL && object->data.seek_table.num_points > 0) \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation error, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_resize_points(FLAC__StreamMetadata *object, unsigned new_num_points);

/** Set a seekpoint in a seektable.
 *
 * \param object     A pointer to an existing SEEKTABLE object.
 * \param point_num  Index into seekpoint array to set.
 * \param point      The point to set.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 *    \code object->data.seek_table.num_points > point_num \endcode
 */
FLAC_API void FLAC__metadata_object_seektable_set_point(FLAC__StreamMetadata *object, unsigned point_num, FLAC__StreamMetadata_SeekPoint point);

/** Insert a seekpoint into a seektable.
 *
 * \param object     A pointer to an existing SEEKTABLE object.
 * \param point_num  Index into seekpoint array to set.
 * \param point      The point to set.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 *    \code object->data.seek_table.num_points >= point_num \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation error, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_insert_point(FLAC__StreamMetadata *object, unsigned point_num, FLAC__StreamMetadata_SeekPoint point);

/** Delete a seekpoint from a seektable.
 *
 * \param object     A pointer to an existing SEEKTABLE object.
 * \param point_num  Index into seekpoint array to set.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 *    \code object->data.seek_table.num_points > point_num \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation error, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_delete_point(FLAC__StreamMetadata *object, unsigned point_num);

/** Check a seektable to see if it conforms to the FLAC specification.
 *  See the format specification for limits on the contents of the
 *  seektable.
 *
 * \param object  A pointer to an existing SEEKTABLE object.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if seek table is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_is_legal(const FLAC__StreamMetadata *object);

/** Append a number of placeholder points to the end of a seek table.
 *
 * \note
 * As with the other ..._seektable_template_... functions, you should
 * call FLAC__metadata_object_seektable_template_sort() when finished
 * to make the seek table legal.
 *
 * \param object  A pointer to an existing SEEKTABLE object.
 * \param num     The number of placeholder points to append.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_template_append_placeholders(FLAC__StreamMetadata *object, unsigned num);

/** Append a specific seek point template to the end of a seek table.
 *
 * \note
 * As with the other ..._seektable_template_... functions, you should
 * call FLAC__metadata_object_seektable_template_sort() when finished
 * to make the seek table legal.
 *
 * \param object  A pointer to an existing SEEKTABLE object.
 * \param sample_number  The sample number of the seek point template.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_template_append_point(FLAC__StreamMetadata *object, FLAC__uint64 sample_number);

/** Append specific seek point templates to the end of a seek table.
 *
 * \note
 * As with the other ..._seektable_template_... functions, you should
 * call FLAC__metadata_object_seektable_template_sort() when finished
 * to make the seek table legal.
 *
 * \param object  A pointer to an existing SEEKTABLE object.
 * \param sample_numbers  An array of sample numbers for the seek points.
 * \param num     The number of seek point templates to append.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_template_append_points(FLAC__StreamMetadata *object, FLAC__uint64 sample_numbers[], unsigned num);

/** Append a set of evenly-spaced seek point templates to the end of a
 *  seek table.
 *
 * \note
 * As with the other ..._seektable_template_... functions, you should
 * call FLAC__metadata_object_seektable_template_sort() when finished
 * to make the seek table legal.
 *
 * \param object  A pointer to an existing SEEKTABLE object.
 * \param num     The number of placeholder points to append.
 * \param total_samples  The total number of samples to be encoded;
 *                       the seekpoints will be spaced approximately
 *                       \a total_samples / \a num samples apart.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_template_append_spaced_points(FLAC__StreamMetadata *object, unsigned num, FLAC__uint64 total_samples);

/** Sort a seek table's seek points according to the format specification,
 *  removing duplicates.
 *
 * \param object   A pointer to a seek table to be sorted.
 * \param compact  If \c false, behaves like FLAC__format_seektable_sort().
 *                 If \c true, duplicates are deleted and the seek table is
 *                 shrunk appropriately; the number of placeholder points
 *                 present in the seek table will be the same after the call
 *                 as before.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_SEEKTABLE \endcode
 * \retval FLAC__bool
 *    \c false if realloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_seektable_template_sort(FLAC__StreamMetadata *object, FLAC__bool compact);

/** Sets the vendor string in a VORBIS_COMMENT block.
 *
 *  If \a copy is \c true, a copy of the entry is stored; otherwise, the object
 *  takes ownership of the \c entry->entry pointer.  Returns \c false if
 *  \a copy == \c true and malloc fails.
 *
 * \param object  A pointer to an existing VORBIS_COMMENT object.
 * \param entry   The entry to set the vendor string to.
 * \param copy    See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 *    \code (entry->entry != NULL && entry->length > 0) ||
 * (entry->entry == NULL && entry->length == 0 && copy == false) \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_set_vendor_string(FLAC__StreamMetadata *object, FLAC__StreamMetadata_VorbisComment_Entry entry, FLAC__bool copy);

/** Resize the comment array.
 *
 *  If the size shrinks, elements will truncated; if it grows, new empty
 *  fields will be added to the end.
 *
 * \param object            A pointer to an existing VORBIS_COMMENT object.
 * \param new_num_comments  The desired length of the array; may be \c 0.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 *    \code (object->data.vorbis_comment.comments == NULL && object->data.vorbis_comment.num_comments == 0) ||
 * (object->data.vorbis_comment.comments != NULL && object->data.vorbis_comment.num_comments > 0) \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation error, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_resize_comments(FLAC__StreamMetadata *object, unsigned new_num_comments);

/*@@@@ code and docs need to assert on track_num */
/** Sets a comment in a VORBIS_COMMENT block.
 *
 *  If \a copy is \c true, a copy of the entry is stored; otherwise, the object
 *  takes ownership of the \c entry->entry pointer.  Returns \c false if
 *  \a copy == \c true and malloc fails.
 *
 * \param object       A pointer to an existing VORBIS_COMMENT object.
 * \param comment_num  Index into comment array to set.
 * \param entry        The entry to set the comment to.
 * \param copy         See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 *    \code (entry->entry != NULL && entry->length > 0) ||
 * (entry->entry == NULL && entry->length == 0 && copy == false) \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_set_comment(FLAC__StreamMetadata *object, unsigned comment_num, FLAC__StreamMetadata_VorbisComment_Entry entry, FLAC__bool copy);

/** Insert a comment in a VORBIS_COMMENT block at the given index.
 *
 *  If \a copy is \c true, a copy of the entry is stored; otherwise, the object
 *  takes ownership of the \c entry->entry pointer.  Returns \c false if
 *  \a copy == \c true and malloc fails.
 *
 * \param object       A pointer to an existing VORBIS_COMMENT object.
 * \param comment_num  The index at which to insert the comment.  The comments
 *                     at and after \a comment_num move right one position.
 *                     To append a comment to the end, set \a comment_num to
 *                     \c object->data.vorbis_comment.num_comments .
 * \param entry        The comment to insert.
 * \param copy         See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 *    \code object->data.vorbis_comment.num_comments >= comment_num \endcode
 *    \code (entry->entry != NULL && entry->length > 0) ||
 * (entry->entry == NULL && entry->length == 0 && copy == false) \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_insert_comment(FLAC__StreamMetadata *object, unsigned comment_num, FLAC__StreamMetadata_VorbisComment_Entry entry, FLAC__bool copy);

/** Delete a comment in a VORBIS_COMMENT block at the given index.
 *
 * \param object       A pointer to an existing VORBIS_COMMENT object.
 * \param comment_num  The index of the comment to delete.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 *    \code object->data.vorbis_comment.num_comments > comment_num \endcode
 *    \code (entry->entry != NULL && entry->length > 0) ||
 * (entry->entry == NULL && entry->length == 0 && copy == false) \endcode
 * \retval FLAC__bool
 *    \c false if realloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_delete_comment(FLAC__StreamMetadata *object, unsigned comment_num);

/*@@@@ add to unit tests */
/** Check if the given Vorbis comment entry's field name matches the given
 *  field name.
 *
 * \param entry              A pointer to an existing Vorbis comment entry.
 * \param field_name         The field name to check.
 * \param field_name_length  The length of \a field_name, not including the
 *                           terminating \c NULL.
 * \assert
 *    \code entry != NULL \endcode
 *    \code (entry->entry != NULL && entry->length > 0)
 * \retval FLAC__bool
 *    \c true if the field names match, else \c false
 */
FLAC_API FLAC__bool FLAC__metadata_object_vorbiscomment_entry_matches(const FLAC__StreamMetadata_VorbisComment_Entry *entry, const char *field_name, unsigned field_name_length);

/*@@@@ add to unit tests */
/** Find a Vorbis comment with the given field name.
 *
 *  The search begins at entry number \a offset; use and offset of 0 to
 *  search from the beginning of the comment array.
 *
 * \param object      A pointer to an existing VORBIS_COMMENT object.
 * \param offset      The offset into the comment array from where to start
 *                    the search.
 * \param field_name  The field name of the comment to find.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 * \retval int
 *    The offset in the comment array of the first comment whose field
 *    name matches \a field_name, or \c -1 if no match was found.
 */
FLAC_API int FLAC__metadata_object_vorbiscomment_find_entry_from(const FLAC__StreamMetadata *object, unsigned offset, const char *field_name);

/*@@@@ add to unit tests */
/** Remove first Vorbis comment matching the given field name.
 *
 * \param object      A pointer to an existing VORBIS_COMMENT object.
 * \param field_name  The field name of comment to delete.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 * \retval int
 *    \c -1 for memory allocation error, \c 0 for no matching entries,
 *    \c 1 for one matching entry deleted.
 */
FLAC_API int FLAC__metadata_object_vorbiscomment_remove_entry_matching(FLAC__StreamMetadata *object, const char *field_name);

/*@@@@ add to unit tests */
/** Remove all Vorbis comments matching the given field name.
 *
 * \param object      A pointer to an existing VORBIS_COMMENT object.
 * \param field_name  The field name of comments to delete.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT \endcode
 * \retval int
 *    \c -1 for memory allocation error, \c 0 for no matching entries,
 *    else the number of matching entries deleted.
 */
FLAC_API int FLAC__metadata_object_vorbiscomment_remove_entries_matching(FLAC__StreamMetadata *object, const char *field_name);

/*@@@@ document */
FLAC_API FLAC__StreamMetadata_CueSheet_Track *FLAC__metadata_object_cuesheet_track_new();

/*@@@@ document */
FLAC_API FLAC__StreamMetadata_CueSheet_Track *FLAC__metadata_object_cuesheet_track_clone(const FLAC__StreamMetadata_CueSheet_Track *object);

/*@@@@ document */
FLAC_API void FLAC__metadata_object_cuesheet_track_delete(FLAC__StreamMetadata_CueSheet_Track *object);

/*@@@@ document */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_track_resize_indices(FLAC__StreamMetadata *object, unsigned track_num, unsigned new_num_indices);

/** Insert an index point in a CUESHEET track at the given index.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    The index of the track to modify.  NOTE: this is not
 *                     necessarily the same as the track's \a number field.
 * \param index_num    The index into the track's index array at which to
 *                     insert the index point.  NOTE: this is not necessarily
 *                     the same as the index point's \a number field.  The
 *                     indices at and after \a index_num move right one
 *                     position.  To append an index point to the end, set
 *                     \a index_num to
 *                     \c object->data.cue_sheet.tracks[track_num].num_indices .
 * \param index        The index point to insert.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code object->data.cue_sheet.num_tracks >= track_num \endcode
 *    \code object->data.cue_sheet.num_tracks > track_num \endcode
 *    \code object->data.cue_sheet.tracks[track_num].num_indices >= index_num \endcode
 * \retval FLAC__bool
 *    \c false if realloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_track_insert_index(FLAC__StreamMetadata *object, unsigned track_num, unsigned index_num, FLAC__StreamMetadata_CueSheet_Index index);

/*@@@@ document, add to unit tests */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_track_insert_blank_index(FLAC__StreamMetadata *object, unsigned track_num, unsigned index_num);

/** Delete an index point in a CUESHEET track at the given index.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    The index into the track array of the track to
 *                     modify.  NOTE: this is not necessarily the same
 *                     as the track's \a number field.
 * \param index_num    The index into the track's index array of the index
 *                     to delete.  NOTE: this is not necessarily the same
 *                     as the index's \a number field.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code object->data.cue_sheet.num_tracks > track_num \endcode
 *    \code object->data.cue_sheet.tracks[track_num].num_indices > index_num \endcode
 * \retval FLAC__bool
 *    \c false if realloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_track_delete_index(FLAC__StreamMetadata *object, unsigned track_num, unsigned index_num);

/** Resize the track array.
 *
 *  If the size shrinks, elements will truncated; if it grows, new blank
 *  tracks will be added to the end.
 *
 * \param object            A pointer to an existing CUESHEET object.
 * \param new_num_tracks    The desired length of the array; may be \c 0.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code (object->data.cue_sheet.tracks == NULL && object->data.cue_sheet.num_tracks == 0) ||
 * (object->data.cue_sheet.tracks != NULL && object->data.cue_sheet.num_tracks > 0) \endcode
 * \retval FLAC__bool
 *    \c false if memory allocation error, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_resize_tracks(FLAC__StreamMetadata *object, unsigned new_num_tracks);

/*@@@@ code and docs need to assert on track_num */
/** Sets a track in a CUESHEET block.
 *
 *  If \a copy is \c true, a copy of the track is stored; otherwise, the object
 *  takes ownership of the \a track pointer.  Returns \c false if
 *  \a copy == \c true and malloc fails.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    Index into track array to set.  NOTE: this is not
 *                     necessarily the same as the track's \a number field.
 * \param track        The track to set the track to.  You may safely pass in
 *                     a const pointer if \a copy is \c true.
 * \param copy         See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_set_track(FLAC__StreamMetadata *object, unsigned track_num, FLAC__StreamMetadata_CueSheet_Track *track, FLAC__bool copy);

/** Insert a track in a CUESHEET block at the given index.
 *
 *  If \a copy is \c true, a copy of the track is stored; otherwise, the object
 *  takes ownership of the \a track pointer.  Returns \c false if
 *  \a copy == \c true and malloc fails.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    The index at which to insert the track.  NOTE: this
 *                     is not necessarily the same as the track's \a number
 *                     field.  The tracks at and after \a track_num move right
 *                     one position.  To append a track to the end, set
 *                     \a track_num to \c object->data.cue_sheet.num_tracks .
 * \param track        The track to insert.  You may safely pass in a const
 *                     pointer if \a copy is \c true.
 * \param copy         See above.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code object->data.cue_sheet.num_tracks >= track_num \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_insert_track(FLAC__StreamMetadata *object, unsigned track_num, FLAC__StreamMetadata_CueSheet_Track *track, FLAC__bool copy);

/*@@@@ add to unit tests */
/** Insert a blank track in a CUESHEET block at the given index.
 *
 *  A blank track is one in which all field values are zero.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    The index at which to insert the track.  NOTE: this
 *                     is not necessarily the same as the track's \a number
 *                     field.  The tracks at and after \a track_num move right
 *                     one position.  To append a track to the end, set
 *                     \a track_num to \c object->data.cue_sheet.num_tracks .
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code object->data.cue_sheet.num_tracks >= track_num \endcode
 * \retval FLAC__bool
 *    \c false if \a copy is \c true and malloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_insert_blank_track(FLAC__StreamMetadata *object, unsigned track_num);

/** Delete a track in a CUESHEET block at the given index.
 *
 * \param object       A pointer to an existing CUESHEET object.
 * \param track_num    The index into the track array of the track to
 *                     delete.  NOTE: this is not necessarily the same
 *                     as the track's \a number field.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 *    \code object->data.cue_sheet.num_tracks > track_num \endcode
 * \retval FLAC__bool
 *    \c false if realloc fails, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_delete_track(FLAC__StreamMetadata *object, unsigned track_num);

/** Check a cue sheet to see if it conforms to the FLAC specification.
 *  See the format specification for limits on the contents of the
 *  cue sheet.
 *
 * \param object     A pointer to an existing CUESHEET object.
 * \param check_cd_da_subset  If \c true, check CUESHEET against more
 *                   stringent requirements for a CD-DA (audio) disc.
 * \param violation  Address of a pointer to a string.  If there is a
 *                   violation, a pointer to a string explanation of the
 *                   violation will be returned here. \a violation may be
 *                   \c NULL if you don't need the returned string.  Do not
 *                   free the returned string; it will always point to static
 *                   data.
 * \assert
 *    \code object != NULL \endcode
 *    \code object->type == FLAC__METADATA_TYPE_CUESHEET \endcode
 * \retval FLAC__bool
 *    \c false if cue sheet is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__metadata_object_cuesheet_is_legal(const FLAC__StreamMetadata *object, FLAC__bool check_cd_da_subset, const char **violation);

/* \} */

#ifdef __cplusplus
}
#endif

#endif
