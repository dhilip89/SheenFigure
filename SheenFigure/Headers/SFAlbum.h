/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SF_ALBUM_H
#define _SF_ALBUM_H

#include "SFBase.h"

/**
 * The type used to represent a glyph set.
 */
typedef struct _SFAlbum *SFAlbumRef;

SFAlbumRef SFAlbumCreate(void);

/**
 * Provides the number of processed code units.
 * @param album
 *      The album whose character count you want to obtain.
 * @return
 *      The number of characters analyzed by the shaping process.
 */
SFUInteger SFAlbumGetCodeunitCount(SFAlbumRef album);

/**
 * Provides the number of produced glyphs.
 * @param album
 *      The album whose number of glyphs you want to obtain.
 * @return
 *      The number of glyphs kept by the album.
 */
SFUInteger SFAlbumGetGlyphCount(SFAlbumRef album);

/**
 * Provides an array of glyphs corresponding to the input text.
 * @param album
 *      The album whose glyphs you want to obtain.
 * @return
 *      An array of glyphs produced as part of shaping process.
 */
const SFGlyphID *SFAlbumGetGlyphIDsPtr(SFAlbumRef album);

/**
 * Provides an array of glyph offsets in font units where each glyph is positioned with respect to
 * zero origin.
 * @param album
 *      The album whose glyph positions you want to obtain.
 * @return
 *      An array of glyph positions produced as part of shaping process.
 */
const SFPoint *SFAlbumGetGlyphOffsetsPtr(SFAlbumRef album);

/**
 * Provides an array of glyph advances in font units.
 * @param album
 *      The album whose glyph advances you want to obtain.
 * @return
 *      An array of glyph advances produced as part of shaping process.
 */
const SFAdvance *SFAlbumGetGlyphAdvancesPtr(SFAlbumRef album);

/**
 * Provides an array, mapping each code unit in the input string to corresponding glyph index.
 * @param album
 *      The album whose charater to glyph map you want to obtain.
 * @return
 *      An array of indexes mapping characters to glyphs.
 */
const SFUInteger *SFAlbumGetCodeunitToGlyphMapPtr(SFAlbumRef album);

SFAlbumRef SFAlbumRetain(SFAlbumRef album);
void SFAlbumRelease(SFAlbumRef album);

#endif
