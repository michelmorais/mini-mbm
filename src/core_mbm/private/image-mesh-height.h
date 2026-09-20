/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef IMAGE_MESH_HEIGHT_H
#define IMAGE_MESH_HEIGHT_H
#include <core_mbm/image-mesh.h>
#include <core_mbm/util-interface.h>
#include <stb/stb-interface.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
namespace mbm { namespace image_mesh {
struct HEIGHT_FIELD
{
    uint32_t imageWidth=0,imageHeight=0,width=0,height=0;
    std::string path;
    std::unique_ptr<stbi_uc,decltype(&std::free)> pixels{nullptr,&std::free};
    std::vector<float> levels;
    bool load(const char *source,const IMAGE_MESH_OPTIONS &o,std::string &error)
    {
        const auto fail=[&](const char *m) { error=m; return false; };
        if (!source || !*source) return fail("Image path required");
        if (!std::isfinite(o.grooveThreshold) || o.grooveThreshold<0 || o.grooveThreshold>1 ||
            !std::isfinite(o.grooveTransition) || o.grooveTransition<0.001f || o.grooveTransition>1 ||
            !std::isfinite(o.heightTolerance) || o.heightTolerance<0.001f || o.heightTolerance>1 || o.smoothPasses>4)
            return fail("Invalid groove threshold [0,1], transition/tolerance [0.001,1], or smoothing passes [0,4]");
        bool exists=false; const char *resolved=util::getFullPath(source,&exists);
        path=exists?resolved:source;
        int iw=0,ih=0,channels=0;
        if (!stbi_info(path.c_str(),&iw,&ih,&channels) || iw<=0 || ih<=0 || static_cast<uint64_t>(iw)*ih>16777216)
            return fail("Cannot inspect image or image exceeds 16 megapixels");
        imageWidth=static_cast<uint32_t>(iw); imageHeight=static_cast<uint32_t>(ih);
        if (o.x>=imageWidth || o.y>=imageHeight) return fail("Crop origin outside image");
        width=o.cropWidth?o.cropWidth:imageWidth-o.x; height=o.cropHeight?o.cropHeight:imageHeight-o.y;
        if (width>imageWidth-o.x || height>imageHeight-o.y) return fail("Crop outside image");
        pixels.reset(stbi_load(path.c_str(),&iw,&ih,&channels,4));
        if (!pixels || iw!=static_cast<int>(imageWidth) || ih!=static_cast<int>(imageHeight))
            return fail("Cannot decode image or dimensions changed");
        levels.resize(static_cast<size_t>(width)*height);
        for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
        {
            const auto *p=pixels.get()+(static_cast<size_t>(y+o.y)*imageWidth+x+o.x)*4;
            const float v=(0.2126f*p[0]+0.7152f*p[1]+0.0722f*p[2])/255.0f;
            levels[static_cast<size_t>(y)*width+x]=o.invert?1-v:v;
        }
        std::vector<float> filtered;
        if (o.smoothPasses) filtered.resize(levels.size());
        for (uint32_t pass=0;pass<o.smoothPasses;++pass)
        {
            for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
            {
                const size_t index=static_cast<size_t>(y)*width+x;
                float sum=0,weight=0;
                for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
                {
                    const uint32_t xx=static_cast<uint32_t>(std::clamp(static_cast<int>(x)+dx,0,static_cast<int>(width)-1));
                    const uint32_t yy=static_cast<uint32_t>(std::clamp(static_cast<int>(y)+dy,0,static_cast<int>(height)-1));
                    const float v=levels[static_cast<size_t>(yy)*width+xx],d=v-levels[index];
                    const float w=1.0f/((1+dx*dx+dy*dy)*(1+100*d*d));
                    sum+=v*w; weight+=w;
                }
                filtered[index]=sum/weight;
            }
            levels.swap(filtered);
        }
        return true;
    }
    float sample(float u,float v) const
    {
        const float x=std::clamp(u,0.0f,1.0f)*(width-1),y=std::clamp(v,0.0f,1.0f)*(height-1);
        const auto x0=static_cast<uint32_t>(x),y0=static_cast<uint32_t>(y);
        const auto x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
        const float fx=x-x0,fy=y-y0;
        return (levels[static_cast<size_t>(y0)*width+x0]*(1-fx)+levels[static_cast<size_t>(y0)*width+x1]*fx)*(1-fy)+
               (levels[static_cast<size_t>(y1)*width+x0]*(1-fx)+levels[static_cast<size_t>(y1)*width+x1]*fx)*fy;
    }
    float mapped(float value,const IMAGE_MESH_OPTIONS &o) const
    {
        if (!o.twoLevels) return value;
        const float t=std::clamp((value-o.grooveThreshold)/o.grooveTransition+0.5f,0.0f,1.0f);
        return t*t*(3-2*t);
    }
};
} }
#endif
