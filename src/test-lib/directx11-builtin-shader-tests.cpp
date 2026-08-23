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
#if defined(USE_DIRECTX11)

#include "directx11-builtin-shader-tests.h"

#include <core_mbm/device.h>
#include <core_mbm/shader-cfg.h>
#include <core_mbm/shader-resource.h>
#include <d3dcompiler.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

int runDirectX11BuiltinShaderTests()
{
    uint32_t passed = 0;
    uint32_t failed = 0;
    const mbm::SHADER_CFG_LOADER &shaderConfig = mbm::DEVICE::getInstance()->getShaderConfig();
    auto compileShaders = [&passed, &failed](const std::vector<mbm::SHADER_CFG *> &shaders,
                                             const bool vertexShader)
    {
        for (const mbm::SHADER_CFG *shader : shaders)
        {
            if (!shader)
                continue;
            ID3DBlob *byteCode = nullptr;
            ID3DBlob *messages = nullptr;
            const char *name = shader->fileName.c_str();
            const std::string &source = shader->codeShader;
            const HRESULT result = D3DCompile(source.c_str(), source.size(), name, nullptr, nullptr, "main",
                                              vertexShader ? mbm::getVSVersion() : mbm::getPSVersion(),
                                              0, 0, &byteCode, &messages);
            if (SUCCEEDED(result))
            {
                ++passed;
                std::printf("PASS %s\n", name);
            }
            else
            {
                ++failed;
                std::printf("FAIL %s\n%s\n", name,
                            messages ? static_cast<const char *>(messages->GetBufferPointer()) : "unknown error");
            }
            if (messages)
                messages->Release();
            if (byteCode)
                byteCode->Release();
        }
    };
    compileShaders(shaderConfig.lsPs, false);
    compileShaders(shaderConfig.lsVs, true);
    std::printf("DirectX 11 built-in shader compilation: passed=%u failed=%u total=%u\n",
                passed, failed, passed + failed);
    return failed == 0 ? 0 : -1;
}

#endif
