/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

/*
    Class identifier for the engine
*/

#include <class-identifier.h>

namespace mbm
{
    const char * getUserTypeAsString(const int value)
    {
        switch(value)
        {
            case L_USER_TYPE_BEGIN                  :return "_userType_begin";
            case L_USER_TYPE_VEC2                   :return "_usertype_vec2";
            case L_USER_TYPE_VEC3                   :return "_usertype_vec3";
            case L_USER_TYPE_RENDERIZABLE           :return "_usertype_renderizable";
            case L_USER_TYPE_TIMER                  :return "_usertype_timer";
            case L_USER_TYPE_SHADER                 :return "_usertype_shader";
            case L_USER_TYPE_AUDIO                  :return "_usertype_audio";
            case L_USER_TYPE_VR                     :return "_usertype_vr";
            case L_USER_TYPE_TEXTURE                :return "_usertype_texture";
            case L_USER_TYPE_SPRITE                 :return "_usertype_sprite";
            case L_USER_TYPE_CAMERA_TARGET          :return "_usertype_camera_2_target";
            case L_USER_TYPE_RENDER_2_TEXTURE       :return "_usertype_render_2_texture";
            case L_USER_TYPE_PARTICLE               :return "_usertype_particle";
            case L_USER_TYPE_STEERED_PARTICLE       :return "_usertype_steered_particle";
            case L_USER_TYPE_MESH                   :return "_usertype_mesh";
            case L_USER_TYPE_MESH_DEBUG             :return "_usertype_mesh_debug";
            case L_USER_TYPE_LINE                   :return "_usertype_line";
            case L_USER_TYPE_GIF                    :return "_usertype_gif";
            case L_USER_TYPE_SHAPE_MESH             :return "_usertype_frame_mesh";
            case L_USER_TYPE_FONT                   :return "_usertype_font";
            case L_USER_TYPE_TEXT                   :return "_usertype_text";
            case L_USER_TYPE_BACKGROUND             :return "_usertype_background";
            case L_USER_TYPE_BOX2D                  :return "_usertype_box2d";
            case L_USER_TYPE_BOX2D_JOINT            :return "_usertype_box2d_joint";
            case L_USER_TYPE_BOX2D_STEERED_PARTICLE :return "_usertype_box2d_steered_particle";
            case L_USER_TYPE_BULLET3D               :return "_usertype_bullet3d";
            case L_USER_TYPE_TILE                   :return "_usertype_tile";
            case L_USER_TYPE_TILE_OBJ               :return "_usertype_tile_obj";
            case L_USER_TYPE_PLUGIN                 :return "_usertype_plugin";
            case L_USER_TYPE_NEW_WRAPPER            :return "_usertype_new_wrapper";
            case L_USER_TYPE_END                    :return "_usertype_end";
            default                                 :return "_usertype_unknown"; 
        }
    }

    bool isRenderizableType(const int value)
    {
        switch(value)
        {
            case L_USER_TYPE_SHADER               :return true;
            case L_USER_TYPE_TEXTURE              :return true;
            case L_USER_TYPE_SPRITE               :return true;
            case L_USER_TYPE_PARTICLE             :return true;
            case L_USER_TYPE_MESH                 :return true;
            case L_USER_TYPE_LINE                 :return true;
            case L_USER_TYPE_GIF                  :return true;
            case L_USER_TYPE_SHAPE_MESH           :return true;
            case L_USER_TYPE_TEXT                 :return true;
            case L_USER_TYPE_BACKGROUND           :return true;
            case L_USER_TYPE_VR                   :return true;
            case L_USER_TYPE_RENDER_2_TEXTURE     :return true;
            case L_USER_TYPE_TILE                 :return true;
            case L_USER_TYPE_TILE_OBJ             :return true;
            case L_USER_TYPE_STEERED_PARTICLE     :return true;
            case L_USER_TYPE_PLUGIN               :return false;
            case L_USER_TYPE_NEW_WRAPPER          :return false;
            default: return false;
        }
    }

    bool isRenderizableType(const L_USER_TYPE value)
    {
        return isRenderizableType((int)value);
    }
}
