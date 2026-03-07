/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal backend stubs for render-to-texture.
// All methods are no-ops / stubs for Milestone 1 (clear screen).
// Full render-to-texture support will be added in a later milestone.

#if defined(USE_METAL)

#include <specific-metal.h>
#include <scene.h>
#include <render-2-texture.h>
#include <texture-manager.h>
#include <util-interface.h>

namespace mbm
{

    RENDERIZABLE_TO_TARGET::RENDERIZABLE_TO_TARGET(const SCENE* scene,
                                                   const TYPE_CLASS newTypeClass,
                                                   const bool _is3d,
                                                   const bool _is2ds) noexcept :
        RENDERIZABLE(scene->getIdScene(), newTypeClass, _is3d, _is2ds)
    {
        this->specificConfig = new RENDER2TARGET_METAL();
        this->colorClearBackGround = COLOR(255, 255, 255);
        this->colorClearBackGround.a = 1.0f;
        this->widthTexture  = 0;
        this->heightTexture = 0;
    }

    RENDERIZABLE_TO_TARGET::~RENDERIZABLE_TO_TARGET()
    {
        delete static_cast<RENDER2TARGET_METAL*>(this->specificConfig);
    }

    FVF_PROVIDE_BY_ENGINE RENDERIZABLE_TO_TARGET::getFvfFromBuffer() const noexcept
    {
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG,
                                     const int /*x*/, const int /*y*/,
                                     const int /*_width*/, const int /*_height*/)
    {
        if (newFileOutNamePNG == nullptr)
            return log_util::fail(__LINE__, __FILE__, "file name to save png is null");
        // TODO: implement Metal render-to-texture readback and PNG save.
        WARN_LOG("Metal: RENDER_2_TEXTURE::saveAsPNG() is not yet implemented.");
        return false;
    }

} // namespace mbm

#endif // USE_METAL
