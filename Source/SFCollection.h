/*
 * Copyright (C) 2015 Muhammad Tayyab Akram
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

#ifndef SF_COLLECTION_INTERNAL_H
#define SF_COLLECTION_INTERNAL_H

#include <SFConfig.h>
#include <SFTypes.h>

#include "SFList.h"

struct _SFGlyphDetail;
struct _SFCollection;

typedef struct _SFGlyphDetail SFGlyphDetail;
typedef struct _SFCollection SFCollection;

typedef SFGlyphDetail *SFGlyphDetailRef;
typedef SFCollection *SFCollectionRef;

enum {
    SFGlyphTraitNone = 0,
    SFGlyphTraitBase = 1,
    SFGlyphTraitLigature = 2,
    SFGlyphTraitMark = 3,
    SFGlyphTraitComponent = 4,
    SFGlyphTraitMaster = 8,
    /**
     * Indicates that the glyph has been removed during substitution process;
     * possibly due to ligature substitution.
     */
    SFGlyphTraitRemoved = 16,
    /**
     * Indicates that the glyph is to be treated as right-to-left for cursive
     * attachments.
     */
    SFGlyphTraitRightToLeft = 32
};
typedef SFUInt32 SFGlyphTrait;

struct _SFGlyphDetail {
    SFUInteger association;    /**< Index of the code point to which the glyph maps. */
    SFGlyphTrait traits;       /**< Traits of the glyph. */
    /**
     * Offset to the next right-to-left cursively connected element.
     */
    SFUInt16 offset;
};

struct _SFCollection {
    const SFCodePoint *codePointArray; /**< The array of codepoints which are to be shaped. */
    SFUInteger *mapArray;
    SFUInteger codePointCount;
    SFUInteger elementCount;

    SF_LIST(SFGlyphID) _glyphs;
    SF_LIST(SFGlyphDetail) _details;
    SF_LIST(SFPoint) _positions;
    SF_LIST(SFInteger) _advances;
};

/**
 * Initializes the collection for given code points.
 */
SF_INTERNAL void SFCollectionInitialize(SFCollectionRef collection, SFCodePoint *codePointArray, SFUInteger codePointCount);

SF_INTERNAL void SFCollectionAllocateGlyphs(SFCollectionRef collection);
SF_INTERNAL void SFCollectionAllocatePositions(SFCollectionRef collection);

SF_INTERNAL void SFCollectionAddGlyph(SFCollectionRef collection, SFGlyphID glyph, SFUInteger association);

/**
 * Reserves specified number of glyphs at the given index.
 * @note
 *      The reserved glyphs will be uninitialized.
 */
SF_INTERNAL void SFCollectionReserveGlyphs(SFCollectionRef collection, SFUInteger index, SFUInteger count);

/**
 * Allocates an array for charater to glyph map.
 * @note
 *      The allocated map will be uninitialized.
 */
SF_INTERNAL void SFCollectionAllocateMap(SFCollectionRef collection);

SF_INTERNAL SFGlyphID SFCollectionGetGlyph(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetGlyph(SFCollectionRef collection, SFIndex index, SFGlyphID glyph);

SF_INTERNAL SFGlyphTrait SFCollectionGetTraits(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetTraits(SFCollectionRef collection, SFIndex index, SFGlyphTrait trait);

SF_INTERNAL SFIndex SFCollectionGetAssociation(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetAssociation(SFCollectionRef collection, SFIndex index, SFIndex association);

SF_INTERNAL SFPoint SFCollectionGetPosition(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetPosition(SFCollectionRef collection, SFIndex index, SFPoint position);

SF_INTERNAL SFInteger SFCollectionGetAdvance(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetAdvance(SFCollectionRef collection, SFIndex index, SFInteger advance);

SF_INTERNAL SFUInt16 SFCollectionGetOffset(SFCollectionRef collection, SFIndex index);
SF_INTERNAL void SFCollectionSetOffset(SFCollectionRef collection, SFIndex index, SFUInt16 offset);

/**
 * Finalizes the collection.
 */
SF_INTERNAL void SFCollectionFinalize(SFCollectionRef collection);

#endif
