--[[
-------------------------------------------------------------------------------------------------------------------------|
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
|------------------------------------------------------------------------------------------------------------------------|

]]--

local M={}
-- Two opaque backgrounds measure coverage independently of the backend's alpha
-- blend equation. RGB = foreground * coverage + background * (1 - coverage).
-- Encode straight-alpha PNGs so ordinary sprite blending does not darken edges.
function M.compose(black,white,width,height,background)
    assert(#black==width*height*4 and #white==#black,'Invalid capture buffers')
    local rows={}
    local function byte(v) return math.max(0,math.min(255,math.floor(v+0.5))) end
    for y=0,height-1 do
        local row={}
        for x=0,width-1 do
            local i=(y*width+x)*4+1
            local r,g,b=black:byte(i,i+2)
            local wr,wg,wb=white:byte(i,i+2)
            local transmission=math.max(0,math.min(255,((wr-r)+(wg-g)+(wb-b))/3))/255
            local alpha=1-transmission
            local bgAlpha=background.a*transmission
            local outAlpha=alpha+bgAlpha
            if outAlpha<0.5/255 then row[x+1]=string.char(0,0,0,0)
            else
                row[x+1]=string.char(byte((r+255*background.r*bgAlpha)/outAlpha),
                    byte((g+255*background.g*bgAlpha)/outAlpha),
                    byte((b+255*background.b*bgAlpha)/outAlpha),byte(255*outAlpha))
            end
        end
        rows[y+1]=table.concat(row)
    end
    return table.concat(rows)
end
return M
