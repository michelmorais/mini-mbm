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

#ifndef IMAGE_MESH_CURVED_H
#define IMAGE_MESH_CURVED_H
#include "image-mesh-topology.h"
#include "image-mesh-progress.h"
#include "image-mesh-curved-hierarchy.h"
#include <cmath>
#include <limits>
namespace mbm { namespace image_mesh {
inline IMAGE_MESH_OPTIONS curvedOptions(IMAGE_MESH_OPTIONS o)
{
    if (o.heightSource!=IMAGE_MESH_HEIGHT_SOURCE::CURVED) return o;
    float low=std::min(o.curvedEdge,o.curvedTarget),high=std::max(o.curvedEdge,o.curvedTarget);
    if (o.curvedHierarchy)
    {
        low=high=o.curvedEdge;
        if (o.curvedNodes && o.curvedNodeCount<=32) for (uint32_t i=0;i<o.curvedNodeCount;++i)
        {
            const auto &node=o.curvedNodes[i];
            if (!node.inherited) { low=std::min(low,node.thickness);high=std::max(high,node.thickness); }
        }
    }
    o.depth=low;o.relief=(high-low)/(o.curvedSymmetric?2.0f:1.0f);
    o.backRelief=o.curvedSymmetric;
    o.backOpen=false; o.backRemap=false; o.backSolid=false; o.backExternal=false;
    o.lockBorder=false; o.followImage=true; o.twoLevels=false; o.smoothPasses=0;
    return o;
}
struct CURVED_FIELD
{
    std::vector<IMAGE_MESH_POINT> contour;
    std::vector<std::vector<IMAGE_MESH_POINT>> holes;
    CURVED_HIERARCHY hierarchy;
    bool prepare(const IMAGE_MESH_OPTIONS &o,std::string &error)
    {
        const auto fail=[&](const char *message) { error=message; return false; };
        const auto dimension=[](float v) { return std::isfinite(v) && v>=0.001f && v<=1000000; };
        if (!dimension(o.width) || !dimension(o.height) || !dimension(o.curvedEdge) || !dimension(o.curvedTarget) ||
            !std::isfinite(o.curvedRadius) || o.curvedRadius<0 || o.curvedRadius>1000000 ||
            !std::isfinite(o.curvedX) || !std::isfinite(o.curvedY) || o.curvedX<0 || o.curvedX>1 || o.curvedY<0 || o.curvedY>1)
            return fail("Manual curved: invalid center, radius or thickness (minimum 0.001)");
        if (o.backOpen || o.backRemap || o.backSolid || o.backExternal)
            return fail("Manual curved: use a closed source-textured back");
        // Reuse the existing contour validation, with no field/refinement or curved recursion.
        IMAGE_MESH_OPTIONS outline=o;
        outline.heightSource=IMAGE_MESH_HEIGHT_SOURCE::MANUAL;
        outline.columns=outline.rows=1; outline.maxVertices=65535; outline.maxTriangles=131070;
        TOPOLOGY t;
        if (!buildTopology(outline,t,error)) return false;
        contour=std::move(t.contour);holes=std::move(t.holes);
        if (o.curvedHierarchy) return hierarchy.prepare(o,contour,error);
        const double tolerance=std::max(o.width,o.height)*1e-6;
        for (size_t i=0;i<contour.size();++i)
        {
            const auto &a=contour[i], &b=contour[(i+1)%contour.size()];
            const double dx=(static_cast<double>(b.x)-a.x)*o.width,dy=(static_cast<double>(b.y)-a.y)*o.height;
            const double cx=(static_cast<double>(o.curvedX)-a.x)*o.width,cy=(static_cast<double>(o.curvedY)-a.y)*o.height;
            const double distance=(dx*cy-dy*cx)/std::hypot(dx,dy);
            // Positive half-planes define the visibility kernel of a CCW simple polygon.
            if (distance<=tolerance) return fail("Manual curved: center cannot see the whole contour; move the center inward");
            // Distance to the segment (not its infinite line) permits circles beyond the kernel.
            const double along=std::clamp((cx*dx+cy*dy)/(dx*dx+dy*dy),0.0,1.0);
            if (std::hypot(cx-along*dx,cy-along*dy)<=o.curvedRadius+tolerance)
                return fail("Manual curved: target circle touches or crosses the contour; reduce its radius");
        }
        return true;
    }
    float level(float u,float v,const IMAGE_MESH_OPTIONS &o) const
    {
        if (o.curvedHierarchy) return hierarchy.level(u,v,o);
        const double dx=(static_cast<double>(u)-o.curvedX)*o.width,dy=(static_cast<double>(v)-o.curvedY)*o.height;
        const double radius=std::hypot(dx,dy);
        double t=1;
        if (radius>o.curvedRadius && radius>1e-12)
        {
            double hit=std::numeric_limits<double>::infinity();
            for (size_t i=0;i<contour.size();++i)
            {
                const auto &a=contour[i], &b=contour[(i+1)%contour.size()];
                const double ax=(static_cast<double>(a.x)-o.curvedX)*o.width,ay=(static_cast<double>(a.y)-o.curvedY)*o.height;
                const double ex=(static_cast<double>(b.x)-a.x)*o.width,ey=(static_cast<double>(b.y)-a.y)*o.height;
                const double det=dx*ey-dy*ex;
                if (std::abs(det)<1e-20) continue;
                const double ray=(ax*ey-ay*ex)/det,edge=(ax*dy-ay*dx)/det;
                if (ray>0 && edge>=-1e-7 && edge<=1.0000001) hit=std::min(hit,ray*radius);
            }
            t=std::isfinite(hit)?std::clamp((hit-radius)/(hit-o.curvedRadius),0.0,1.0):0;
        }
        if (o.curvedTarget==o.curvedEdge) return 0;
        return static_cast<float>(o.curvedTarget>o.curvedEdge?t:1-t);
    }
};
} }
#endif
