/*
* Copyright (C) 2018 Muhammad Tayyab Akram
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
#include <stddef.h>

#include "SFAlbum.h"
#include "SFArtist.h"
#include "SFBase.h"
#include "SFShapingEngine.h"
#include "SFTextProcessor.h"
#include "SFSimpleEngine.h"

static SFScriptKnowledgeRef _SFSimpleKnowledgeSeekScript(const void *object, SFTag scriptTag);
static void _SFSimpleEngineProcessAlbum(const void *object, SFAlbumRef album);

static SFScriptKnowledge _SFSimpleScriptKnowledge = {
    SFTextDirectionLeftToRight,
    { NULL, 0 },
    { NULL, 0 }
};

SFShapingKnowledge SFSimpleKnowledgeInstance = {
    &_SFSimpleKnowledgeSeekScript
};

static SFScriptKnowledgeRef _SFSimpleKnowledgeSeekScript(const void *object, SFTag scriptTag)
{
    return &_SFSimpleScriptKnowledge;
}

static SFShapingEngine _SFSimpleEngineBase = {
    &_SFSimpleEngineProcessAlbum
};

SF_INTERNAL void SFSimpleEngineInitialize(SFSimpleEngineRef simpleEngine, SFArtistRef artist)
{
    simpleEngine->_base = _SFSimpleEngineBase;
    simpleEngine->_artist = artist;
}

static void _SFSimpleEngineProcessAlbum(const void *object, SFAlbumRef album)
{
    SFSimpleEngineRef simpleEngine = (SFSimpleEngineRef)object;
    SFArtistRef artist = simpleEngine->_artist;
    SFTextProcessor processor;

    SFTextProcessorInitialize(&processor, artist->pattern, album, artist->textDirection, artist->textMode, SFFalse);
    SFTextProcessorDiscoverGlyphs(&processor);
    SFTextProcessorSubstituteGlyphs(&processor);
    SFTextProcessorPositionGlyphs(&processor);
    SFTextProcessorWrapUp(&processor);
}
