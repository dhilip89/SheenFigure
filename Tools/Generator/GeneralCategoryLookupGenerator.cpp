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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Utilities/Math.h"
#include "Utilities/Converter.h"
#include "Utilities/ArrayBuilder.h"
#include "Utilities/FileBuilder.h"

#include "GeneralCategoryLookupGenerator.h"

using namespace std;
using namespace SheenFigure::Parser;
using namespace SheenFigure::Generator;
using namespace SheenFigure::Generator::Utilities;

static const size_t MIN_MAIN_SEGMENT_SIZE = 8;
static const size_t MAX_MAIN_SEGMENT_SIZE = 512;

static const size_t MIN_BRANCH_SEGMENT_SIZE = 8;
static const size_t MAX_BRANCH_SEGMENT_SIZE = 512;

static const string DATA_ARRAY_TYPE = "static const SFUInt8";
static const string DATA_ARRAY_NAME = "_SFGeneralCategoryPrimaryData";

static const string MAIN_INDEXES_ARRAY_TYPE = "static const SFUInt16";
static const string MAIN_INDEXES_ARRAY_NAME = "_SFGeneralCategoryMainIndexes";

static const string BRANCH_INDEXES_ARRAY_TYPE = "static const SFUInt16";
static const string BRANCH_INDEXES_ARRAY_NAME = "_SFGeneralCategoryBranchIndexes";

GeneralCategoryLookupGenerator::MainDataSegment::MainDataSegment(size_t index, MainDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string GeneralCategoryLookupGenerator::MainDataSegment::hintLine() const {
    return ("/* DATA_BLOCK: -- 0x" + Converter::toHex(index, 4) + "..0x" + Converter::toHex(index + dataset->size() - 1, 4) + " -- */");
}

GeneralCategoryLookupGenerator::BranchDataSegment::BranchDataSegment(size_t index, BranchDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string GeneralCategoryLookupGenerator::BranchDataSegment::hintLine() const {
    return ("/* INDEX_BLOCK: -- 0x" + Converter::toHex(index, 4) + "..0x" + Converter::toHex(index + dataset->size() - 1, 4) + " -- */");
}

GeneralCategoryLookupGenerator::GeneralCategoryLookupGenerator(const UnicodeData &unicodeData)
    : m_generalCategoryDetector(unicodeData)
    , m_firstCodePoint(unicodeData.firstCodepoint())
    , m_lastCodePoint(unicodeData.lastCodepoint())
    , m_mainSegmentSize(0)
    , m_branchSegmentSize(0)
{
}

void GeneralCategoryLookupGenerator::setMainSegmentSize(size_t segmentSize) {
    m_mainSegmentSize = min(MAX_MAIN_SEGMENT_SIZE, max(MIN_MAIN_SEGMENT_SIZE, segmentSize));
}

void GeneralCategoryLookupGenerator::setBranchSegmentSize(size_t segmentSize) {
    m_branchSegmentSize = min(MAX_BRANCH_SEGMENT_SIZE, max(MIN_BRANCH_SEGMENT_SIZE, segmentSize));
}

void GeneralCategoryLookupGenerator::displayBidiClassesFrequency() {
    map<uint8_t, size_t> frequency;

    for (uint32_t i = 0; i < m_lastCodePoint; i++) {
        frequency[m_generalCategoryDetector.numberForCodePoint(i)]++;
    }

    for (auto &element : frequency) {
        cout << m_generalCategoryDetector.numberToName(element.first) << " Code Points: " << element.second << endl;
    }
}

void GeneralCategoryLookupGenerator::analyzeData() {
    cout << "Analyzing data for general category lookup." << endl;

    size_t minMemory = SIZE_MAX;
    size_t mainSegmentSize = 0;
    size_t branchSegmentSize = 0;

    m_mainSegmentSize = MIN_MAIN_SEGMENT_SIZE;
    while (m_mainSegmentSize <= MAX_MAIN_SEGMENT_SIZE) {
        collectMainData();

        m_branchSegmentSize = MIN_BRANCH_SEGMENT_SIZE;
        while (m_branchSegmentSize <= MAX_BRANCH_SEGMENT_SIZE) {
            collectBranchData();

            size_t memory = m_dataSize + ((m_mainIndexesSize + m_branchIndexesSize) * 2);
            if (memory < minMemory) {
                mainSegmentSize = m_mainSegmentSize;
                branchSegmentSize = m_branchSegmentSize;
                minMemory = memory;
            }

            m_branchSegmentSize++;
        }

        m_mainSegmentSize++;
    }

    m_mainSegmentSize = mainSegmentSize;
    m_branchSegmentSize = branchSegmentSize;

    cout << "  Main Segment Size: " << mainSegmentSize << endl;
    cout << "  Branch Segment Size: " << branchSegmentSize << endl;
    cout << "  Required Memory: " << minMemory << " bytes";
}

void GeneralCategoryLookupGenerator::collectMainData() {
    size_t unicodeCount = m_lastCodePoint - m_firstCodePoint;
    size_t maxSegments = Math::FastCeil(unicodeCount, m_mainSegmentSize);

    m_dataSegments.clear();
    m_dataSegments.reserve(maxSegments);

    m_dataReferences.clear();
    m_dataReferences.reserve(maxSegments);

    m_dataSize = 0;

    uint8_t defaultBidiClass = m_generalCategoryDetector.nameToNumber("Cn");

    for (size_t i = 0; i < maxSegments; i++) {
        uint32_t segmentStart = m_firstCodePoint + (uint32_t)(i * m_mainSegmentSize);
        uint32_t segmentEnd = min(m_lastCodePoint, (uint32_t)(segmentStart + m_mainSegmentSize - 1));

        MainDataSet dataset(new UnsafeMainDataSet());
        dataset->reserve(m_mainSegmentSize);

        for (uint32_t unicode = segmentStart; unicode <= segmentEnd; unicode++) {
            uint8_t number = m_generalCategoryDetector.numberForCodePoint(unicode);
            if (number) {
                dataset->push_back(number);
            } else {
                dataset->push_back(defaultBidiClass);
            }
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_dataSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_dataSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_dataSegments.size();
            m_dataSegments.push_back(MainDataSegment(m_dataSize, dataset));
            m_dataSize += dataset->size();
        }

        m_dataReferences.push_back(&m_dataSegments.at(segmentIndex));
    }

    m_mainIndexesSize = m_dataReferences.size();
}

void GeneralCategoryLookupGenerator::collectBranchData() {
    size_t mainIndexesCount = m_dataReferences.size() - 1;
    size_t maxSegments = Math::FastCeil(mainIndexesCount, m_branchSegmentSize);

    m_branchSegments.clear();
    m_branchSegments.reserve(maxSegments);

    m_branchReferences.clear();
    m_branchReferences.reserve(maxSegments);

    m_mainIndexesSize = 0;

    for (size_t i = 0; i < maxSegments; i++) {
        size_t segmentStart = (i * m_branchSegmentSize);
        size_t segmentEnd = min(mainIndexesCount, segmentStart + m_branchSegmentSize - 1);

        BranchDataSet dataset(new UnsafeBranchDataSet());
        dataset->reserve(m_branchSegmentSize);

        for (size_t i = segmentStart; i <= segmentEnd; i++) {
            dataset->push_back(m_dataReferences.at(i));
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_branchSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_branchSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_branchSegments.size();
            m_branchSegments.push_back(BranchDataSegment(m_mainIndexesSize, dataset));
            m_mainIndexesSize += dataset->size();
        }

        m_branchReferences.push_back(&m_branchSegments.at(segmentIndex));
    }
    
    m_branchIndexesSize = m_branchReferences.size();
}

void GeneralCategoryLookupGenerator::generateFile(const std::string &directory) {
    collectMainData();
    collectBranchData();

    ArrayBuilder arrData;
    arrData.setDataType(DATA_ARRAY_TYPE);
    arrData.setName(DATA_ARRAY_NAME);
    arrData.setElementSpace(3);

    int dataCount = 0;
    auto dataPtr = m_dataSegments.begin();
    auto dataEnd = m_dataSegments.end();
    for (; dataPtr != dataEnd; dataPtr++) {
        const MainDataSegment &segment = *dataPtr;
        bool isLast = (dataPtr == (dataEnd - 1));

        arrData.append(segment.hintLine());
        arrData.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            arrData.appendElement(m_generalCategoryDetector.numberToName(segment.dataset->at(j)));
            dataCount++;

            if (!isLast || j != (length - 1)) {
                arrData.newElement();
            }
        }

        if (!isLast) {
            arrData.newLine();
        }
    }
    arrData.setSizeDescriptor(Converter::toString(dataCount));

    ArrayBuilder arrMainIndexes;
    arrMainIndexes.setDataType(MAIN_INDEXES_ARRAY_TYPE);
    arrMainIndexes.setName(MAIN_INDEXES_ARRAY_NAME);

    size_t segmentStart = m_firstCodePoint;
    int mainIndexCount = 0;
    auto mainIndexPtr = m_branchSegments.begin();
    auto mainIndexEnd = m_branchSegments.end();
    for (; mainIndexPtr != mainIndexEnd; mainIndexPtr++) {
        const BranchDataSegment &segment = *mainIndexPtr;
        bool isLast = (mainIndexPtr == (mainIndexEnd - 1));

        arrMainIndexes.append(segment.hintLine());
        arrMainIndexes.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            string element = "0x" + Converter::toHex(segment.dataset->at(j)->index, 4);
            arrMainIndexes.appendElement(element);
            mainIndexCount++;

            if (!isLast || j != (length - 1)) {
                arrMainIndexes.newElement();
            }
        }

        if (!isLast) {
            arrMainIndexes.newLine();
        }

        segmentStart += m_mainSegmentSize;
    }
    arrMainIndexes.setSizeDescriptor(Converter::toString(mainIndexCount));

    ArrayBuilder arrBranchIndexes;
    arrBranchIndexes.setDataType(BRANCH_INDEXES_ARRAY_TYPE);
    arrBranchIndexes.setName(BRANCH_INDEXES_ARRAY_NAME);

    segmentStart = 0;
    int branchIndexCount = 0;
    auto branchIndexPtr = m_branchReferences.begin();
    auto branchIndexEnd = m_branchReferences.end();
    for (; branchIndexPtr != branchIndexEnd; branchIndexPtr++) {
        const BranchDataSegment &segment = **branchIndexPtr;
        bool isLast = (branchIndexPtr == (branchIndexEnd - 1));
        string element = "0x" + Converter::toHex(segment.index, 4);

        arrBranchIndexes.appendElement(element);
        branchIndexCount++;

        if (!isLast) {
            arrBranchIndexes.newElement();
        }

        segmentStart += m_branchSegmentSize;
    }
    arrBranchIndexes.setSizeDescriptor(Converter::toString(branchIndexCount));

    set<string> categoryNames;
    string upperName;
    m_generalCategoryDetector.getAllNames(categoryNames);

    FileBuilder header(directory + "/SFGeneralCategoryLookup.h");
    header.append("/*").newLine();
    header.append(" * Automatically generated by SheenFigureGenerator tool. ").newLine();
    header.append(" * DO NOT EDIT!!").newLine();
    header.append(" */").newLine();
    header.newLine();
    header.append("#ifndef _SF_GENERAL_CATEGORY_LOOKUP_H").newLine();
    header.append("#define _SF_GENERAL_CATEGORY_LOOKUP_H").newLine();
    header.newLine();
    header.append("#include <SFConfig.h>").newLine();
    header.newLine();
    header.append("#include \"SFBase.h\"").newLine();
    header.append("#include \"SFGeneralCategory.h\"").newLine();
    header.newLine();
    header.append("SF_INTERNAL SFGeneralCategory SFGeneralCategoryDetermine(SFCodepoint codepoint);").newLine();
    header.newLine();
    header.append("#endif").newLine();

    FileBuilder source(directory + "/SFGeneralCategoryLookup.c");
    source.append("/*").newLine();
    source.append(" * Automatically generated by SheenFigureGenerator tool. ").newLine();
    source.append(" * DO NOT EDIT!!").newLine();
    source.append(" *").newLine();
    source.append(" * REQUIRED MEMORY: " + Converter::toString((int)m_dataSize)
                  + "+(" + Converter::toString((int)m_mainIndexesSize) + "*2)+("
                  + Converter::toString((int)m_branchIndexesSize) + "*2) = "
                  + Converter::toString((int)(m_dataSize + m_mainIndexesSize*2 + m_branchIndexesSize*2))
                  + " Bytes").newLine();
    source.append(" */").newLine();
    source.newLine();
    source.append("#include <SFConfig.h>").newLine();
    source.newLine();
    source.append("#include \"SFBase.h\"").newLine();
    source.append("#include \"SFGeneralCategory.h\"").newLine();
    source.append("#include \"SFGeneralCategoryLookup.h\"").newLine();
    source.newLine();

    for (string n : categoryNames) {
        upperName = n;
        Converter::toUpper(upperName);

        source.append("#define " + n).appendTab().append(" SFGeneralCategory" + upperName).newLine();
    }
    source.newLine();

    source.append(arrData).newLine();
    source.append(arrMainIndexes).newLine();
    source.append(arrBranchIndexes).newLine();

    string maxUnicode = "0x" + Converter::toHex(m_lastCodePoint, 6);
    string mainDivider = "0x" + Converter::toHex(m_mainSegmentSize, 4);
    string branchDivider = "0x" + Converter::toHex(m_mainSegmentSize * m_branchSegmentSize, 4);
    source.append("SF_INTERNAL SFGeneralCategory SFGeneralCategoryDetermine(SFCodepoint codepoint) {").newLine();
    source.appendTabs(1).append("if (codepoint <= " + maxUnicode + ") {").newLine();
    source.appendTabs(2).append("return " + DATA_ARRAY_NAME + "[").newLine();
    source.appendTabs(2).append("        " + MAIN_INDEXES_ARRAY_NAME + "[").newLine();
    source.appendTabs(2).append("         " + BRANCH_INDEXES_ARRAY_NAME + "[").newLine();
    source.appendTabs(2).append("              codepoint / " + branchDivider).newLine();
    source.appendTabs(2).append("         ] + (codepoint % " + branchDivider + ") / " + mainDivider).newLine();
    source.appendTabs(2).append("        ] + (codepoint % " + mainDivider + ")").newLine();
    source.appendTabs(2).append("       ];").newLine();
    source.appendTabs(1).append("}").newLine();
    source.newLine();
    source.appendTab().append("return SFGeneralCategoryCN;").newLine();
    source.append("}").newLine();
    source.newLine();

    for (string n : categoryNames) {
        source.append("#undef " + n).newLine();
    }
}
