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

#ifndef __SHEENFIGURE_TESTER__OPEN_TYPE__BASE_H
#define __SHEENFIGURE_TESTER__OPEN_TYPE__BASE_H

#include <cstdint>

namespace SheenFigure {
namespace Tester {
namespace OpenType {

class Writer;

typedef int16_t Int16;
typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef UInt16 Glyph;
typedef UInt16 Offset;

struct Table {
    virtual void write(Writer &writer) = 0;
};

}
}
}

#endif
