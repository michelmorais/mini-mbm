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

#include <renderizable-clone.h>
#include <background.h>
#include <mesh.h>
#include <sprite.h>
#include <texture-view.h>
#include <background.h>
#include <gif-view.h>
#include <font.h>
#include <shape-mesh.h>
#include <line-mesh.h>
#include <particle.h>
#include <render-2-texture.h>
#include <tile.h>
#include <util-interface.h>
#include <animation.h>

namespace mbm
{
	RENDERIZABLE* clone(const SCENE* scene,RENDERIZABLE * clone_from)
	{
		if(clone_from == nullptr)
		{
			return nullptr;
		}
		if(clone_from->isLoaded() == false)
		{
			PRINT_IF_DEBUG("Renderizable to be cloned [%s] is not loaded",clone_from->getTypeClassName());
			return nullptr;
		}
		if(scene == nullptr)
		{
			PRINT_IF_DEBUG("Scene unexpected null...");
			return nullptr;
		}

		RENDERIZABLE * the_clone = nullptr;

		switch(clone_from->typeClass)
		{
			case TYPE_CLASS_MESH:          the_clone = new MESH(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_SPRITE:        the_clone = new SPRITE(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_TEXTURE:       the_clone = new TEXTURE_VIEW(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_BACKGROUND:    the_clone = new BACKGROUND(scene,clone_from->is3D); break;
			case TYPE_CLASS_GIF:           the_clone = new GIF_VIEW(scene,clone_from->is3D,clone_from->is2dS); break;
			//case TYPE_CLASS_TEXT:          the_clone = new TEXT_DRAW(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_SHAPE_MESH:    the_clone = new SHAPE_MESH(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_LINE_MESH:     the_clone = new LINE_MESH(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_PARTICLE:      the_clone = new PARTICLE(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_RENDER_2_TEX:  the_clone = new RENDER_2_TEXTURE(scene,clone_from->is3D,clone_from->is2dS); break;
			case TYPE_CLASS_TILE:          the_clone = new TILE(scene,clone_from->is3D,clone_from->is2dS); break;
			//case TYPE_CLASS_TILE_OBJ:      the_clone = new TILE_OBJ(scene,clone_from->is3D,clone_from->is2dS); break;
			default:
			{
				PRINT_IF_DEBUG("Could not clone object [%s] [%s]",clone_from->getTypeClassName(),clone_from->getFileName());
				return nullptr;
			}
		}

		if(clone_from->clone(the_clone) == true)
		{
			const auto * animFrom  = clone_from->getAnimationManager();
			auto * animClone = the_clone->getAnimationManager();
			if(animClone && animFrom)
				animClone->setAnimationByIndex(animFrom->getIndexAnimation());
		}
		else
		{
			delete the_clone;
			the_clone = nullptr;
			PRINT_IF_DEBUG("Could not clone object [%s] [%s]",clone_from->getTypeClassName(),clone_from->getFileName());
		}
		return the_clone;
	}
}

