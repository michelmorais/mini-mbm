/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal blend implementation.
// Blending in Metal is baked into MTLRenderPipelineState — it cannot be changed
// at draw time like GL/D3D9.  Each compiled shader already contains two PSO
// variants compiled by compileShader() in shader-metal.mm:
//
//   standardPSO — src_alpha × src + (1−src_alpha) × dst  (BLEND_INVSRCALPHA)
//   additivePSO — src_alpha × src + 1              × dst  (BLEND_ONE)
//
// RENDER_STATE::set() stores the requested BLEND_STATE on the device so that
// SHADER::render() / renderDynamic() can pick the right PSO before each draw.
//
// Blend equations (setBlendDefaultOp / setBlendOp): both PSOs are compiled with
// MTLBlendOperationAdd.  Operations 2–5 (SUBTRACT, REVERSE SUBTRACT, MIN, MAX)
// are not supported without additional PSO variants; they silently use ADD.

#if defined(USE_METAL)

#include <blend.h>
#include <shader-fx.h>
#include <device.h>
#include <specific-metal.h>

namespace mbm
{
    void RENDER_STATE::set(const BLEND_STATE blendState) const noexcept
    {
        // Store the requested blend state so SHADER::render() / renderDynamic()
        // can select the right pre-built PSO (standardPSO vs additivePSO).
        // BLEND_ONE (2) → additivePSO   (src_alpha × src + 1 × dst)
        // all others    → standardPSO  (src_alpha × src + (1−src_alpha) × dst)
        auto* dev = mbm::DEVICE::getInstance();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = dev ? dev->getSpecificContextDevice() : nullptr;
        if (ctx)
            ctx->currentBlendState = static_cast<int>(blendState);
    }

    void FX::setBlendDefaultOp()
    {
        // Both PSOs compile with MTLBlendOperationAdd — the default is already correct.
        // No runtime action needed.
    }

    void FX::setBlendOp()
    {
        // blendOperation == 1 (ADD) is already the compiled-in default for both PSOs.
        // Operations 2–5 (SUBTRACT, REVERSE SUBTRACT, MIN, MAX) would require
        // additional PSO variants per shader; they are not built currently.
        // The engine will continue rendering with ADD blend equation for those cases.
    }

} // namespace mbm

#endif // USE_METAL
