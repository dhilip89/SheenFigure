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
#include <stdlib.h>

#include "SFAlbum.h"
#include "SFBase.h"
#include "SFCommon.h"
#include "SFData.h"
#include "SFGDEF.h"
#include "SFLocator.h"
#include "SFOpenType.h"

#include "SFGlyphManipulation.h"
#include "SFGlyphPositioning.h"
#include "SFGlyphSubstitution.h"
#include "SFTextProcessor.h"

enum {
    _SFGlyphZoneInput = 0,
    _SFGlyphZoneBacktrack = 1,
    _SFGlyphZoneLookahead = 2
};
typedef SFUInt8 _SFGlyphZone;

typedef struct {
    void *helperPtr;
    SFUInt16 recordValue;
    SFGlyphID glyphID;
    _SFGlyphZone glyphZone;
} _SFGlyphAgent;

typedef SFBoolean (*_SFGlyphAssessment)(_SFGlyphAgent *glyphAgent);

static SFBoolean _SFApplyRuleSetTable(SFTextProcessorRef textProcessor,
    SFData ruleSet, _SFGlyphAssessment glyphAsessment, void *helperPtr);
static SFBoolean _SFApplyRuleTable(SFTextProcessorRef textProcessor,
    SFData rule, SFBoolean includeFirst, _SFGlyphAssessment glyphAsessment, void *helperPtr);

static SFBoolean _SFApplyChainRuleSetTable(SFTextProcessorRef textProcessor,
    SFData chainRuleSet, _SFGlyphAssessment glyphAsessment, void *helperPtr);
static SFBoolean _SFApplyChainRuleTable(SFTextProcessorRef textProcessor,
    SFData chainRule, SFBoolean includeFirst, _SFGlyphAssessment glyphAsessment, void *helperPtr);

static SFBoolean _SFApplyContextLookups(SFTextProcessorRef textProcessor,
    SFData lookupArray, SFUInteger lookupCount, SFUInteger contextStart, SFUInteger contextEnd);

static SFBoolean _SFAssessGlyphByEquality(_SFGlyphAgent *glyphAgent)
{
    return (glyphAgent->glyphID == glyphAgent->recordValue);
}

static SFBoolean _SFAssessGlyphByClass(_SFGlyphAgent *glyphAgent)
{
    SFData classDef = NULL;
    SFUInt16 glyphClass;

    switch (glyphAgent->glyphZone) {
        case _SFGlyphZoneInput:
            classDef = ((SFData *)glyphAgent->helperPtr)[0];
            break;

        case _SFGlyphZoneBacktrack:
            classDef = ((SFData *)glyphAgent->helperPtr)[1];
            break;

        case _SFGlyphZoneLookahead:
            classDef = ((SFData *)glyphAgent->helperPtr)[2];
            break;
    }

    glyphClass = SFOpenTypeSearchGlyphClass(classDef, glyphAgent->glyphID);

    return (glyphClass == glyphAgent->recordValue);
}

static SFBoolean _SFAssessGlyphByCoverage(_SFGlyphAgent *glyphAgent)
{
    SFData coverage = SFData_Subdata((SFData)glyphAgent->helperPtr, glyphAgent->recordValue);
    SFUInteger covIndex;

    covIndex = SFOpenTypeSearchCoverageIndex(coverage, glyphAgent->glyphID);

    return (covIndex != SFInvalidIndex);
}

static SFBoolean _SFAssessBacktrackGlyphs(SFTextProcessorRef textProcessor,
    SFData valueArray, SFUInteger valueCount, _SFGlyphAssessment glyphAssessment, void *helperPtr)
{
    SFAlbumRef album = textProcessor->_album;
    SFLocatorRef locator = &textProcessor->_locator;
    SFUInteger backIndex = locator->index;
    SFUInteger valueIndex;
    _SFGlyphAgent glyphAgent;

    glyphAgent.helperPtr = helperPtr;
    glyphAgent.glyphZone = _SFGlyphZoneBacktrack;

    for (valueIndex = 0; valueIndex < valueCount; valueIndex++) {
        backIndex = SFLocatorGetBefore(locator, backIndex);

        if (backIndex != SFInvalidIndex) {
            glyphAgent.glyphID = SFAlbumGetGlyph(album, backIndex);
            glyphAgent.recordValue = SFUInt16Array_Value(valueArray, valueIndex);

            if (!glyphAssessment(&glyphAgent)) {
                return SFFalse;
            }
        } else {
            return SFFalse;
        }
    }

    return SFTrue;
}

static SFBoolean _SFAssessInputGlyphs(SFTextProcessorRef textProcessor,
    SFData valueArray, SFUInteger valueCount, SFBoolean includeFirst,
   _SFGlyphAssessment glyphAssessment, void *helperPtr, SFUInteger *contextEnd)
{
    SFAlbumRef album = textProcessor->_album;
    SFLocatorRef locator = &textProcessor->_locator;
    SFUInteger inputIndex = locator->index;
    SFUInteger valueIndex = 0;
    _SFGlyphAgent glyphAgent;

    glyphAgent.helperPtr = helperPtr;
    glyphAgent.glyphZone = _SFGlyphZoneInput;

    if (includeFirst) {
        glyphAgent.glyphID = SFAlbumGetGlyph(album, inputIndex);
        glyphAgent.recordValue = SFUInt16Array_Value(valueArray, 0);

        if (!glyphAssessment(&glyphAgent)) {
            return SFFalse;
        }

        valueIndex += 1;
    } else {
        valueCount -= 1;
    }

    for (; valueIndex < valueCount; valueIndex++) {
        inputIndex = SFLocatorGetAfter(locator, inputIndex);

        if (inputIndex != SFInvalidIndex) {
            glyphAgent.glyphID = SFAlbumGetGlyph(album, inputIndex);
            glyphAgent.recordValue = SFUInt16Array_Value(valueArray, valueIndex);

            if (!glyphAssessment(&glyphAgent)) {
                return SFFalse;
            }
        } else {
            return SFFalse;
        }
    }

    *contextEnd = inputIndex;
    return SFTrue;
}

static SFBoolean _SFAssessLookaheadGlyphs(SFTextProcessorRef textProcessor,
    SFData valueArray, SFUInteger valueCount,
    _SFGlyphAssessment glyphAssessment, void *helperPtr, SFUInteger contextEnd)
{
    SFAlbumRef album = textProcessor->_album;
    SFLocatorRef locator = &textProcessor->_locator;
    SFUInteger aheadIndex = contextEnd;
    SFUInteger valueIndex;
    _SFGlyphAgent glyphAgent;

    glyphAgent.helperPtr = helperPtr;
    glyphAgent.glyphZone = _SFGlyphZoneLookahead;

    for (valueIndex = 0; valueIndex < valueCount; valueIndex++) {
        aheadIndex = SFLocatorGetAfter(locator, aheadIndex);

        if (aheadIndex != SFInvalidIndex) {
            glyphAgent.glyphID = SFAlbumGetGlyph(album, aheadIndex);
            glyphAgent.recordValue = SFUInt16Array_Value(valueArray, valueIndex);

            if (!glyphAssessment(&glyphAgent)) {
                return SFFalse;
            }
        } else {
            return SFFalse;
        }
    }

    return SFTrue;
}

SF_PRIVATE SFBoolean _SFApplyContextSubtable(SFTextProcessorRef textProcessor, SFData context)
{
    SFAlbumRef album = textProcessor->_album;
    SFLocatorRef locator = &textProcessor->_locator;
    SFUInt16 tblFormat;

    tblFormat = SFContext_Format(context);

    switch (tblFormat) {
        case 1: {
            SFData coverage = SFContextF1_CoverageTable(context);
            SFUInt16 ruleSetCount = SFContextF1_RuleSetCount(context);
            SFGlyphID locGlyph;
            SFUInteger covIndex;

            locGlyph = SFAlbumGetGlyph(album, locator->index);
            covIndex = SFOpenTypeSearchCoverageIndex(coverage, locGlyph);

            if (covIndex < ruleSetCount) {
                SFData ruleSet = SFContextF1_RuleSetTable(context, covIndex);
                return _SFApplyRuleSetTable(textProcessor, ruleSet, _SFAssessGlyphByEquality, NULL);
            }
            break;
        }

        case 2: {
            SFData coverage = SFContextF2_CoverageTable(context);
            SFGlyphID locGlyph;
            SFUInteger covIndex;

            locGlyph = SFAlbumGetGlyph(album, locator->index);
            covIndex = SFOpenTypeSearchCoverageIndex(coverage, locGlyph);

            if (covIndex != SFInvalidIndex) {
                SFData classDef = SFContextF2_ClassDefTable(context);
                SFUInt16 ruleSetCount = SFContextF2_RuleSetCount(context);
                SFUInt16 locClass;

                locClass = SFOpenTypeSearchGlyphClass(classDef, locGlyph);

                if (locClass < ruleSetCount) {
                    SFData ruleSet = SFContextF2_RuleSetTable(context, locClass);
                    return _SFApplyRuleSetTable(textProcessor, ruleSet, _SFAssessGlyphByClass, &classDef);
                }
            }
            break;
        }

        case 3: {
            SFData rule = SFContextF3_Rule(context);
            return _SFApplyRuleTable(textProcessor, rule, SFTrue, _SFAssessGlyphByCoverage, (void *)context);
        }
    }
    
    return SFFalse;
}

static SFBoolean _SFApplyRuleSetTable(SFTextProcessorRef textProcessor,
    SFData ruleSet, _SFGlyphAssessment glyphAsessment, void *helperPtr)
{
    SFUInt16 ruleCount = SFRuleSet_RuleCount(ruleSet);
    SFUInteger ruleIndex;

    /* Match each rule sequentially as they are ordered by preference. */
    for (ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
        SFOffset ruleOffset = SFRuleSet_RuleOffset(ruleSet, ruleIndex);

        if (ruleOffset) {
            SFData rule = SFData_Subdata(ruleSet, ruleOffset);

            if (_SFApplyRuleTable(textProcessor, rule, SFFalse, glyphAsessment, helperPtr)) {
                return SFTrue;
            }
        }
    }

    return SFFalse;
}

static SFBoolean _SFApplyRuleTable(SFTextProcessorRef textProcessor,
    SFData rule, SFBoolean includeFirst, _SFGlyphAssessment glyphAsessment, void *helperPtr)
{
    SFUInt16 glyphCount = SFRule_GlyphCount(rule);

    /* Make sure that rule table contains at least one glyph. */
    if (glyphCount > 0) {
        SFUInt16 lookupCount = SFRule_LookupCount(rule);
        SFData valueArray = SFRule_ValueArray(rule);
        SFData lookupArray = SFRule_LookupArray(rule, glyphCount - !includeFirst);
        SFUInteger contextStart = textProcessor->_locator.index;
        SFUInteger contextEnd;

        return (_SFAssessInputGlyphs(textProcessor, valueArray, glyphCount, includeFirst, glyphAsessment, helperPtr, &contextEnd)
             && _SFApplyContextLookups(textProcessor, lookupArray, lookupCount, contextStart, contextEnd));
    }

    return SFFalse;
}

SF_PRIVATE SFBoolean _SFApplyChainContextSubtable(SFTextProcessorRef textProcessor, SFData chainContext)
{
    SFAlbumRef album = textProcessor->_album;
    SFLocatorRef locator = &textProcessor->_locator;
    SFUInt16 tblFormat;
    
    tblFormat = SFChainContext_Format(chainContext);

    switch (tblFormat) {
        case 1: {
            SFData coverage = SFChainContextF1_CoverageTable(chainContext);
            SFUInt16 ruleSetCount = SFChainContextF1_ChainRuleSetCount(chainContext);
            SFGlyphID locGlyph;
            SFUInteger covIndex;

            locGlyph = SFAlbumGetGlyph(album, locator->index);
            covIndex = SFOpenTypeSearchCoverageIndex(coverage, locGlyph);

            if (covIndex < ruleSetCount) {
                SFData chainRuleSet = SFChainContextF1_ChainRuleSetTable(chainContext, covIndex);
                return _SFApplyChainRuleSetTable(textProcessor, chainRuleSet, _SFAssessGlyphByEquality, NULL);
            }
            break;
        }

        case 2: {
            SFData coverage = SFChainContextF2_CoverageTable(chainContext);
            SFGlyphID locGlyph;
            SFUInteger covIndex;

            locGlyph = SFAlbumGetGlyph(album, locator->index);
            covIndex = SFOpenTypeSearchCoverageIndex(coverage, locGlyph);

            if (covIndex != SFInvalidIndex) {
                SFData backtrackClassDef = SFChainContextF2_BacktrackClassDefTable(chainContext);
                SFData inputClassDef = SFChainContextF2_InputClassDefTable(chainContext);
                SFData lookaheadClassDef = SFChainContextF2_LookaheadClassDefTable(chainContext);
                SFUInt16 chainRuleSetCount = SFChainContextF2_ChainRuleSetCount(chainContext);
                SFUInt16 inputClass;

                inputClass = SFOpenTypeSearchGlyphClass(inputClassDef, locGlyph);

                if (inputClass < chainRuleSetCount) {
                    SFData chainRuleSet = SFChainContextF2_ChainRuleSetTable(chainContext, inputClass);
                    SFData helpers[3];

                    helpers[0] = inputClassDef;
                    helpers[1] = backtrackClassDef;
                    helpers[2] = lookaheadClassDef;

                    return _SFApplyChainRuleSetTable(textProcessor, chainRuleSet, _SFAssessGlyphByClass, helpers);
                }
            }
            break;
        }

        case 3: {
            SFData chainRule = SFChainContextF3_ChainRuleTable(chainContext);
            return _SFApplyChainRuleTable(textProcessor, chainRule, SFTrue, _SFAssessGlyphByCoverage, (void *)chainContext);
        }
    }

    return SFFalse;
}

static SFBoolean _SFApplyChainRuleSetTable(SFTextProcessorRef textProcessor,
    SFData chainRuleSet, _SFGlyphAssessment glyphAsessment, void *helperPtr)
{
    SFUInt16 ruleCount = SFChainRuleSet_ChainRuleCount(chainRuleSet);
    SFUInteger ruleIndex;

    /* Match each rule sequentially as they are ordered by preference. */
    for (ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
        SFData chainRule = SFChainRuleSet_ChainRuleTable(chainRuleSet, ruleIndex);

        if (_SFApplyChainRuleTable(textProcessor, chainRule, SFFalse, glyphAsessment, helperPtr)) {
            return SFTrue;
        }
    }

    return SFFalse;
}

static SFBoolean _SFApplyChainRuleTable(SFTextProcessorRef textProcessor,
    SFData chainRule, SFBoolean includeFirst, _SFGlyphAssessment glyphAsessment, void *helperPtr)
{
    SFData backtrackRecord = SFChainRule_BacktrackRecord(chainRule);
    SFUInt16 backtrackCount = SFBacktrackRecord_GlyphCount(backtrackRecord);
    SFData backtrackArray = SFBacktrackRecord_ValueArray(backtrackRecord);
    SFData inputRecord = SFBacktrackRecord_InputRecord(backtrackRecord, backtrackCount);
    SFUInt16 inputCount = SFInputRecord_GlyphCount(inputRecord);

    /* Make sure that input record has at least one glyph. */
    if (inputCount > 0) {
        SFData inputArray = SFInputRecord_ValueArray(inputRecord);
        SFData lookaheadRecord = SFInputRecord_LookaheadRecord(inputRecord, inputCount - !includeFirst);
        SFUInt16 lookaheadCount = SFLookaheadRecord_GlyphCount(lookaheadRecord);
        SFData lookaheadArray = SFLookaheadRecord_ValueArray(lookaheadRecord);
        SFData contextRecord = SFLookaheadRecord_ContextRecord(lookaheadRecord, lookaheadCount);
        SFUInteger lookupCount = SFContextRecord_LookupCount(contextRecord);
        SFData lookupArray = SFContextRecord_LookupArray(contextRecord);
        SFUInteger contextStart = textProcessor->_locator.index;
        SFUInteger contextEnd;

        return (_SFAssessInputGlyphs(textProcessor, inputArray, inputCount, includeFirst, glyphAsessment, helperPtr, &contextEnd)
             && _SFAssessBacktrackGlyphs(textProcessor, backtrackArray, backtrackCount, glyphAsessment, helperPtr)
             && _SFAssessLookaheadGlyphs(textProcessor, lookaheadArray, lookaheadCount, glyphAsessment, helperPtr, contextEnd)
             && _SFApplyContextLookups(textProcessor, lookupArray, lookupCount, contextStart, contextEnd));
    }

    return SFFalse;
}

static SFBoolean _SFApplyContextLookups(SFTextProcessorRef textProcessor,
    SFData lookupArray, SFUInteger lookupCount, SFUInteger contextStart, SFUInteger contextEnd)
{
    SFLocatorRef contextLocator = &textProcessor->_locator;
    SFLocator originalLocator = *contextLocator;
    SFUInteger lookupIndex;

    /* Make the context locator cover only context range. */
    SFLocatorReset(contextLocator, contextStart, (contextEnd - contextStart) + 1);

    /* Apply the lookup records sequentially as they are ordered by preference. */
    for (lookupIndex = 0; lookupIndex < lookupCount; lookupIndex++) {
        SFData lookupRecord = SFLookupArray_Value(lookupArray, lookupIndex);
        SFUInt16 sequenceIndex = SFLookupRecord_SequenceIndex(lookupRecord);
        SFUInt16 lookupListIndex = SFLookupRecord_LookupListIndex(lookupRecord);

        /* Jump the locator to context index. */
        SFLocatorJumpTo(contextLocator, contextStart);
        
        if (SFLocatorMoveNext(contextLocator)) {
            /* Skip the glyphs till sequence index and apply the lookup. */
            if (SFLocatorSkip(contextLocator, sequenceIndex)) {
                _SFApplyLookup(textProcessor, lookupListIndex);
            }
        }
    }

    /* Take the state of context locator so that input glyphs are skipped properly. */
    SFLocatorTakeState(&originalLocator, contextLocator);
    /* Switch back to the original locator. */
    textProcessor->_locator = originalLocator;

    return SFTrue;
}

SF_PRIVATE SFBoolean _SFApplyExtensionSubtable(SFTextProcessorRef textProcessor, SFData extension)
{
    SFUInt16 tblFormat = SFExtension_Format(extension);

    switch (tblFormat) {
        case 1: {
            SFLookupType lookupType = SFExtensionF1_LookupType(extension);
            SFData innerSubtable = SFExtensionF1_ExtensionData(extension);

            return textProcessor->_lookupOperation(textProcessor, lookupType, innerSubtable);
        }
    }
    
    return SFFalse;
}
