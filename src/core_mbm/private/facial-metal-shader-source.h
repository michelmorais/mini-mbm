/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef FACIAL_METAL_SHADER_SOURCE_H
#define FACIAL_METAL_SHADER_SOURCE_H
#include <cstdint>
#include <string>

namespace mbm::facial
{
    inline void appendMetalFacialDeformation(std::string &source, const uint32_t targets,
                                             const uint32_t width, const uint32_t rows,
                                             const bool normals)
    {
        source += "uint facialId=facialVertexId+facialVertexOffset;uint2 facialCoord=uint2(facialId%" +
            std::to_string(width) + ",facialId/" + std::to_string(width) + ");bool facialActive=false;\n";
        source += "for(uint t=0;t<" + std::to_string(targets) +
            ";++t){float w=facialWeights[t];if(w==0.0f)continue;facialActive=true;uint2 c=facialCoord+uint2(0,t*" +
            std::to_string(rows) + ");in.pos+=facialDeltas.read(c).xyz*w;\n";
        if (normals)
            source += "in.nor+=facialDeltas.read(c+uint2(0," + std::to_string(rows / 2u) + ")).xyz*w;\n";
        source += "}\n";
        if (normals)
            source += "if(facialActive && dot(in.nor,in.nor)>=1e-12f)in.nor=normalize(in.nor);\n";
    }
}
#endif
