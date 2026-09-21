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

#ifndef IMAGE_MESH_HEIGHT_AREAS_H
#define IMAGE_MESH_HEIGHT_AREAS_H
#include <core_mbm/image-mesh.h>
#include "image-mesh-progress.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace mbm { namespace image_mesh {
inline bool composeHeightAreas(const IMAGE_MESH_OPTIONS &o,uint32_t width,uint32_t height,
                               std::vector<float> &values,std::string &error)
{
    const auto fail=[&](const char *message) { error=message;return false; };
    if (o.heightAreaCount>32 || (o.heightAreaCount && !o.heightAreas))
        return fail("heightAreas accepts at most 32 contours");
    const auto cross=[](const IMAGE_MESH_POINT &a,const IMAGE_MESH_POINT &b,const IMAGE_MESH_POINT &c) {
        return (static_cast<double>(b.x)-a.x)*(static_cast<double>(c.y)-a.y)-(static_cast<double>(b.y)-a.y)*(static_cast<double>(c.x)-a.x);
    };
    const auto on=[&](const IMAGE_MESH_POINT &a,const IMAGE_MESH_POINT &b,const IMAGE_MESH_POINT &p) {
        return std::abs(cross(a,b,p))<=1e-10 && p.x>=std::min(a.x,b.x)-1e-10 && p.x<=std::max(a.x,b.x)+1e-10 &&
            p.y>=std::min(a.y,b.y)-1e-10 && p.y<=std::max(a.y,b.y)+1e-10;
    };
    uint64_t work=0;
    for (uint32_t index=0;index<o.heightAreaCount;++index)
    {
        const auto &area=o.heightAreas[index];
        if (!area.points || area.count<3 || area.count>128 || !std::isfinite(area.height) || area.height<0 || area.height>1 ||
            !std::isfinite(area.transition) || area.transition<0 || area.transition>1)
            return fail("Invalid height area: 3..128 points, height and transition in [0,1]");
        float minX=1,minY=1,maxX=0,maxY=0;
        for (uint32_t i=0;i<area.count;++i)
        {
            const auto &p=area.points[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.x>1 || p.y<0 || p.y>1)
                return fail("Height area points must be normalized crop coordinates in [0,1]");
            minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
        }
        double signedArea=0;
        for (uint32_t i=0;i<area.count;++i)
        {
            const auto &a=area.points[i],&b=area.points[(i+1)%area.count],&c=area.points[(i+2)%area.count];
            if (std::hypot(a.x-b.x,a.y-b.y)<1e-6 || (std::abs(cross(a,b,c))<=1e-10 && !on(a,c,b)))
                return fail("Height area has repeated points or backtracking edges");
            for (uint32_t j=i+1;j<area.count;++j)
            {
                if (j==(i+1)%area.count || (j+1)%area.count==i) continue;
                const auto &p=area.points[j],&q=area.points[(j+1)%area.count];
                if ((cross(a,b,p)*cross(a,b,q)<0 && cross(p,q,a)*cross(p,q,b)<0) ||
                    on(a,b,p) || on(a,b,q) || on(p,q,a) || on(p,q,b))
                    return fail("Height area crosses or touches itself");
            }
            signedArea+=static_cast<double>(a.x)*b.y-static_cast<double>(a.y)*b.x;
        }
        if (std::abs(signedArea)<1e-8) return fail("Height area has negligible area");
        if (!area.enabled || o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::IMAGE) continue;
        const uint32_t x0=static_cast<uint32_t>(std::floor(minX*(width-1))),x1=static_cast<uint32_t>(std::ceil(maxX*(width-1)));
        const uint32_t y0=static_cast<uint32_t>(std::floor(minY*(height-1))),y1=static_cast<uint32_t>(std::ceil(maxY*(height-1)));
        work+=static_cast<uint64_t>(x1-x0+1)*(y1-y0+1)*area.count;
        if (work>64000000) return fail("Height areas exceed 64 million edge evaluations; reduce area size or contour points");
        const float transition=area.transition*std::max(1u,std::min(width,height)-1);
        for (uint32_t y=y0;y<=y1;++y) for (uint32_t x=x0;x<=x1;++x)
        {
            if (x==x0) checkpoint(o,"areas",0.16f+0.04f*(index+static_cast<float>(y-y0)/(y1-y0+1))/std::max(1u,o.heightAreaCount));
            bool inside=false;double nearest=1e30;
            for (uint32_t i=0;i<area.count;++i)
            {
                const auto &a=area.points[i],&b=area.points[(i+1)%area.count];
                const double ax=a.x*(width-1),ay=a.y*(height-1),bx=b.x*(width-1),by=b.y*(height-1);
                if ((ay>y)!=(by>y) && x<(bx-ax)*(y-ay)/(by-ay)+ax) inside=!inside;
                const double dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
                const double t=length>0?std::clamp(((x-ax)*dx+(y-ay)*dy)/length,0.0,1.0):0;
                nearest=std::min(nearest,std::hypot(x-ax-t*dx,y-ay-t*dy));
            }
            if (!inside && nearest>1e-5) continue;
            const float weight=transition>0?std::min(1.0f,static_cast<float>(nearest)/transition):1;
            auto &value=values[static_cast<size_t>(y)*width+x];
            value+=(area.height-value)*weight;
        }
    }
    return true;
}
} }
#endif
