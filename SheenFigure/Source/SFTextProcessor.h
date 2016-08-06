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

#ifndef _SF_TEXT_PROCESSOR_INTERNAL_H
#define _SF_TEXT_PROCESSOR_INTERNAL_H

#include <SFConfig.h>

#include "SFAlbum.h"
#include "SFArtist.h"
#include "SFBase.h"
#include "SFFont.h"
#include "SFLocator.h"
#include "SFPattern.h"

typedef struct _SFTextProcessor {
    SFPatternRef _pattern;
    SFAlbumRef _album;
    SFData _glyphClassDef;
    SFTextDirection _textDirection;
    SFTextMode _textMode;
    SFLocator _locator;
} SFTextProcessor, *SFTextProcessorRef;

SF_INTERNAL void SFTextProcessorInitialize(SFTextProcessorRef textProcessor, SFPatternRef pattern, SFAlbumRef album, SFTextDirection textDirection, SFTextMode textMode);

SF_INTERNAL void SFTextProcessorDiscoverGlyphs(SFTextProcessorRef textProcessor);
SF_INTERNAL void SFTextProcessorSubstituteGlyphs(SFTextProcessorRef textProcessor);
SF_INTERNAL void SFTextProcessorPositionGlyphs(SFTextProcessorRef textProcessor);
SF_INTERNAL void SFTextProcessorWrapUp(SFTextProcessorRef textProcessor);

SF_PRIVATE void _SFApplyLookup(SFTextProcessorRef processor, SFFeatureKind featureKind, SFUInt16 lookupIndex);

#endif
