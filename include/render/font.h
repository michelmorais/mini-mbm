/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef FONT_GLES_H
#define FONT_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace mbm
{
    enum ALIGNED : char
    {
        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT,
    };
    class FONT_DRAW;
    class TEXT_DRAW;
    typedef bool (*OnRestoreFont)(FONT_DRAW *FONT_DRAW, TEXT_DRAW *TEXT_DRAW_ptr);

    class TEXT_DRAW : public RENDERIZABLE, public ANIMATION_MANAGER
    {
      public:
        friend class FONT_DRAW;
        OnRestoreFont onRestoreFont;
        FONT_DRAW *   parentFONT_DRAW;
        ALIGNED       aligned;
        float         spaceXCharacter;
        float         spaceYCharacter;
        uint8_t wildCardChangeAnim;
        // Bounding box in position-space coordinates (screen pixels for is2dS, world units for is2dW/is3D).
        // Updated by forceCalcSize(). For is2dS: aabbMin=(position.x, position.y) top-left,
        // aabbMax=(position.x+w, position.y+h) bottom-right. For is2dW/is3D: aabbMin.y = position.y-h.
        // Note: if position changes without a forceCalcSize() call, aabbMin/aabbMax become stale;
        // use getAABB() for size only (it is always correct) and position.xy for origin.
        VEC2 aabbMin;
        VEC2 aabbMax;
    
        API_IMPL virtual ~TEXT_DRAW();
        API_IMPL void release();
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, OnRestoreFont ptrOnRestoreFont,FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText,const VEC3 &position,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText,const VEC2 &position,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL static uint8_t withoutBOM2Map(uint8_t index, const uint8_t mapBoom) noexcept;
        // Sets the text and automatically updates bounds (no need to call forceCalcSize after).
        API_IMPL void setText(const char *format, ...);
        // Returns the current text string.
        API_IMPL const std::string& getText() const;
        API_IMPL bool getWidthHeight(float *_width, float *_height, const bool consider_scale = true) const override;
        // Recalculates bounding dimensions. Call after changing scale or position.
        // Not needed after setText() — that calls it automatically.
        API_IMPL void forceCalcSize();
        API_IMPL bool getWidthHeightString(float *_width, float *_height, const char *str);
        // Returns the bounding size in position-space units (screen pixels for is2dS, world units otherwise).
        using RENDERIZABLE::getAABB;
        API_IMPL void getAABB(float *w, float *h) const override;
        API_IMPL bool isOver3d(DEVICE *, const float x, const float y) const override;
        API_IMPL bool isOver2dw(DEVICE *, const float x, const float y) const override;
        API_IMPL bool isOver2ds(DEVICE *, const float x, const float y) const override;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
        API_IMPL bool setTexture(const MESH_MBM *mesh,const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;
        FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;

    private:
        bool isOnFrustum() override;
        bool renderText(const bool doRender);
        bool render() override;
        ANIMATION *getNextIndexSpecialAnim(const std::string &textDraw, const uint32_t s, uint32_t *curIndex,uint32_t & indexNewAnim);
        bool onRestoreDevice() override;
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
        const std::string getTextWithoutSpecialLetters()const;
        std::string textWithoutSpeciaLetters;
        std::string text;
        MESH_MBM *mesh;
        float     widthFirstLetter;
        VEC2      beginText;
        VEC2      endText;
    };

    class FONT_DRAW
    {
      public:
        std::string fontName; // font's name
        API_IMPL FONT_DRAW(const SCENE *scene);
        API_IMPL virtual ~FONT_DRAW();
        API_IMPL void release();
        API_IMPL TEXT_DRAW *addText(const char *newText,const VEC2 &position, const bool _is2dFont = true,const bool isScreen2d = true);
        API_IMPL TEXT_DRAW *addText(const char *newText, const bool _is2dFont = true, const bool isScreen2d = true);    
        API_IMPL TEXT_DRAW *addText(const char *newText,const VEC3 &position, const bool _is2dFont = true,const bool isScreen2d = true);
        API_IMPL uint32_t getTotalText() const;
        API_IMPL TEXT_DRAW *getText(const uint8_t index);
        API_IMPL bool loadFont(const char *fileNameMbmOrTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng);
        API_IMPL const char *getFileName() const;
        API_IMPL MESH_MBM * getMesh();
        API_IMPL const char *getFileNameTextureLoaded() const;
        API_IMPL void  setLetterXDiff(const char* letter,const float diffX);
        API_IMPL float getLetterXDiff(const char* letter)const;
        API_IMPL void  setLetterYDiff(const char* letter,const float diffY);
        API_IMPL float getLetterYDiff(const char* letter)const;
        API_IMPL void  setLetterSize(const char* letter,const uint32_t size_x,const uint32_t size_y);
        API_IMPL bool  getLetterSize(const char* letter,uint32_t & out_size_x,uint32_t & out_size_y)const;
        std::string texture_file_name_created;
        API_IMPL void onStop();
      private:
        void fillAnimation(TEXT_DRAW *text);
        static bool OnRestoreFont(FONT_DRAW *that, TEXT_DRAW *TEXT_DRAW_ptr);
        bool OnRestore(TEXT_DRAW *whatText);
        bool isLoaded() const;
        uint8_t getIndexFromLetter(const char* letter)const;
    
        MESH_MBM *               mesh;
        std::vector<TEXT_DRAW *> lsText;
        const int                idScene;
        std::string              fileName;
    };
}
#endif
