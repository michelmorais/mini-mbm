--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]
function onInitScene()
 local rgba={10,20,30,0,40,50,60,64,70,80,90,128,100,110,120,255}
 local path='/tmp/read-image-pixels-rgba.png'
 assert(mbm.createTexture(rgba,2,2,4,'read_pixels_rgba',path))
 local bytes,w,h=mbm.readImagePixels(path)
 assert(w==2 and h==2 and #bytes==16)
 assert(bytes==mbm.readImagePixels(path,'rgba'))
 local alpha,aw,ah=mbm.readImagePixels(path,'alpha')
 assert(aw==w and ah==h and #alpha==4)
 for i=1,16 do assert(bytes:byte(i)==rgba[i],'RGBA order/value mismatch') end
 for i=1,4 do assert(alpha:byte(i)==rgba[i*4],'alpha extraction mismatch') end
 local rgb={1,2,3,4,5,6,7,8,9,10,11,12}
 path='/tmp/read-image-pixels-rgb.png'
 assert(mbm.createTexture(rgb,2,2,3,'read_pixels_rgb',path))
 assert(mbm.readImagePixels(path,'alpha')==string.rep(string.char(255),4))
 local missing,reason=mbm.readImagePixels('/tmp/read-image-pixels-missing.png','alpha')
 assert(not missing and type(reason)=='string')
 local ok=pcall(mbm.readImagePixels,path,'rgb');assert(not ok,'invalid format accepted')
 assert(mbm.readPngAlpha==nil,'obsolete registration remains')
 print('READ IMAGE PIXELS RGBA / ALPHA / OPAQUE / ARGUMENTS OK')
 mbm.quit()
end
