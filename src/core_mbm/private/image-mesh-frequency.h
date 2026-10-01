/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#ifndef IMAGE_MESH_FREQUENCY_H
#define IMAGE_MESH_FREQUENCY_H
#include "image-mesh-height.h"
namespace mbm { namespace image_mesh {
inline bool filterGeometryHeights(HEIGHT_FIELD &field,const IMAGE_MESH_OPTIONS &o,std::string &error)
{
    if (o.geometryBlurRadius==0) return true;
    if (o.geometryBlurRadius>32 || o.voxelized || o.twoLevels || o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::CURVED)
    { error="Geometry frequency separation requires continuous image/manual/mixed height and radius [1,32]";return false; }
    IMAGE_MESH_OPTIONS outline=o;
    outline.followImage=false;outline.columns=outline.rows=1;
    outline.heightEditCount=outline.heightAreaCount=0;
    outline.maxVertices=65535;outline.maxTriangles=131070;
    TOPOLOGY topology;
    if (!buildTopology(outline,topology,error)) return false;
    const auto inside=[](const IMAGE_MESH_POINT &p,const std::vector<IMAGE_MESH_POINT> &loop)
    {
        bool result=false;
        for (size_t i=0,j=loop.size()-1;i<loop.size();j=i++)
        {
            const auto &a=loop[i], &b=loop[j];
            if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) result=!result;
        }
        return result;
    };
    const uint32_t width=field.width,height=field.height;
    const int radius=static_cast<int>(o.geometryBlurRadius);
    const auto at=[](int value,uint32_t size) { return static_cast<uint32_t>(std::clamp(value,0,static_cast<int>(size)-1)); };
    const size_t count=static_cast<size_t>(width)*height;
    std::vector<float> sums(count),weights(count),result(count),row(width),alpha(width);
    // Alpha/mask-weighted separable box filter. Never quantize through an image file.
    for (uint32_t y=0;y<height;++y)
    {
        checkpoint(o,"heights",0.25f+0.02f*static_cast<float>(y)/height);
        for (uint32_t x=0;x<width;++x)
        {
            if (x%256==0) checkpoint(o,"heights",0.25f+0.02f*static_cast<float>(y)/height);
            const IMAGE_MESH_POINT p{static_cast<float>(x)/std::max(1u,width-1),static_cast<float>(y)/std::max(1u,height-1)};
            float weight=field.pixels.get()[(static_cast<size_t>(y+o.y)*field.imageWidth+x+o.x)*4+3]/255.0f;
            if (weight>0)
            {
                bool covered=inside(p,topology.contour);
                for (const auto &hole:topology.holes) if (inside(p,hole)) { covered=false;break; }
                if (!covered && borderDistance(p,topology)>1e-6f) weight=0;
            }
            alpha[x]=weight;row[x]=field.surface(p.x,p.y,o)*weight;
        }
        double sum=0,weight=0;
        for (int x=-radius;x<=radius;++x) { const auto i=at(x,width);sum+=row[i];weight+=alpha[i]; }
        for (uint32_t x=0;x<width;++x)
        {
            const size_t i=static_cast<size_t>(y)*width+x;
            sums[i]=static_cast<float>(sum);weights[i]=static_cast<float>(weight);
            const auto left=at(static_cast<int>(x)-radius,width),right=at(static_cast<int>(x)+radius+1,width);
            sum+=row[right]-row[left];weight+=alpha[right]-alpha[left];
        }
    }
    std::vector<double> totals(width,0),counts(width,0);
    const auto accumulate=[&](int y,int sign)
    {
        const size_t offset=static_cast<size_t>(at(y,height))*width;
        for (uint32_t x=0;x<width;++x) { totals[x]+=sign*sums[offset+x];counts[x]+=sign*weights[offset+x]; }
    };
    for (int y=-radius;y<=radius;++y) accumulate(y,1);
    for (uint32_t y=0;y<height;++y)
    {
        checkpoint(o,"heights",0.27f+0.02f*static_cast<float>(y)/height);
        for (uint32_t x=0;x<width;++x)
            result[static_cast<size_t>(y)*width+x]=counts[x]>1e-8 ?
                static_cast<float>(std::clamp(totals[x]/counts[x],0.0,1.0)) :
                field.surface(static_cast<float>(x)/std::max(1u,width-1),static_cast<float>(y)/std::max(1u,height-1),o);
        accumulate(static_cast<int>(y)-radius,-1);accumulate(static_cast<int>(y)+radius+1,1);
    }
    field.geometryLevels.swap(result);
    return true;
}
} }
#endif
