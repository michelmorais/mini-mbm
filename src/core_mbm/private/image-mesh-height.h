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
    std::vector<float> levels, painted;
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
        return paint(o,error);
    }
    bool paint(const IMAGE_MESH_OPTIONS &o,std::string &error)
    {
        if (o.heightEditCount>4096 || (o.heightEditCount && !o.heightEdits))
        { error="Invalid heightEdits: maximum 4096 dabs"; return false; }
        if (!o.heightEditCount) return true;
        for (uint32_t i=0;i<o.heightEditCount;++i)
        {
            const auto &d=o.heightEdits[i];
            const auto unit=[](float v) { return std::isfinite(v) && v>=0 && v<=1; };
            if (!unit(d.x) || !unit(d.y) || !unit(d.radius) || d.radius<0.001f ||
                !unit(d.strength) || !unit(d.height) || d.mode<IMAGE_MESH_BRUSH::RAISE || d.mode>IMAGE_MESH_BRUSH::SMOOTH)
            { error="Invalid heightEdits dab: coordinates/strength/height [0,1], radius [0.001,1]"; return false; }
        }
        painted.reserve(levels.size());
        for (float v:levels) painted.push_back(mapped(v,o));
        uint64_t work=0;
        std::vector<float> patch;
        for (uint32_t i=0;i<o.heightEditCount;++i)
        {
            const auto &d=o.heightEdits[i];
            const float cx=d.x*(width-1),cy=d.y*(height-1);
            const float radius=std::max(0.5f,d.radius*std::max(1u,std::min(width,height)-1));
            const int x0=std::max(0,static_cast<int>(std::floor(cx-radius)));
            const int y0=std::max(0,static_cast<int>(std::floor(cy-radius)));
            const int x1=std::min(static_cast<int>(width)-1,static_cast<int>(std::ceil(cx+radius)));
            const int y1=std::min(static_cast<int>(height)-1,static_cast<int>(std::ceil(cy+radius)));
            const size_t pw=static_cast<size_t>(x1-x0+1),ph=static_cast<size_t>(y1-y0+1);
            work+=pw*ph*(d.mode==IMAGE_MESH_BRUSH::SMOOTH?9:1);
            if (work>64000000) { error="Height painting exceeds 64 million pixel operations; reduce dabs or brush radius"; return false; }
            patch.resize(pw*ph);
            for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x)
            {
                const size_t index=static_cast<size_t>(y)*width+x;
                const float old=painted[index];
                const float distance=std::sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy))/radius;
                const float falloff=std::max(0.0f,1-distance);
                const float weight=d.strength*falloff*falloff*(3-2*falloff);
                float value=old;
                if (d.mode==IMAGE_MESH_BRUSH::RAISE) value=old+weight;
                else if (d.mode==IMAGE_MESH_BRUSH::LOWER) value=old-weight;
                else if (d.mode==IMAGE_MESH_BRUSH::FLATTEN) value=old+(d.height-old)*weight;
                else
                {
                    float sum=0;
                    for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
                    {
                        const int xx=std::clamp(x+dx,0,static_cast<int>(width)-1);
                        const int yy=std::clamp(y+dy,0,static_cast<int>(height)-1);
                        sum+=painted[static_cast<size_t>(yy)*width+xx];
                    }
                    value=old+(sum/9-old)*weight;
                }
                patch[static_cast<size_t>(y-y0)*pw+x-x0]=std::clamp(value,0.0f,1.0f);
            }
            // Commit the whole dab after sampling, so smoothing does not depend on scan direction.
            for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x)
                painted[static_cast<size_t>(y)*width+x]=patch[static_cast<size_t>(y-y0)*pw+x-x0];
        }
        // Store only the correction: untouched cells must retain the original remap-after-interpolation path.
        for (size_t i=0;i<painted.size();++i) painted[i]-=mapped(levels[i],o);
        return true;
    }
    float interpolate(float u,float v,const std::vector<float> &values) const
    {
        const float x=std::clamp(u,0.0f,1.0f)*(width-1),y=std::clamp(v,0.0f,1.0f)*(height-1);
        const auto x0=static_cast<uint32_t>(x),y0=static_cast<uint32_t>(y);
        const auto x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
        const float fx=x-x0,fy=y-y0;
        return (values[static_cast<size_t>(y0)*width+x0]*(1-fx)+values[static_cast<size_t>(y0)*width+x1]*fx)*(1-fy)+
               (values[static_cast<size_t>(y1)*width+x0]*(1-fx)+values[static_cast<size_t>(y1)*width+x1]*fx)*fy;
    }
    float sample(float u,float v) const { return interpolate(u,v,levels); }
    float surface(float u,float v,const IMAGE_MESH_OPTIONS &o) const
    { return painted.empty()?mapped(sample(u,v),o):std::clamp(mapped(sample(u,v),o)+interpolate(u,v,painted),0.0f,1.0f); }
    float mapped(float value,const IMAGE_MESH_OPTIONS &o) const
    {
        if (!o.twoLevels) return value;
        const float t=std::clamp((value-o.grooveThreshold)/o.grooveTransition+0.5f,0.0f,1.0f);
        return t*t*(3-2*t);
    }
};
} }
#endif
