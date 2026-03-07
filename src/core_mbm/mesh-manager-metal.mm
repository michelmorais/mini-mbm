/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal mesh-manager stub.
// MESH_MBM_DEBUG::fillInSubsetDebug() is the backend-specific function needed
// by the mesh debugging system. For Milestone 1 it is not called, but the
// linker requires the symbol.

#if defined(USE_METAL)

#include <mesh-manager.h>
#include <util-interface.h>

namespace mbm
{
    bool MESH_MBM_DEBUG::fillInSubsetDebug(
        const MESH_MBM* /*meshMemory*/,
        const int /*currentFrame*/,
        const std::map<int, float>& /*lsLetterChangedValuesByCurFrameX*/,
        const std::map<int, float>& /*lsLetterChangedValuesByCurFrameY*/,
        util::HEADER_FRAME* /*headerFrame*/,
        util::BUFFER_MESH_DEBUG* /*pBuffer*/)
    {
        // TODO: implement Metal vertex/index buffer debug readback.
        return false;
    }

} // namespace mbm

#endif // USE_METAL
