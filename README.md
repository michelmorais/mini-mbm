# Mini MBM	

[![Documentation Status](https://mbm-documentation.readthedocs.io/en/latest/)](https://mbm-documentation.readthedocs.io/en/latest/)


** Mini MBM ** is the given name to the graphics engine which offers basic and essential resources to ** develop games ** and/ or ** 2D and 3D ** applications.
At this version is possible to develop desktop (Windows,Linux) and Android (mobile).

You can find the latest online documentation (on going yet) [here](https://mbm-documentation.readthedocs.io/en/latest/).


The project include:

- The engine use ** opengles 2.0 ** as main core, ** LUA 5.4 ** as script, ** miniz ** as compression lib,  ** lodepng ** and ** stb ** to load images. 

What is optional?

- Lua: LUA is the main interface however is optional. (you can make it using C++)
- Box2d:   plugin/module for physics simulation with box 2d
- Bullet:  plugin/module for physics simulation with bullet 3d
- Editor features: Features which allow to edit your resource files. (use pre defined ** -DUSE_EDITOR_FEATURES ** to include)

Some useful LUA editor:

- All the LUA editor are in the folder 'editor' and are useful to help you make you application in a easy way.
- Sprite Maker: Creates sprite to be used in the engine. see [sprite-maker](https://mbm-documentation.readthedocs.io/en/latest/editors.html#sprite-maker)
- Font Maker: creates font for the engine from True Type Font (TTF). see [font-maker](https://mbm-documentation.readthedocs.io/en/latest/editors.html#font-maker).
- Scene Editor: Creates scenes 2D to easy position of the objects (images, sprites, meshes, etc.). see [scene-editor](https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d)
- Shader editor: Creating effects in the sprites, meshes, fonts, etc, using an editor to configure and save it in a binary format. see [shader-editor](https://mbm-documentation.readthedocs.io/en/latest/editors.html#shader-editor)
- Particle editor: Our particle editor in a simple way. see [particle-editor](https://mbm-documentation.readthedocs.io/en/latest/editors.html#particle-editor)
- Simulator: You can simulate using your own lua script just clicking a play button inside the scene editor.


## License

Released under the MIT license.

MIT License (MIT)                                                                                                     
Copyright (C) 2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      
                                                                                                                      
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          
documentation files (the "Software"), to deal in the Software without restriction, including without limitation       
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      
to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    
                                                                                                                      
The above copyright notice and this permission notice shall be included in all copies or substantial portions of      
the Software.                                                                                                         
                                                                                                                      
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.