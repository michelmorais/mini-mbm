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
#include "image-mesh-height-areas.h"
#include "image-mesh-progress.h"
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
    // Coarse summed-area index: locate actual raster corrections, not brush coordinates.
    std::vector<uint32_t> paintIndex;
    std::vector<uint8_t> paintMask;
    uint32_t paintStride=0,paintMinX=0,paintMinY=0,paintMaxX=0,paintMaxY=0;
    bool hasPainting() const { return !paintIndex.empty(); }
    bool paintAt(float u,float v) const
    {
        if (!hasPainting()) return false;
        const auto x=static_cast<uint32_t>(std::clamp(u,0.0f,1.0f)*(width-1));
        const auto y=static_cast<uint32_t>(std::clamp(v,0.0f,1.0f)*(height-1));
        for (uint32_t yy=y?y-1:0;yy<=std::min(y+1,height-1);++yy)
            for (uint32_t xx=x?x-1:0;xx<=std::min(x+1,width-1);++xx)
                if (paintMask[static_cast<size_t>(yy)*width+xx]) return true;
        return false;
    }
    bool paintTouches(float minX,float minY,float maxX,float maxY) const
    {
        if (!hasPainting()) return false;
        const uint32_t x0=static_cast<uint32_t>(std::clamp(minX,0.0f,1.0f)*(width-1))/8;
        const uint32_t y0=static_cast<uint32_t>(std::clamp(minY,0.0f,1.0f)*(height-1))/8;
        const uint32_t x1=static_cast<uint32_t>(std::clamp(maxX,0.0f,1.0f)*(width-1))/8+1;
        const uint32_t y1=static_cast<uint32_t>(std::clamp(maxY,0.0f,1.0f)*(height-1))/8+1;
        const auto at=[&](uint32_t x,uint32_t y) { return paintIndex[static_cast<size_t>(y)*paintStride+x]; };
        return at(x1,y1)+at(x0,y0)>at(x0,y1)+at(x1,y0);
    }
    static float channelValue(const stbi_uc *p, IMAGE_MESH_HEIGHT_CHANNEL channel)
    {
        switch (channel)
        {
            case IMAGE_MESH_HEIGHT_CHANNEL::LUMINANCE: return (0.2126f*p[0]+0.7152f*p[1]+0.0722f*p[2])/255.0f;
            case IMAGE_MESH_HEIGHT_CHANNEL::RED: return p[0]/255.0f;
            case IMAGE_MESH_HEIGHT_CHANNEL::GREEN: return p[1]/255.0f;
            case IMAGE_MESH_HEIGHT_CHANNEL::BLUE: return p[2]/255.0f;
            case IMAGE_MESH_HEIGHT_CHANNEL::ALPHA: return p[3]/255.0f;
        }
        return 0;
    }
    bool load(const char *source,const IMAGE_MESH_OPTIONS &o,std::string &error)
    {
        const auto fail=[&](const char *m) { error=m; return false; };
        if (!source || !*source) return fail("Image path required");
        if (o.heightSource<IMAGE_MESH_HEIGHT_SOURCE::IMAGE || o.heightSource>IMAGE_MESH_HEIGHT_SOURCE::MIXED ||
            !std::isfinite(o.baseHeight) || o.baseHeight<0 || o.baseHeight>1)
            return fail("Invalid heightSource or baseHeight [0,1]");
        if (!std::isfinite(o.grooveThreshold) || o.grooveThreshold<0 || o.grooveThreshold>1 ||
            !std::isfinite(o.grooveTransition) || o.grooveTransition<0.001f || o.grooveTransition>1 ||
            !std::isfinite(o.heightTolerance) || o.heightTolerance<0.001f || o.heightTolerance>1 || o.smoothPasses>4)
            return fail("Invalid groove threshold [0,1], transition/tolerance [0.001,1], or smoothing passes [0,4]");
        if (o.heightChannel<IMAGE_MESH_HEIGHT_CHANNEL::LUMINANCE || o.heightChannel>IMAGE_MESH_HEIGHT_CHANNEL::ALPHA)
            return fail("Invalid heightChannel");
        bool exists=false; const char *resolved=util::getFullPath(source,&exists);
        path=exists?resolved:source;
        int iw=0,ih=0,channels=0;
        if (!stbi_info(path.c_str(),&iw,&ih,&channels) || iw<=0 || ih<=0 || static_cast<uint64_t>(iw)*ih>16777216)
            return fail("Cannot inspect image or image exceeds 16 megapixels");
        imageWidth=static_cast<uint32_t>(iw); imageHeight=static_cast<uint32_t>(ih);
        if (o.x>=imageWidth || o.y>=imageHeight) return fail("Crop origin outside image");
        width=o.cropWidth?o.cropWidth:imageWidth-o.x; height=o.cropHeight?o.cropHeight:imageHeight-o.y;
        if (width>imageWidth-o.x || height>imageHeight-o.y) return fail("Crop outside image");
        checkpoint(o,"decode",0.02f);
        pixels.reset(stbi_load(path.c_str(),&iw,&ih,&channels,4));
        if (!pixels || iw!=static_cast<int>(imageWidth) || ih!=static_cast<int>(imageHeight))
            return fail("Cannot decode image or dimensions changed");
        std::unique_ptr<stbi_uc,decltype(&std::free)> heightPixels{nullptr,&std::free};
        int hw=0,hh=0;
        if (o.heightSource!=IMAGE_MESH_HEIGHT_SOURCE::MANUAL && o.heightImage && *o.heightImage)
        {
            const char *heightResolved=util::getFullPath(o.heightImage,&exists);
            const std::string heightPath=exists?heightResolved:o.heightImage;
            if (!stbi_info(heightPath.c_str(),&hw,&hh,&channels) || hw<=0 || hh<=0 ||
                static_cast<uint64_t>(hw)*hh>16777216)
                return fail("Height image is missing or invalid (maximum 16 million pixels)");
            const int expectedWidth=hw,expectedHeight=hh;
            checkpoint(o,"decode",0.04f);
            heightPixels.reset(stbi_load(heightPath.c_str(),&hw,&hh,&channels,4));
            if (!heightPixels || hw!=expectedWidth || hh!=expectedHeight)
                return fail("Cannot decode height image or dimensions changed");
        }
        checkpoint(o,"heights",0.08f);
        levels.resize(static_cast<size_t>(width)*height);
        for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
        {
            const auto *p=pixels.get()+(static_cast<size_t>(y+o.y)*imageWidth+x+o.x)*4;
            if (x==0) checkpoint(o,"heights",0.08f);
            float v=channelValue(p,o.heightChannel);
            if (heightPixels)
            {
                const double u=static_cast<double>(o.heightImageToRegion?x:x+o.x)/std::max(1u,(o.heightImageToRegion?width:imageWidth)-1);
                const double vCoord=static_cast<double>(o.heightImageToRegion?y:y+o.y)/std::max(1u,(o.heightImageToRegion?height:imageHeight)-1);
                const double px=u*(hw-1),py=vCoord*(hh-1);
                const int x0=static_cast<int>(px),y0=static_cast<int>(py);
                const int x1=std::min(x0+1,hw-1),y1=std::min(y0+1,hh-1);
                const float fx=static_cast<float>(px-x0),fy=static_cast<float>(py-y0);
                const auto sample=[&](int xx,int yy) {
                    return channelValue(heightPixels.get()+(static_cast<size_t>(yy)*hw+xx)*4,o.heightChannel);
                };
                const float top=sample(x0,y0)*(1-fx)+sample(x1,y0)*fx;
                const float bottom=sample(x0,y1)*(1-fx)+sample(x1,y1)*fx;
                v=top*(1-fy)+bottom*fy;
            }
            levels[static_cast<size_t>(y)*width+x]=o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL?o.baseHeight:(o.invert?1-v:v);
        }
        heightPixels.reset();
        std::vector<float> filtered;
        const uint32_t passes=o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL?0:o.smoothPasses;
        if (passes) filtered.resize(levels.size());
        for (uint32_t pass=0;pass<passes;++pass)
        {
            for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
            {
                if (x==0) checkpoint(o,"heights",0.08f+0.08f*(pass+static_cast<float>(y)/height)/passes);
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
        if (!o.heightEditCount && !o.heightAreaCount) return true;
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
        if (!composeHeightAreas(o,width,height,painted,error)) return false;
        uint64_t work=0;
        std::vector<float> patch;
        for (uint32_t i=0;i<o.heightEditCount;++i)
        {
            checkpoint(o,"painting",0.2f+0.04f*i/o.heightEditCount);
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
                if (x==x0) checkpoint(o,"painting",0.2f+0.04f*i/o.heightEditCount);
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
        // Locate corrections before turning this buffer into the final painted raster.
        bool changed=false;
        for (size_t i=0;i<painted.size();++i)
        {
            painted[i]-=mapped(levels[i],o);
            changed=changed || std::abs(painted[i])>1e-7f;
        }
        if (!changed) { painted.clear(); return true; }
        paintStride=(width+7)/8+1;
        const uint32_t tileRows=(height+7)/8;
        paintIndex.assign(static_cast<size_t>(paintStride)*(tileRows+1),0);
        paintMask.assign(levels.size(),0);
        paintMinX=width-1; paintMinY=height-1;
        for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
        {
            if (x==0) checkpoint(o,"painting",0.24f);
            if (std::abs(painted[static_cast<size_t>(y)*width+x])<=1e-7f) continue;
            // One-pixel mask dilation covers all cells touching a corrected sample.
            for (uint32_t yy=y?y-1:0;yy<=std::min(y+1,height-1);++yy)
                for (uint32_t xx=x?x-1:0;xx<=std::min(x+1,width-1);++xx)
                    paintMask[static_cast<size_t>(yy)*width+xx]=1;
            const uint32_t x0=x>3?x-3:0,y0=y>3?y-3:0;
            const uint32_t x1=std::min(x+3,width-1),y1=std::min(y+3,height-1);
            paintMinX=std::min(paintMinX,x0); paintMinY=std::min(paintMinY,y0);
            paintMaxX=std::max(paintMaxX,x1); paintMaxY=std::max(paintMaxY,y1);
            for (uint32_t ty=y0/8;ty<=y1/8;++ty) for (uint32_t tx=x0/8;tx<=x1/8;++tx)
                paintIndex[static_cast<size_t>(ty+1)*paintStride+tx+1]=1;
        }
        for (size_t i=0;i<painted.size();++i) painted[i]+=mapped(levels[i],o);
        for (uint32_t y=1;y<=tileRows;++y) for (uint32_t x=1;x<paintStride;++x)
        {
            const size_t i=static_cast<size_t>(y)*paintStride+x;
            paintIndex[i]+=paintIndex[i-1]+paintIndex[i-paintStride]-paintIndex[i-paintStride-1];
        }
        return true;
    }
    template<class T>
    float interpolate(float u,float v,const std::vector<T> &values) const
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
    {
        const float automatic=mapped(sample(u,v),o);
        if (!hasPainting()) return automatic;
        const float weight=interpolate(u,v,paintMask);
        if (weight==0) return automatic;
        // Interpolate final heights, not brush deltas on top of a nonlinear remap:
        // neighboring pixels painted to 1 must remain a flat plateau between pixels.
        const float value=std::clamp(automatic+(interpolate(u,v,painted)-automatic)*weight,0.0f,1.0f);
        // Use the same plateau tolerance as normal classification; tiny near-flat
        // height differences on narrow refined faces otherwise tilt their normals.
        if (o.heightSource!=IMAGE_MESH_HEIGHT_SOURCE::MANUAL && o.twoLevels && value>=0.9999f) return 1;
        if (o.heightSource!=IMAGE_MESH_HEIGHT_SOURCE::MANUAL && o.twoLevels && value<=0.0001f) return 0;
        return value;
    }
    float transition(float u,float v,const IMAGE_MESH_OPTIONS &o) const
    {
        if (o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL) return surface(u,v,o);
        const float raw=sample(u,v);
        if (!hasPainting()) return raw;
        const float level=surface(u,v,o);
        if (std::abs(level-mapped(raw,o))<=1e-7f) return raw;
        if (!o.twoLevels) return level;
        // Express corrected height in the original threshold domain. Keep untouched raw
        // values EXACT: using near-0/1 height isocurves everywhere would lose plateau normals.
        if (level<=0) return std::min(raw,o.grooveThreshold-o.grooveTransition*0.5f-0.0001f);
        if (level>=1) return std::max(raw,o.grooveThreshold+o.grooveTransition*0.5f+0.0001f);
        float lo=0,hi=1;
        for (unsigned i=0;i<20;++i)
        {
            const float t=(lo+hi)*0.5f;
            if (t*t*(3-2*t)<level) lo=t; else hi=t;
        }
        return o.grooveThreshold+((lo+hi)*0.5f-0.5f)*o.grooveTransition;
    }
    float mapped(float value,const IMAGE_MESH_OPTIONS &o) const
    {
        if (o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL) return o.baseHeight;
        if (!o.twoLevels) return value;
        const float t=std::clamp((value-o.grooveThreshold)/o.grooveTransition+0.5f,0.0f,1.0f);
        return t*t*(3-2*t);
    }
};
} }
#endif
