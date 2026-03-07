/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal blend stubs.
// Blending in Metal is baked into MTLRenderPipelineState, not set at draw time
// like GL. For now these are stubs; full support requires dynamic pipeline-state
// management keyed on (shader × blend-mode).

#if defined(USE_METAL)

#include <blend.h>
#include <shader-fx.h>

namespace mbm
{
    void RENDER_STATE::set(const BLEND_STATE /*blendState*/) const noexcept
    {
        // TODO: store pending blend state; rebuild pipeline state before next draw.
    }

    void FX::setBlendDefaultOp()
    {
        // TODO: set blend equation to ADD in pipeline descriptor.
    }

    void FX::setBlendOp()
    {
        // TODO: map blendOperation enum to MTLBlendOperation.
    }

} // namespace mbm

#endif // USE_METAL
