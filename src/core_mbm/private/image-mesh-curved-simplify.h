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

#ifndef IMAGE_MESH_CURVED_SIMPLIFY_H
#define IMAGE_MESH_CURVED_SIMPLIFY_H
#include "image-mesh-height.h"
#include "image-mesh-topology.h"
#include "image-mesh-progress.h"
#include "mesh-planar.h"
#include <limits>

namespace mbm { namespace image_mesh {
// Remove only interior vertices. Each replacement inherits a conservative error
// bound against the original piecewise-linear surface, so passes cannot hide drift.
namespace curved_simplify {
using FACE = std::array<uint32_t,3>;
struct POINT { double x,y; };
inline double cross(POINT a,POINT b,POINT c)
{ return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
inline POINT point(const IMAGE_MESH_POINT &p) { return {p.x,p.y}; }
inline bool weights(POINT p,const std::array<POINT,3> &t,double (&w)[3])
{
    const double d=cross(t[0],t[1],t[2]);
    if (std::abs(d)<1e-14) return false;
    w[0]=cross(p,t[1],t[2])/d;w[1]=cross(t[0],p,t[2])/d;w[2]=1-w[0]-w[1];
    return w[0]>=-1e-9 && w[1]>=-1e-9 && w[2]>=-1e-9;
}
// The difference of two linear triangles attains its extrema at vertices of
// their intersection: contained corners and segment intersections suffice.
inline double difference(const FACE &a,const FACE &b,const TOPOLOGY &t,const std::vector<double> &h)
{
    std::array<POINT,3> pa,pb;
    for (unsigned k=0;k<3;++k) { pa[k]=point(t.points[a[k]]);pb[k]=point(t.points[b[k]]); }
    double maximum=-1;
    const auto sample=[&](POINT p) {
        double wa[3],wb[3];
        if (!weights(p,pa,wa) || !weights(p,pb,wb)) return;
        double delta=0;for (unsigned k=0;k<3;++k) delta+=wa[k]*h[a[k]]-wb[k]*h[b[k]];
        maximum=std::max(maximum,std::abs(delta));
    };
    for (unsigned i=0;i<3;++i)
    {
        sample(pa[i]);sample(pb[i]);
        const POINT p=pa[i],q=pa[(i+1)%3];
        for (unsigned j=0;j<3;++j)
        {
            const POINT r=pb[j],s=pb[(j+1)%3];
            const double d=(q.x-p.x)*(s.y-r.y)-(q.y-p.y)*(s.x-r.x);
            if (std::abs(d)<1e-18) continue;
            const double u=((r.x-p.x)*(s.y-r.y)-(r.y-p.y)*(s.x-r.x))/d;
            const double v=((r.x-p.x)*(q.y-p.y)-(r.y-p.y)*(q.x-p.x))/d;
            if (u>=0 && u<=1 && v>=0 && v<=1) sample({p.x+u*(q.x-p.x),p.y+u*(q.y-p.y)});
        }
    }
    return maximum;
}
inline bool onSegment(POINT p,POINT a,POINT b)
{
    const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
    const double u=length>0?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.0,1.0):0;
    return std::hypot(p.x-a.x-u*dx,p.y-a.y-u*dy)<=4e-7;
}
inline bool protectedPoint(uint32_t index,const TOPOLOGY &t,const IMAGE_MESH_OPTIONS &o)
{
    const POINT p=point(t.points[index]);
    if (!o.curvedHierarchy)
    {
        if (std::hypot(p.x-o.curvedX,p.y-o.curvedY)<=4e-7) return true;
        const double r=std::hypot((p.x-o.curvedX)*o.width,(p.y-o.curvedY)*o.height);
        return o.curvedRadius>0 && std::abs(r-o.curvedRadius)<=1e-6*std::max(o.width,o.height);
    }
    for (uint32_t n=0;n<o.curvedNodeCount;++n)
    {
        const auto &node=o.curvedNodes[n];
        const uint32_t segments=node.count>2?node.count:1;
        for (uint32_t j=0;j<segments;++j)
            if (onSegment(p,point(node.points[j]),point(node.points[(j+1)%node.count]))) return true;
    }
    return false;
}
inline void run(const IMAGE_MESH_OPTIONS &o,const HEIGHT_FIELD &field,TOPOLOGY &t,IMAGE_MESH_REPORT &report)
{
    if (o.heightSource!=IMAGE_MESH_HEIGHT_SOURCE::CURVED || !o.curvedSimplify) return;
    report.curvedSourceTriangles=static_cast<uint32_t>(t.triangles.size());
    const size_t target=static_cast<size_t>(std::ceil(t.triangles.size()*o.curvedSimplifyRatio));
    std::vector<double> heights(t.points.size()),errors(t.triangles.size(),0);
    std::vector<bool> locked(t.points.size(),false);
    for (auto i:t.boundary) locked[i]=true;
    for (uint32_t i=0;i<t.points.size();++i)
    {
        checkpoint(o,"curved_simplify",0.87f);
        heights[i]=field.surface(t.points[i].x,t.points[i].y,o);
        locked[i]=locked[i] || protectedPoint(i,t,o);
    }
    // Retain sampled peaks/valleys as well as authored controls. Flat interiors
    // are removable; the exact geometry of plateau borders remains constrained.
    std::vector<bool> lower(t.points.size(),false),higher=lower;
    for (const auto &f:t.triangles) for (unsigned j=0;j<3;++j) for (unsigned k=0;k<3;++k)
    {
        lower[f[j]]=lower[f[j]] || heights[f[k]]<heights[f[j]]-1e-7;
        higher[f[j]]=higher[f[j]] || heights[f[k]]>heights[f[j]]+1e-7;
    }
    for (size_t i=0;i<locked.size();++i) locked[i]=locked[i] || (lower[i]!=higher[i]);
    if (o.curvedSimplifyMode != 0)
    {
        mesh_simplifier::INPUT input, result;
        mesh_simplifier::planar::ATTRIBUTES attributes;
        attributes.exact = true;
        attributes.locked = locked;
        for (size_t i=0; i<t.points.size(); ++i)
        {
            const auto &p=t.points[i];
            const float height=static_cast<float>(heights[i])*o.relief;
            input.positions.emplace_back((p.x-0.5f)*o.width,(0.5f-p.y)*o.height,-o.depth*0.5f-height);
        }
        // Only exact horizontal plateaus with the generator's constant-normal
        // policy are eligible. Sloped/curved faces stay separate, preserving
        // height controls and the specific reducer's immutable error baseline.
        for (uint32_t f=0; f<t.triangles.size(); ++f)
        {
            const auto &face=t.triangles[f];
            const auto level=heights[face[0]];
            const bool plateau=level==heights[face[1]] && level==heights[face[2]] &&
                (o.relief==0 || level>=0.9999 || level<=0.0001 || o.curvedHierarchy);
            input.triangleGroups.push_back(plateau?0:f+1);
            for (auto v:face)
            {
                input.indices.push_back(v);
                attributes.uv.emplace_back(t.points[v].x,t.points[v].y);
                if (!plateau) attributes.locked[v]=true;
            }
        }
        mesh_simplifier::planar::REPORT planarReport;
        std::string error;
        if (!mesh_simplifier::planar::run(input,attributes,result,planarReport,error,
            [&](float progress) { checkpoint(o,"curved_simplify",0.87f+0.005f*progress); }))
            throw std::runtime_error(error);
        if (planarReport.removed)
        {
            t.triangles.clear();
            for (size_t i=0;i<result.indices.size();i+=3)
                t.triangles.push_back({result.indices[i],result.indices[i+1],result.indices[i+2]});
            errors.assign(t.triangles.size(),0);
        }
        report.curvedPlanarRemovedTriangles=planarReport.removed;
    }
    for (unsigned pass=0;o.curvedSimplifyMode!=2 && pass<32 && t.triangles.size()>target;++pass)
    {
        std::vector<std::vector<uint32_t>> incident(t.points.size());
        for (uint32_t i=0;i<t.triangles.size();++i) for (auto v:t.triangles[i]) incident[v].push_back(i);
        std::vector<bool> touched(t.points.size(),false),dead(t.triangles.size(),false);
        std::vector<FACE> added;std::vector<double> addedErrors;
        size_t remaining=t.triangles.size();
        for (uint32_t v=0;v<t.points.size() && remaining>target;++v)
        {
            if ((v&63)==0) checkpoint(o,"curved_simplify",(o.curvedSimplifyMode?0.875f:0.87f)+0.015f*pass/32);
            const auto &fan=incident[v];
            if (locked[v] || touched[v] || fan.size()<3 || fan.size()>32 || remaining<target+2) continue;
            std::vector<std::array<uint32_t,2>> edges;
            bool valid=true;
            for (auto f:fan)
            {
                const auto &face=t.triangles[f];
                for (unsigned k=0;k<3;++k) if (face[k]==v)
                {
                    const auto a=face[(k+1)%3],b=face[(k+2)%3];
                    if (touched[a] || touched[b]) valid=false;
                    edges.push_back({a,b});
                }
            }
            if (!valid) continue;
            std::vector<uint32_t> ring{edges[0][0]};
            for (size_t k=0;k<edges.size();++k)
            {
                const auto it=std::find_if(edges.begin(),edges.end(),[&](const auto &e){return e[0]==ring.back();});
                if (it==edges.end()) { valid=false;break; }
                const auto next=(*it)[1];
                if (k+1==edges.size()) { valid=next==ring.front();break; }
                if (std::find(ring.begin(),ring.end(),next)!=ring.end()) { valid=false;break; }
                ring.push_back(next);
            }
            if (!valid) continue;
            std::vector<IMAGE_MESH_POINT> polygon;for (auto i:ring) polygon.push_back(t.points[i]);
            std::vector<FACE> replacement;
            if (!triangulatePolygon(polygon,replacement) || replacement.size()+2!=fan.size()) continue;
            std::vector<double> bounds;
            for (auto &face:replacement)
            {
                for (auto &i:face) i=ring[i];
                if (cross(point(t.points[face[0]]),point(t.points[face[1]]),point(t.points[face[2]]))<=1e-14)
                { valid=false;break; }
                // Generation stores world coordinates as floats. A valid UV
                // triangle can become collinear after that conversion.
                std::array<POINT,3> world;
                for (unsigned k=0;k<3;++k)
                {
                    const auto &p=t.points[face[k]];
                    world[k]={static_cast<float>((p.x-0.5f)*o.width),static_cast<float>((p.y-0.5f)*o.height)};
                }
                if (cross(world[0],world[1],world[2])<=0) { valid=false;break; }
                double bound=0;
                for (auto old:fan)
                {
                    const double delta=difference(face,t.triangles[old],t,heights);
                    if (delta>=0) bound=std::max(bound,errors[old]+delta+1e-12);
                }
                if (bound>o.curvedSimplifyError) { valid=false;break; }
                bounds.push_back(bound);
            }
            if (!valid) continue;
            touched[v]=true;for (auto i:ring) touched[i]=true;
            for (auto f:fan) dead[f]=true;
            added.insert(added.end(),replacement.begin(),replacement.end());
            addedErrors.insert(addedErrors.end(),bounds.begin(),bounds.end());
            remaining-=2;
        }
        if (added.empty()) break;
        for (size_t i=0;i<t.triangles.size();++i) if (!dead[i])
        { added.push_back(t.triangles[i]);addedErrors.push_back(errors[i]); }
        t.triangles.swap(added);errors.swap(addedErrors);
    }
    std::vector<uint32_t> remap(t.points.size(),std::numeric_limits<uint32_t>::max());
    std::vector<IMAGE_MESH_POINT> compact;
    for (auto &f:t.triangles) for (auto &v:f)
    {
        if (remap[v]==std::numeric_limits<uint32_t>::max())
        { remap[v]=static_cast<uint32_t>(compact.size());compact.push_back(t.points[v]); }
        v=remap[v];
    }
    for (auto &v:t.boundary) v=remap[v];
    t.points.swap(compact);
    report.curvedResultTriangles=static_cast<uint32_t>(t.triangles.size());
    report.curvedTargetReached=o.curvedSimplifyMode==2 || t.triangles.size()<=target;
    report.curvedMaximumError=static_cast<float>(*std::max_element(errors.begin(),errors.end()));
}
} // namespace curved_simplify
} }
#endif
