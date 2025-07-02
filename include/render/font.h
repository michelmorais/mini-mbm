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

    class TEXT_DRAW : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        friend class FONT_DRAW;
        OnRestoreFont onRestoreFont;
        FONT_DRAW *   parentFONT_DRAW;
        std::string   text;
        ALIGNED       aligned;
        float         spaceXCharacter;
        float         spaceYCharacter;
        uint8_t wildCardChangeAnim;
    
        API_IMPL virtual ~TEXT_DRAW();
        API_IMPL void release();
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, OnRestoreFont ptrOnRestoreFont,FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText, VEC3 &position,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText, VEC2 &position,OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW);
        API_IMPL static uint8_t withoutBOM2Map(uint8_t index, const uint8_t mapBoom) noexcept;
        API_IMPL void setText(const char *format, ...);
        API_IMPL bool getWidthHeight(float *_width, float *_height, const bool consider_scale = true) const override;
        API_IMPL void forceCalcSize();
        API_IMPL bool getWidthHeightString(float *_width, float *_height, const char *str);
        API_IMPL bool isOver3d(DEVICE *, const float x, const float y) const override;
        API_IMPL bool isOver2dw(DEVICE *, const float x, const float y) const override;
        API_IMPL bool isOver2ds(DEVICE *, const float x, const float y) const override;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
        API_IMPL bool setTexture(const MESH_MBM *mesh,const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;

    private:
        bool isOnFrustum() override;
        bool renderText(const bool doRender);
        bool render() override;
        ANIMATION *getNextIndexSpecialAnim(const std::string &textDraw, const uint32_t s, uint32_t *curIndex,uint32_t & indexNewAnim);
        bool onRestoreDevice() override;
        void onStop() override;
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
        const std::string getTextWithoutSpecialLetters()const;
        std::string textWithoutSpeciaLetters;
        MESH_MBM *mesh;
        float     widthFirstLetter;
        VEC2      beginText;
        VEC2      endText;
    };

    class FONT_DRAW : public COMMON_DEVICE
    {
      public:
        std::string fontName; // font's name
        API_IMPL FONT_DRAW(const SCENE *scene);
        API_IMPL virtual ~FONT_DRAW();
        API_IMPL void release();
        API_IMPL TEXT_DRAW *addText(const char *newText, VEC2 &position, const bool _is2dFont = true,const bool isScreen2d = true);
        API_IMPL TEXT_DRAW *addText(const char *newText, const bool _is2dFont = true, const bool isScreen2d = true);    
        API_IMPL TEXT_DRAW *addText(const char *newText, VEC3 &position, const bool _is2dFont = true,const bool isScreen2d = true);
        API_IMPL uint32_t getTotalText() const;
        API_IMPL TEXT_DRAW *getText(const uint8_t index);
        API_IMPL bool loadFont(const char *fileNameMbmOrTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng);
        API_IMPL const char *getFileName() const;
        API_IMPL MESH_MBM * getMesh();
        #ifdef USE_EDITOR_FEATURES
        API_IMPL const char *getFileNameTextureLoaded() const;
        API_IMPL void  setLetterXDiff(const char* letter,const float diffX);
        API_IMPL float getLetterXDiff(const char* letter)const;
        API_IMPL void  setLetterYDiff(const char* letter,const float diffY);
        API_IMPL float getLetterYDiff(const char* letter)const;
        API_IMPL void  setLetterSize(const char* letter,const uint32_t size_x,const uint32_t size_y);
        API_IMPL bool  getLetterSize(const char* letter,uint32_t & out_size_x,uint32_t & out_size_y)const;
        std::string texture_file_name_created;
        #endif
        API_IMPL void onStop();
      private:
        void fillAnimation(TEXT_DRAW *text);
        static bool OnRestoreFont(FONT_DRAW *that, TEXT_DRAW *TEXT_DRAW_ptr);
        bool OnRestore(TEXT_DRAW *whatText);
        bool isLoaded() const;
        #ifdef USE_EDITOR_FEATURES
        uint8_t getIndexFromLetter(const char* letter)const;
        #endif
    
        MESH_MBM *               mesh;
        std::vector<TEXT_DRAW *> lsText;
        const int                idScene;
        std::string              fileName;
    };
}
#endif
