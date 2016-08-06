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

#include <cstdint>
#include <iostream>

extern "C" {
#include <SheenFigure/Source/SFAlbum.h>
#include <SheenFigure/Source/SFPattern.h>
#include <SheenFigure/Source/SFPatternBuilder.h>
#include <SheenFigure/Source/SFTextProcessor.h>
}

#include "OpenType/GSUB.h"
#include "OpenType/Writer.h"
#include "TextProcessorTester.h"

using namespace std;
using namespace SheenFigure::Tester;
using namespace SheenFigure::Tester::OpenType;

struct FontObject {
    Writer &writer;
    SFTag tag;
};

static void loadTable(void *object, SFTag tag, SFUInt8 *buffer, SFUInteger *length)
{
    FontObject *fontObject = reinterpret_cast<FontObject *>(object);

    if (tag == fontObject->tag) {
        if (buffer) {
            memcpy(buffer, fontObject->writer.data(), (size_t)fontObject->writer.size());
        }
        if (length) {
            *length = (SFUInteger)fontObject->writer.size();
        }
    }
}

static SFGlyphID getGlyphID(void *object, SFCodepoint codepoint)
{
    return (SFGlyphID)codepoint;
}

static SFAdvance getGlyphAdvance(void *object, SFFontLayout fontLayout, SFGlyphID glyphID)
{
    return (glyphID * 100);
}

static void writeTable(Writer &writer,
    LookupSubtable &subtable, LookupSubtable *referrals[], SFUInteger count)
{
    UInt16 lookupCount = (UInt16)(count + 1);

    /* Create the lookup tables. */
    LookupTable *lookups = new LookupTable[lookupCount];
    lookups[0].lookupType = subtable.lookupType();
    lookups[0].lookupFlag = (LookupFlag)0;
    lookups[0].subTableCount = 1;
    lookups[0].subtables = &subtable;
    lookups[0].markFilteringSet = 0;

    for (SFUInteger i = 1; i < lookupCount; i++) {
        LookupSubtable *other = referrals[i - 1];
        lookups[i].lookupType = other->lookupType();
        lookups[i].lookupFlag = (LookupFlag)0;
        lookups[i].subTableCount = 1;
        lookups[i].subtables = other;
        lookups[i].markFilteringSet = 0;
    }

    /* Create the lookup list table. */
    LookupListTable lookupList;
    lookupList.lookupCount = lookupCount;
    lookupList.lookupTables = lookups;

    UInt16 lookupIndex[1] = { 0 };

    /* Create the feature table. */
    FeatureTable testFeature;
    testFeature.featureParams = 0;
    testFeature.lookupCount = 1;
    testFeature.lookupListIndex = lookupIndex;

    /* Create the feature record. */
    FeatureRecord featureRecord[1];
    memcpy(&featureRecord[0].featureTag, "test", 4);
    featureRecord[0].feature = &testFeature;

    /* Create the feature list table. */
    FeatureListTable featureList;
    featureList.featureCount = 1;
    featureList.featureRecord = featureRecord;

    UInt16 dfltFeatureIndex[] = { 0 };

    /* Create the language system table. */
    LangSysTable dfltLangSys;
    dfltLangSys.lookupOrder = 0;
    dfltLangSys.reqFeatureIndex = 0xFFFF;
    dfltLangSys.featureCount = 1;
    dfltLangSys.featureIndex = dfltFeatureIndex;

    /* Create the script table. */
    ScriptTable dfltScript;
    dfltScript.defaultLangSys = &dfltLangSys;
    dfltScript.langSysCount = 0;
    dfltScript.langSysRecord = NULL;

    /* Create the script record. */
    ScriptRecord scripts[1];
    memcpy(&scripts[0].scriptTag, "dflt", 4);
    scripts[0].script = &dfltScript;

    /* Create the script list table */
    ScriptListTable scriptList;
    scriptList.scriptCount = 1;
    scriptList.scriptRecord = scripts;

    /* Create the container table. */
    GSUB gsub;
    gsub.version = 0x00010000;
    gsub.scriptList = &scriptList;
    gsub.featureList = &featureList;
    gsub.lookupList = &lookupList;

    writer.writeTable(&gsub);

    delete [] lookups;
}

static void processSubtable(SFAlbumRef album,
    SFCodepoint *input, SFUInteger length, SFBoolean positioning,
    LookupSubtable &subtable, LookupSubtable *referrals[], SFUInteger count)
{
    /* Write the table for the given lookup. */
    Writer writer;
    writeTable(writer, subtable, referrals, count);

    /* Create a font object containing writer and tag. */
    FontObject object = {
        .writer = writer,
        .tag = (positioning ? SFTagMake('G', 'P', 'O', 'S') : SFTagMake('G', 'S', 'U', 'B')),
    };

    /* Create the font with protocol. */
    SFFontProtocol protocol = {
        .loadTable = &loadTable,
        .getGlyphIDForCodepoint = &getGlyphID,
        .getAdvanceForGlyph = &getGlyphAdvance,
    };
    SFFontRef font = SFFontCreateWithProtocol(&protocol, &object);

    /* Create a pattern. */
    SFPatternRef pattern = SFPatternCreate();

    /* Build the pattern. */
    SFPatternBuilder builder;
    SFPatternBuilderInitialize(&builder, pattern);
    SFPatternBuilderSetFont(&builder, font);
    SFPatternBuilderSetScript(&builder, SFTagMake('d', 'f', 'l', 't'), SFTextDirectionLeftToRight);
    SFPatternBuilderSetLanguage(&builder, SFTagMake('d', 'f', 'l', 't'));
    SFPatternBuilderBeginFeatures(&builder, SFFeatureKindSubstitution);
    SFPatternBuilderAddFeature(&builder, SFTagMake('t', 'e', 's', 't'), 0);
    SFPatternBuilderAddLookup(&builder, 0);
    SFPatternBuilderMakeFeatureUnit(&builder);
    SFPatternBuilderEndFeatures(&builder);
    SFPatternBuilderBuild(&builder);

    /* Create the codepoint sequence. */
    SBCodepointSequence sequence;
    sequence.stringEncoding = SBStringEncodingUTF32;
    sequence.stringBuffer = input;
    sequence.stringLength = length;

    /* Reset the album for given codepoints. */
    SFCodepoints codepoints;
    SFCodepointsInitialize(&codepoints, &sequence, SFFalse);
    SFAlbumReset(album, &codepoints, length);

    /* Process the album. */
    SFTextProcessor processor;
    SFTextProcessorInitialize(&processor, pattern, album, SFTextDirectionLeftToRight, SFTextModeForward);
    SFTextProcessorDiscoverGlyphs(&processor);
    SFTextProcessorSubstituteGlyphs(&processor);
    SFTextProcessorPositionGlyphs(&processor);
    SFTextProcessorWrapUp(&processor);
    
    /* Release the allocated objects. */
    SFPatternRelease(pattern);
    SFFontRelease(font);
}

TextProcessorTester::TextProcessorTester()
{
}

void TextProcessorTester::processGSUB(SFAlbumRef album,
    SFCodepoint *input, SFUInteger length,
    LookupSubtable &subtable, LookupSubtable *referrals[], SFUInteger count)
{
    processSubtable(album, input, length, SFFalse, subtable, referrals, count);
}

void TextProcessorTester::processGPOS(SFAlbumRef album,
    SFCodepoint *input, SFUInteger length,
    LookupSubtable &subtable, LookupSubtable *referrals[], SFUInteger count)
{
    processSubtable(album, input, length, SFTrue, subtable, referrals, count);
}

void TextProcessorTester::test()
{
    testSingleSubstitution();
    testMultipleSubstitution();
    testLigatureSubstitution();
    testChainContextSubstitution();
}