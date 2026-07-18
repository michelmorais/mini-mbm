/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if defined (USE_DUMMY_BACK_END_ENGINE)

#include <core_mbm/core-exports.h>
#include <string>
#include <stdio.h>

namespace mbm
{
    static const char* resourceShader[] = {//organizado de 3 em 3. sendo: Nome do arquivo, Codigo shader e configurações CFG.


    /* Alpha It -----------------------------------------------------------------------------------------------------*/

    //AlphaIt *********************
    "alpharit.ps",

    "TODO",

    "[ps-alpharit.ps] = alpharit.ps\n"
    "[ps-alpharit.ps][float][ray] = min 0.000000 max 1.000000 default 0.05000000\n"
    "[ps-alpharit.ps][vector2][center] = min 0.000000 0.000000 max 1.000000 1.000000 default 0.500000 0.500000\n"
    "[ps-alpharit.ps][vector2][prop] = min 0.000000 0.000000 max 100.000000 100.000000 default 1.000000 1.33333\n"
    "[ps-alpharit.ps][float][alpha] = min 0.000000 max 1.000000 default 1.000000",

    // tint **********************
    "tint.ps",

    "TODO",

    "[ps-tint.ps] = tint.ps\n"
    "[ps-tint.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

    // color it **********************
    "color it.ps",

    "TODO",

    "[ps-color-it.ps] = color it.ps\n"
    "[ps-color-it.ps][float][enable]        = min 0.0   max 1.0   default 1.0 \n"
    "[ps-color-it.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

    //blend *********************
    "blend.ps",

    "TODO",

    "[ps-blend.ps] = blend.ps\n"
    "[ps-blend.ps][float][invertSample] = min 0 max 1 default 0 \n"
    "[ps-blend.ps][float][disableSample1] = min 0 max 1 default 0 \n"
    "[ps-blend.ps][rgb][colorAdd] = min 0 0 0 max 255 255 255 default 255 0 0 \n"
    "[ps-blend.ps][rgba][junctionRemove] = min 0 0 0 0 max 255 255 255 255 default 0.2 0.2 0.2 0.0  \n",
    //blend *********************



    //font *********************
    "font.ps",

    "TODO",

    "[ps-font.ps] = font.ps\n"
    "[ps-font.ps][rgb][colorFont]             = min 0 0 0   max 1.0 1.0 1.0 default 1.0 1.0 1.0   \n",
    //Font *********************

    //Color Keying *********************
    "color keying.ps",

    "TODO",

    "[ps-color-keying.ps] = color keying.ps\n"
    "[ps-color-keying.ps][float][granThen]             = min 0.0              max 1.0              default 1.0             #boolean\n"
    "[ps-color-keying.ps][float][tolerance]            = min 0.0              max 1.0              default 0.3             #float\n"
    "[ps-color-keying.ps][rgba][colorDst]              = min 0.1 0.2 0.3 1.0  max 1.0 1.0 1.0 1.0  default 1.0 0.0 1.0 1.0 #cor RGBA definida como float\n"
    "[ps-color-keying.ps][rgba][colorSrc]              = min 10 20 30  255    max 125 128 250 255  default 125 128 50 255  #cor RGBA\n",
    //Color Keying *********************

    //transparent *********************
    "transparent.ps",

    "TODO",

    "[ps-transparent.ps] = transparent.ps\n"
    "[ps-transparent.ps][float][alpha]                = min 0.0              max 1.0              default 1.0\n",
    //transparent *********************

    //outline *********************
    "outline.ps",

    "TODO",

    "[ps-outline.ps] = outline.ps\n"
    "[ps-outline.ps][rgb][color] = min 0.0 0.0 0.0 max 1.0 1.0 1.0 default 1.0 0.9 0.1\n"
    "[ps-outline.ps][float][thickness] = min 0.01 max 0.5 default 0.12\n",
    //outline *********************

    //saturate *********************
    "saturate.ps",

    "TODO",

    "[ps-saturate.ps] = saturate.ps\n"
    "[ps-saturate.ps][rgb][color]                = min 0.0 0.0 0.0           max 1.0 1.0 1.0           default 1.0 1.0 1.0\n",
    //saturate *********************

    //Night Vision **********************
    "night vision.ps",

    "TODO",

    "[ps-night-vision.ps] = night vision.ps\n"
    "[ps-night-vision.ps][float][fInverseViewportWidth]   = min 0.00001    max 1.0      default 1.0\n"
    "[ps-night-vision.ps][float][fInverseViewportHeight]  = min 0.00001    max 1.0      default 1.0\n",
    //Night Vision **********************

    //Night Vision blur **********************
    "night vision blur.ps",

    "TODO",

    "[ps-night-vision-blur.ps] = night vision blur.ps\n"
    "[ps-night-vision-blur.ps][float][brightness]   = min 0.00001    max 100.0     default 10\n"
    "[ps-night-vision-blur.ps][float][contrast]     = min 0.00001    max 10.0      default 6.8\n",
    //Night Vision blur **********************

    //Multi textura **********************
    "multi textura.ps",

    "TODO",

    "[ps-multi-textura.ps] = multi textura.ps\n"
    "[ps-multi-textura.ps][float][gamma]   	= min 0.0    max 100.0     default 2.0\n",
    //Multi Textura **********************

    //Wave **********************
    "wave.ps",

    "TODO",

    "[ps-wave.ps] = wave.ps\n"
    "[ps-wave.ps][float][effectTime]   = min 0.0    max 10.0      default 0.0\n"
    "[ps-wave.ps][float][sizeWave]     = min 0.0    max 100.0     default 10\n",
    //Wave **********************

    //Bands **********************
    "bands.ps",

    "TODO",

    "[ps-bands.ps] = bands.ps\n"
    "[ps-bands.ps][float][bandDensity]   = min 0.0   max 150.0   default 65.0\n"
    "[ps-bands.ps][float][bandIntensity] = min 0.001 max 0.56    default 0.56\n",
    //Bands **********************

    //Bloom **********************
    "bloom.ps",

    "TODO",

    "[ps-bloom.ps] = bloom.ps\n"
    "[ps-bloom.ps][float][BloomIntensity]   = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseIntensity]    = min 0.0   max 2.0   default 0.5\n"
    "[ps-bloom.ps][float][BloomSaturation]  = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseSaturation]   = min 0.0   max 2.0   default 0.5\n",
    //Bloom **********************

    //Bright Extract **********************
    "bright extract.ps",

    "TODO",

    "[ps-bright-extract.ps] = bright extract.ps\n"
    "[ps-bright-extract.ps][float][threshold]   = min 0.0   max 1.0   default 0.5\n",
    //Bright Extract **********************

    //Color Tone **********************
    "color tone.ps",

    "TODO",

    "[ps-color-tone.ps] = color tone.ps\n"
    "[ps-color-tone.ps][float][desaturation] = min 0.0      max 1.0              default 0.5\n"
    "[ps-color-tone.ps][float][toned]        = min 0.0      max 1.0              default 0.5\n"
    "[ps-color-tone.ps][rgba][lightColor]    = min 0 0 0 0  max 255 255 255 255  default 255 255 255 255\n"
    "[ps-color-tone.ps][rgba][darkColor]     = min 0 0 0 0  max 255 255 255 255  default 255 255 0 0.7\n",
    //Color Tone **********************

    //Brightness **********************
    "brightness.ps",

    "TODO",

    "[ps-brightness.ps] = brightness.ps\n"
    "[ps-brightness.ps][float][brightness] = min 0.0      max 1.0              default 0.5\n"
    "[ps-brightness.ps][float][contrast]   = min 0.0      max 2.0              default 1.5\n",
    //Brightness **********************

    //Blur Directional **********************
    "blur directional.ps",

    "TODO",

    "[ps-blur-directional.ps] = blur directional.ps\n"
    "[ps-blur-directional.ps][float][angle]      = min 0.0      max 360.0      default 0.0\n"
    "[ps-blur-directional.ps][float][blurAmount] = min 0.000    max 0.01       default 0.000\n",
    //Blur Direcional **********************

    //Embossed **********************
    "embossed.ps",

    "TODO",

    "[ps-embossed.ps] = embossed.ps\n"
    "[ps-embossed.ps][float][amount]      = min 0.0      max 1.0      default 0.5\n"
    "[ps-embossed.ps][float][width]       = min 0.0      max 0.1      default 0.0022999998\n",
    //Embossed **********************

    //Frosty out line **********************
    "frosty out line.ps",

    "TODO",

    "[ps-frosty-out-line.ps] = frosty out line.ps\n"
    "[ps-frosty-out-line.ps][float][width]      = min 0.0      max 650.0      default 300.0\n"
    "[ps-frosty-out-line.ps][float][height]     = min 0.0      max 500.0      default 300.0\n",
    //Frosty Out Line **********************

    //Glass Tile **********************
    "glass tile.ps",

    "TODO",

    "[ps-glass-tile.ps] = glass tile.ps\n"
    "[ps-glass-tile.ps][float][tiles]        = min 0.0           max 20.0                 default 5.0\n"
    "[ps-glass-tile.ps][float][bevelWidth]   = min 1.0           max 10.0                 default 300.0\n"
    "[ps-glass-tile.ps][float][offset]       = min 0.0           max 3.0                  default 300.0\n"
    "[ps-glass-tile.ps][rgba][groutColor]    = min 0 0 0 0       max 255 255 255 255      default 0 0 0 0 \n",
    //Glass Tile **********************

    //Poisson **********************
    "poisson.ps",

    "TODO",

    "[ps-poisson.ps] = poisson.ps\n"
    "[ps-poisson.ps][float][poisson]       = min 1.0           max 10.0                 default 3.0\n"
    "[ps-poisson.ps][vector2][inputSize]   = min 1.0 1.0       max 1000.0 1000.0        default 600.0 400.0\n",
    //Poisson **********************

    //Invert Color **********************
    "invert color.ps",

    "TODO",

    "[ps-invert-color.ps] = invert color.ps\n",
    //Invert Color **********************

    //out of bounds **********************
    "out of bounds.ps",

    "TODO",

    "[ps-out-of-bounds.ps] = out of bounds.ps\n"
    "[ps-out-of-bounds.ps][rgb][color]           = min 0 0 0     max 1.0 0.0 1.0     default 1.0 0 0 \n",
    //out of bounds **********************

    //Light streak **********************
    "light streak.ps",

    "TODO",

    "[ps-light-streak.ps] = light streak.ps\n"
    "[ps-light-streak.ps][float][brightThreshold]  = min 0.0           max 1.0                 default 0.5\n"
    "[ps-light-streak.ps][float][scale]            = min 0.0           max 1.0                 default 0.5\n"
    "[ps-light-streak.ps][vector2][direction]      = min -1.0 -1.0     max 1.0 1.0             default 0.5 1.0\n"
    "[ps-light-streak.ps][vector2][attenuation]    = min 1.0 1.0       max 1000.0 1000.0       default 800.0 600.0\n",
    //Light streak **********************

    //Magnifying glass **********************
    "magnifying glass.ps",

    "TODO",

    "[ps-magnifying-glass.ps] = magnifying glass.ps\n"
    "[ps-magnifying-glass.ps][vector2][center]       = min 0.0 0.0       max 1.0 1.0             default 0.5 0.5\n"
    "[ps-magnifying-glass.ps][float][radius]         = min 0.0           max 1.0                 default 0.25\n"
    "[ps-magnifying-glass.ps][float][magnification]  = min 0.0           max 5.0                 default 2.0\n"
    "[ps-magnifying-glass.ps][float][aspectRatio]    = min 0.5           max 2.0                 default 1.33\n",
    //Light streak **********************

    //Old Movie **********************
    "old movie.ps",

    "TODO",

    "[ps-old-movie.ps] = old movie.ps\n"
    "[ps-old-movie.ps][float][scratchAmount]       = min 0.00001     max 0.01            default 0.0044\n"
    "[ps-old-movie.ps][float][noiseAmount]         = min 0.000001    max 1.0             default 0.000001\n"
    "[ps-old-movie.ps][float][frame]               = min 0.0         max 2.0             default 1.0\n",
    //Old Movie **********************

    //Pinch mouse **********************
    "pinch mouse.ps",

    "TODO",

    "[ps-pinch-mouse.ps] = pinch mouse.ps\n"
    "[ps-pinch-mouse.ps][vector2][center]         = min 0.0 0.0     max 794.0 678.0  default 0.5 0.5\n"
    "[ps-pinch-mouse.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
    "[ps-pinch-mouse.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
    "[ps-pinch-mouse.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",
    //Pinch mouse**********************


    //Pinch **********************
    "pinch.ps",

    "TODO",

    "[ps-pinch.ps] = pinch.ps\n"
    "[ps-pinch.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
    "[ps-pinch.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
    "[ps-pinch.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
    "[ps-pinch.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",
    //Pinch **********************

    //Ripple **********************
    "ripple.ps",

    "TODO",

    "[ps-ripple.ps] = ripple.ps\n"
    "[ps-ripple.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
    "[ps-ripple.ps][float][amplitude]        = min 0.0         max 1.0          default 0.1\n"
    "[ps-ripple.ps][float][frequency]        = min 0.0         max 100.0        default 70.0\n"
    "[ps-ripple.ps][float][phase]            = min -20.0       max 20.0         default 0.0\n"
    "[ps-ripple.ps][float][aspectRatio]      = min 0.5         max 2.0          default 1.34\n",
    //Ripple **********************

    //Sharpen **********************
    "sharpen.ps",

    "TODO",

    "[ps-sharpen.ps] = sharpen.ps\n"
    "[ps-sharpen.ps][vector2][inputSize]     = min 1.0 1.0     max 1000.0 1000.0      default 800.0 600.0\n"
    "[ps-sharpen.ps][float][amount]          = min 0.0         max 2.0                default 1.0 \n",
    //sharpen **********************


    //Sketch **********************
    "sketch.ps",

    "TODO",

    "[ps-sketch.ps] = sketch.ps\n"
    "[ps-sketch.ps][float][brushSize]          = min 0.0006         max 1.0                default 0.003 \n",
    //Sketch **********************

    //Smooth Magnify **********************
    "smooth magnify.ps",

    "TODO",

    "[ps-smooth-magnify.ps] = smooth magnify.ps\n"
    "[ps-smooth-magnify.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-smooth-magnify.ps][float][innerRadius]       = min 0.0         max 1.0                default 0.2 \n"
    "[ps-smooth-magnify.ps][float][outerRadius]       = min 0.0         max 1.0                default 0.4 \n"
    "[ps-smooth-magnify.ps][float][magnification]     = min 0.0         max 5.0                default 2.0 \n"
    "[ps-smooth-magnify.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",
    //Smooth Magnify **********************


    //Spiral **********************
    "spiral.ps",

    "TODO",

    "[ps-spiral.ps] = spiral.ps\n"

    "[ps-spiral.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-spiral.ps][float][spiralStrength]    = min 0.0         max 20.0               default 10.0 \n"
    "[ps-spiral.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",
    //Spiral **********************

    //Tone Mapping **********************
    "tone mapping.ps",

    "TODO",

    "[ps-tone-mapping.ps] = tone mapping.ps\n"
    "[ps-tone-mapping.ps][vector2][vignetteCenter]  = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-tone-mapping.ps][float][defog]             = min 0.0         max 1.0                default 0.4 \n"
    "[ps-tone-mapping.ps][float][exposure]          = min -1.0        max 1.0                default -0.2 \n"
    "[ps-tone-mapping.ps][float][gamma]             = min 0.5         max 2.0                default 0.63 \n"
    "[ps-tone-mapping.ps][float][vignetteRadius]    = min 0.0         max 1.0                default 0.5 \n"
    "[ps-tone-mapping.ps][float][vignetteAmount]    = min -1.0        max 1.0                default 0.0 \n"
    "[ps-tone-mapping.ps][float][blueShift]         = min 0.0         max 10.0               default 10.0 \n"
    "[ps-tone-mapping.ps][rgba][fogColor]           = min 0 0 0 0     max 255 255 255 255    default 255 255 255 255 \n",
    //Tone Mapping **********************

    //Toon **********************
    "toon.ps",

    "TODO",

    "[ps-toon.ps] = toon.ps\n"
    "[ps-toon.ps][float][levels]         = min 0.0         max 15.0               default 5.0 \n",
    //Toon **********************

    //Fade **********************
    "fade.ps",

    "TODO",

    "[ps-fade.ps] = fade.ps\n"
    "[ps-fade.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
    //Fade **********************

    //Fade Radial **********************
    "fade radial.ps",

    "TODO",

    "[ps-fade-radial.ps] = fade radial.ps\n"
    "[ps-fade-radial.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
    //Fade Radial **********************


    //Fade Ripple **********************
    "fade ripple.ps",

    "TODO",

    "[ps-fade-ripple.ps] = fade ripple.ps\n"
    "[ps-fade-ripple.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
    //Fade Ripple **********************

    //Fade Saturate **********************
    "fade saturate.ps",

    "TODO",

    "[ps-fade-saturate.ps] = fade saturate.ps\n"
    "[ps-fade-saturate.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
    //Fade Saturate **********************


    //Fade Twist **********************
    "fade twist.ps",

    "TODO",

    "[ps-fade-twist.ps] = fade twist.ps\n"
    "[ps-fade-twist.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
    "[ps-fade-twist.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",
    //Fade Twist **********************

    //Fade Twist Grid **********************
    "fade twist grid.ps",

    "TODO",

    "[ps-fade-twist-grid.ps] = fade twist grid.ps\n"
    "[ps-fade-twist-grid.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
    "[ps-fade-twist-grid.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",
    //Fade Twist Grid **********************

    //Fade Wave **********************
    "fade wave.ps",

    "TODO",

    "[ps-fade-wave.ps] = fade wave.ps\n"
    "[ps-fade-wave.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
    //Fade Wave **********************


    //Blur com Zoom **********************
    "blur zoom.ps",

    "TODO",

    "[ps-blur-zoom.ps] = blur zoom.ps\n"
    "[ps-blur-zoom.ps][vector2][center]           = min 0.0 0.0     max 1.0 1.0             default 30.0 30.0 \n"
    "[ps-blur-zoom.ps][float][blurAmount]         = min 0.0         max 2.0                 default 0.2 \n",
    //Blur com Zoom **********************


    //Texture Map **********************
    "texture map.ps",

    "TODO",

    "[ps-texture-map.ps] = texture map.ps\n"
    "[ps-texture-map.ps][float][horizontalSize]       = min 0.0         max 5.0                 default 1.0 \n"
    "[ps-texture-map.ps][float][verticalSize]         = min 0.0         max 5.0                 default 1.0 \n"
    "[ps-texture-map.ps][float][horizontalOffset]     = min 0.0         max 1.0                 default 0.0 \n"
    "[ps-texture-map.ps][float][verticalOffset]       = min 0.0         max 1.0                 default 0.0 \n"
    "[ps-texture-map.ps][float][strength]             = min 0.0         max 10.0                default 1.0 \n",
    //Texture Map **********************




    /* VERTEX SHADER -----------------------------------------------------------------------------------------------------*/




    //Outline **********************
    "outline.vs",

    "TODO",

    "[vs-outline.vs] = outline.vs\n",
    //Outline **********************

    //Textura Simples **********************
    "simple texture.vs",

    "TODO",

    "[vs-simple-texture.vs] = simple texture.vs\n",
    //Textura Simples **********************


    //Escala simples **********************
    "scale.vs",
    
    "TODO",

    "[vs-scale.vs] = scale.vs\n"
    "[vs-scale.vs][vector2][scale]       = min -10.0 -10.0         max 10.0  10.0                default 1.0 1.0\n",
    //Escala simples **********************


    //Escala Diff **********************
    "scale diff.vs",

    "TODO",

    "[vs-scale-diff.vs] = scale diff.vs\n"
    "[vs-scale-diff.vs][float][scale]       = min 0.0         max 3.0                 default 1.0 \n"
    "[vs-scale-diff.vs][float][height]      = min -100.0      max 100.0               default 0.0 \n"
    "[vs-scale-diff.vs][float][maxHeight]   = min -100.0      max 100.0               default 0.0 \n",
    //Escala Diff **********************

    //Escala Diff by Y **********************
    "scale diff by y.vs",

    "TODO",

    "[vs-scale-diff-by-y.vs] = scale diff by y.vs\n"
    "[vs-scale-diff-by-y.vs][float][scale]       = min 0.0         max 3.0                 default 1.0 \n"
    "[vs-scale-diff-by-y.vs][float][height]      = min -100.0      max 100.0               default 0.0 \n"
    "[vs-scale-diff-by-y.vs][float][maxHeight]   = min -100.0      max 100.0               default 0.0 \n",
    //Escala Diff by Y **********************


    //Fluttering **********************
    "fluttering.vs",

    "TODO",

    "[vs-fluttering.vs] = fluttering.vs\n"
    "[vs-fluttering.vs][float][wave]       = min 0.0         max 1.0                 default 1.0 \n"
    "[vs-fluttering.vs][float][deepth]     = min 0.0         max 10.0                default 1.0 \n",
    //Fluttering **********************

    nullptr,nullptr,nullptr };

    const char** getShaderEngineBuiltIn()
    {
        return resourceShader;
    }

    const char* getCodePScolorFor_LINE_MESH()
    {
        // Pixel Shader for line LINE_MESH (we do not expose to user, that is why is not part of resourceShader)
        static const char* codePScolor_LINE_MESH = "TODO";

        return codePScolor_LINE_MESH;
    }
    const char* getCodeVScolorFor_LINE_MESH()
    {
        // Vertex Shader for line LINE_MESH (we do not expose to user, that is why is not part of resourceShader)
        static const char* codeVsColor_LINE_MESH = "TODO";

        return codeVsColor_LINE_MESH;
    }

    API_IMPL const char* getParticlePSCode()
    {
        static const char* psParticleCode = "TODO";

        return psParticleCode;
    }

    const char* getParticleVSCode()
    {
        static const char* vsParticleCode = "TODO";

        return vsParticleCode;
    }

    const char* getSteeredParticlePSCode(bool hasColor)
    {
        if (hasColor)
        {
            return  "TODO";
        }
        else
        {
            return  "TODO";
        }
    }
    const char* getSteeredParticleVSCode()
    {
        return "TODO";
    }

    static std::string PS_Vesrion("TODO_version_ps");
    static std::string VS_Vesrion("TODO_version_vs");

    const char* getPSVersion()
    {
        return PS_Vesrion.c_str();
    }
    const char* getVSVersion()
    {
        return VS_Vesrion.c_str();
    }
    void setPSVersion(const char* version)
    {
        if (version == nullptr)
            version = "";
        PS_Vesrion = version;
    }
    void setVSVersion(const char* version)
    {
        if (version == nullptr)
            version = "";
        VS_Vesrion = version;
    }
}


#endif
