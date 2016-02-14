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

#include <SFConfig.h>
#include <SFTypes.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "SFAssert.h"
#include "SFAlbum.h"

static const SFGlyphMask _SFGlyphMaskEmpty = { { SFUInt16Max, 0 } };

static void SFAlbumSetGlyphMask(SFAlbumRef album, SFUInteger index, SFGlyphMask glyphMask);

SF_PRIVATE SFUInt16 _SFAlbumGetAntiFeatureMask(SFUInt16 featureMask)
{
    /* The assumtion must NOT break that the feature mask will never be equal to default mask. */
    SFAssert(featureMask != _SFGlyphMaskEmpty.section.featureMask);

    return !featureMask ? ~_SFGlyphMaskEmpty.section.featureMask : ~featureMask;
}

SFAlbumRef SFAlbumCreate(void)
{
    SFAlbumRef album = malloc(sizeof(SFAlbum));
    SFAlbumInitialize(album);

    return album;
}

SFUInteger SFAlbumGetGlyphCount(SFAlbumRef album)
{
    return album->glyphCount;
}

SFGlyphID *SFAlbumGetGlyphIDs(SFAlbumRef album)
{
    return album->_glyphs.items;
}

SFPoint *SFAlbumGetGlyphPositions(SFAlbumRef album)
{
    return album->_positions.items;
}

SFInteger *SFAlbumGetGlyphAdvances(SFAlbumRef album)
{
    return album->_advances.items;
}

SFUInteger *SFAlbumGetCharacterToGlyphMap(SFAlbumRef album)
{
    return album->mapArray;
}

SFAlbumRef SFAlbumRetain(SFAlbumRef album)
{
    if (album) {
        album->_retainCount++;
    }

    return album;
}

void SFAlbumRelease(SFAlbumRef album)
{
    if (album && --album->_retainCount == 0) {
        SFAlbumFinalize(album);
    }
}

SF_INTERNAL void SFAlbumInitialize(SFAlbumRef album)
{
    album->codePointArray = NULL;
    album->mapArray = NULL;
    album->codePointCount = 0;
    album->glyphCount = 0;

    SFListInitialize(&album->_indexes, sizeof(SFUInteger));
    SFListInitialize(&album->_glyphs, sizeof(SFGlyphID));
    SFListInitialize(&album->_details, sizeof(SFGlyphDetail));
    SFListInitialize(&album->_positions, sizeof(SFPoint));
    SFListInitialize(&album->_advances, sizeof(SFInteger));

    album->_version = 0;
    album->_state = _SFAlbumStateEmpty;
    album->_retainCount = 1;
}

SF_INTERNAL void SFAlbumReset(SFAlbumRef album, SFCodepoint *codePointArray, SFUInteger codePointCount)
{
	/* There must be some code points. */
	SFAssert(codePointArray != NULL && codePointCount > 0);

    /*
     * TODO: memory management of map.
     */

    album->codePointArray = codePointArray;
	album->mapArray = NULL;
    album->codePointCount = codePointCount;
    album->glyphCount = 0;

    SFListClear(&album->_indexes);
    SFListClear(&album->_glyphs);
    SFListClear(&album->_details);
    SFListClear(&album->_positions);
    SFListClear(&album->_advances);

    album->_version = 0;
    album->_state = _SFAlbumStateEmpty;
}

SF_INTERNAL void SFAlbumStartFilling(SFAlbumRef album)
{
    SFUInteger indexesCapacity = album->codePointCount >> 1;
	SFUInteger glyphCapacity = album->codePointCount << 1;

    SFListReserveRange(&album->_indexes, 0, indexesCapacity);
    SFListReserveRange(&album->_glyphs, 0, glyphCapacity);
    SFListReserveRange(&album->_details, 0, glyphCapacity);

	album->_state = _SFAlbumStateFilling;
}

SF_INTERNAL void SFAlbumAddGlyph(SFAlbumRef album, SFGlyphID glyph, SFUInteger association)
{
    SFUInteger index;

    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    album->_version++;
    index = album->glyphCount++;

    /* Initialize glyph along with its details. */
    SFAlbumSetGlyph(album, index, glyph);
    SFAlbumSetGlyphMask(album, index, _SFGlyphMaskEmpty);
    SFAlbumSetSingleAssociation(album, index, association);
}

SF_INTERNAL void SFAlbumReserveGlyphs(SFAlbumRef album, SFUInteger index, SFUInteger count)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    album->_version++;
    album->glyphCount += count;

    SFListReserveRange(&album->_glyphs, index, count);
    SFListReserveRange(&album->_details, index, count);
}

SF_INTERNAL SFGlyphID SFAlbumGetGlyph(SFAlbumRef album, SFUInteger index)
{
    return SFListGetVal(&album->_glyphs, index);
}

SF_INTERNAL void SFAlbumSetGlyph(SFAlbumRef album, SFUInteger index, SFGlyphID glyph)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    SFListSetVal(&album->_glyphs, index, glyph);
}

SF_INTERNAL SFUInteger SFAlbumGetSingleAssociation(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_details, index)->association;
}

SF_INTERNAL void SFAlbumSetSingleAssociation(SFAlbumRef album, SFUInteger index, SFUInteger association)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);
    /* The glyph must not be composite. */
    SFAssert(!(SFAlbumGetTraits(album, index) & SFGlyphTraitComposite));

    SFListGetRef(&album->_details, index)->association = association;
}

SF_INTERNAL SFUInteger *SFAlbumGetCompositeAssociations(SFAlbumRef album, SFUInteger index, SFUInteger *outCount)
{
    SFUInteger association;
    SFUInteger *array;

    /* The glyph must be composite. */
    SFAssert(SFAlbumGetTraits(album, index) & SFGlyphTraitComposite);

    association = SFListGetRef(&album->_details, index)->association;
    array = SFListGetRef(&album->_indexes, association);
    *outCount = array[0];

    return &array[1];
}

SF_INTERNAL SFUInteger *SFAlbumMakeCompositeAssociations(SFAlbumRef album, SFUInteger index, SFUInteger count)
{
    SFUInteger association;
    SFUInteger reference;
    SFUInteger *array;

    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);
    /* The glyph must be composite. */
    SFAssert(SFAlbumGetTraits(album, index) & SFGlyphTraitComposite);

    association = album->_indexes.count;
    SFListAdd(&album->_indexes, count);
    SFListGetRef(&album->_details, index)->association = association;

    reference = album->_indexes.count;
    SFListReserveRange(&album->_indexes, reference, count);
    array = SFListGetRef(&album->_indexes, reference);

    return array;
}

SF_PRIVATE SFGlyphMask _SFAlbumGetGlyphMask(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_details, index)->mask;
}

static void SFAlbumSetGlyphMask(SFAlbumRef album, SFUInteger index, SFGlyphMask glyphMask)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    SFListGetRef(&album->_details, index)->mask = glyphMask;
}

SF_INTERNAL SFUInt16 SFAlbumGetFeatureMask(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_details, index)->mask.section.featureMask;
}

SF_INTERNAL void SFAlbumSetFeatureMask(SFAlbumRef album, SFUInteger index, SFUInt16 featureMask)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    SFListGetRef(&album->_details, index)->mask.section.featureMask = featureMask;
}

SF_INTERNAL SFGlyphTraits SFAlbumGetTraits(SFAlbumRef album, SFUInteger index)
{
    return (SFGlyphTraits)SFListGetRef(&album->_details, index)->mask.section.glyphTraits;
}

SF_INTERNAL void SFAlbumSetTraits(SFAlbumRef album, SFUInteger index, SFGlyphTraits traits)
{
    /* The album must be either in filling state or arranging state. */
    SFAssert(album->_state == _SFAlbumStateFilling || album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_details, index)->mask.section.glyphTraits = (SFUInt16)traits;
}

SF_INTERNAL void SFAlbumInsertTraits(SFAlbumRef album, SFUInteger index, SFGlyphTraits traits)
{
    /* The album must be either in filling state or arranging state. */
    SFAssert(album->_state == _SFAlbumStateFilling || album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_details, index)->mask.section.glyphTraits |= (SFUInt16)traits;
}

SF_INTERNAL void SFAlbumRemoveTraits(SFAlbumRef album, SFUInteger index, SFGlyphTraits traits)
{
    /* The album must be either in filling state or arranging state. */
    SFAssert(album->_state == _SFAlbumStateFilling || album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_details, index)->mask.section.glyphTraits &= (SFUInt16)~traits;
}

SF_INTERNAL void SFAlbumStopFilling(SFAlbumRef album)
{
    /* The album must be in filling state. */
    SFAssert(album->_state == _SFAlbumStateFilling);

    album->_state = _SFAlbumStateFilled;
}

SF_INTERNAL void SFAlbumStartArranging(SFAlbumRef album)
{
    /* The album must be filled before arranging it. */
    SFAssert(album->_state == _SFAlbumStateFilled);

    SFListReserveRange(&album->_positions, 0, album->glyphCount);
    SFListReserveRange(&album->_advances, 0, album->glyphCount);

    album->_state = _SFAlbumStateArranging;
}

SF_INTERNAL SFInteger SFAlbumGetX(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_positions, index)->x;
}

SF_INTERNAL void SFAlbumSetX(SFAlbumRef album, SFUInteger index, SFInteger x)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_positions, index)->x = x;
}

SF_INTERNAL SFInteger SFAlbumGetY(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_positions, index)->y;
}

SF_INTERNAL void SFAlbumSetY(SFAlbumRef album, SFUInteger index, SFInteger y)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_positions, index)->y = y;
}

SF_INTERNAL SFInteger SFAlbumGetAdvance(SFAlbumRef album, SFUInteger index)
{
    return SFListGetVal(&album->_advances, index);
}

SF_INTERNAL void SFAlbumSetAdvance(SFAlbumRef album, SFUInteger index, SFInteger advance)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    SFListSetVal(&album->_advances, index, advance);
}

SF_INTERNAL SFUInt16 SFAlbumGetCursiveOffset(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_details, index)->cursiveOffset;
}

SF_INTERNAL void SFAlbumSetCursiveOffset(SFAlbumRef album, SFUInteger index, SFUInt16 offset)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_details, index)->cursiveOffset = offset;
}

SF_INTERNAL SFUInt16 SFAlbumGetAttachmentOffset(SFAlbumRef album, SFUInteger index)
{
    return SFListGetRef(&album->_details, index)->attachmentOffset;
}

SF_INTERNAL void SFAlbumSetAttachmentOffset(SFAlbumRef album, SFUInteger index, SFUInt16 offset)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    SFListGetRef(&album->_details, index)->attachmentOffset = offset;
}

SF_INTERNAL void SFAlbumStopArranging(SFAlbumRef album)
{
    /* The album must be in arranging state. */
    SFAssert(album->_state == _SFAlbumStateArranging);

    album->_state = _SFAlbumStateArranged;
}

SF_INTERNAL void SFAlbumRemoveGlyphs(SFAlbumRef album, SFUInteger index, SFUInteger count)
{
    SFListRemoveRange(&album->_glyphs, index, count);
    SFListRemoveRange(&album->_details, index, count);
    SFListRemoveRange(&album->_positions, index, count);
    SFListRemoveRange(&album->_advances, index, count);
}

SF_INTERNAL void SFAlbumRemovePlaceholders(SFAlbumRef album)
{
    SFUInteger placeholderCount = 0;
    SFUInteger index = album->glyphCount;

    while (index--) {
        SFGlyphTraits traits = SFAlbumGetTraits(album, index);
        if (traits & SFGlyphTraitPlaceholder) {
            placeholderCount++;
        } else {
            if (placeholderCount) {
                SFAlbumRemoveGlyphs(album, index + 1, placeholderCount);
                album->glyphCount -= placeholderCount;
                placeholderCount = 0;
            }
        }
    }

    if (placeholderCount) {
        SFAlbumRemoveGlyphs(album, 0, placeholderCount);
        album->glyphCount -= placeholderCount;
    }
}

SF_INTERNAL void SFAlbumBuildCharToGlyphMap(SFAlbumRef album)
{
    SFUInteger index = album->glyphCount;
    SFUInteger *map;

    map = malloc(sizeof(SFUInteger) * album->codePointCount);

    /* Traverse in reverse order so that first glyph takes priority in case of multiple substitution. */
    while (index--) {
        SFGlyphTraits traits = SFAlbumGetTraits(album, index);
        SFUInteger association;

        if (traits & SFGlyphTraitComposite) {
            SFUInteger count;
            SFUInteger *array;
            SFUInteger j;

            array = SFAlbumGetCompositeAssociations(album, index, &count);

            for (j = 0; j < count; j++) {
                association = array[j];
                /* The association MUST be valid. */
                SFAssert(association < album->codePointCount);

                map[association] = index;
            }
        } else {
            association = SFAlbumGetSingleAssociation(album, index);
            /* The association MUST be valid. */
            SFAssert(association < album->codePointCount);

            map[association] = index;
        }
    }

    album->mapArray = map;
}

SF_INTERNAL void SFAlbumFinalize(SFAlbumRef album) {
    SFListFinalize(&album->_indexes);
    SFListFinalize(&album->_glyphs);
    SFListFinalize(&album->_details);
    SFListFinalize(&album->_positions);
    SFListFinalize(&album->_advances);
}
