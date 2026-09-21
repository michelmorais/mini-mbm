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

#ifndef IMAGE_MESH_SIDES_H
#define IMAGE_MESH_SIDES_H
#include "image-mesh-topology.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace mbm { namespace image_mesh {
struct SIDE_POINT { double x=0,y=0; };
inline double sideCross(const SIDE_POINT &a,const SIDE_POINT &b,const SIDE_POINT &c)
{ return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
inline bool sideHit(const SIDE_POINT &a,const SIDE_POINT &b,const SIDE_POINT &c,const SIDE_POINT &d)
{
    const double a1=sideCross(a,b,c),a2=sideCross(a,b,d),b1=sideCross(c,d,a),b2=sideCross(c,d,b);
    if (((a1>0 && a2<0)||(a1<0 && a2>0)) && ((b1>0 && b2<0)||(b1<0 && b2>0))) return true;
    const auto on=[](const SIDE_POINT &p,const SIDE_POINT &q,const SIDE_POINT &r,double cross) {
        return std::abs(cross)<1e-8 && r.x>=std::min(p.x,q.x)-1e-8 && r.x<=std::max(p.x,q.x)+1e-8 &&
            r.y>=std::min(p.y,q.y)-1e-8 && r.y<=std::max(p.y,q.y)+1e-8;
    };
    return on(a,b,c,a1)||on(a,b,d,a2)||on(c,d,a,b1)||on(c,d,b,b2);
}
inline bool sideInside(const std::vector<SIDE_POINT> &p,const SIDE_POINT &q)
{
    bool in=false;
    for (size_t i=0,j=p.size()-1;i<p.size();j=i++)
        if ((p[i].y>q.y)!=(p[j].y>q.y) && q.x<(p[j].x-p[i].x)*(q.y-p[i].y)/(p[j].y-p[i].y)+p[i].x) in=!in;
    return in;
}
// Reject topology changes: one simple inner contour, no crossing UV strips.
inline bool sideOffset(const IMAGE_MESH_OPTIONS &o,const std::vector<SIDE_POINT> &outer,
                       double inset,std::vector<SIDE_POINT> &inner)
{
    const size_t n=outer.size(); inner.resize(n);
    const double w=o.cropWidth-1.0,h=o.cropHeight-1.0;
    if (o.shape==IMAGE_MESH_SHAPE::ELLIPSE)
    {
        if (inset>=std::min(w,h)*0.5) return false;
        for (size_t i=0;i<n;++i)
            inner[i]={w*.5+(outer[i].x-w*.5)*(1-2*inset/w),h*.5+(outer[i].y-h*.5)*(1-2*inset/h)};
    }
    else for (size_t i=0;i<n;++i)
    {
        const auto &a=outer[(i+n-1)%n],&b=outer[i],&c=outer[(i+1)%n];
        const double ax=b.x-a.x,ay=b.y-a.y,bx=c.x-b.x,by=c.y-b.y;
        const double la=std::hypot(ax,ay),lb=std::hypot(bx,by),den=ax*by-ay*bx;
        if (la<1e-8 || lb<1e-8 || std::abs(den)<1e-10) return false;
        const SIDE_POINT p={b.x-inset*ay/la,b.y+inset*ax/la};
        const SIDE_POINT q={b.x-inset*by/lb,b.y+inset*bx/lb};
        const double t=((q.x-p.x)*by-(q.y-p.y)*bx)/den;
        inner[i]={p.x+t*ax,p.y+t*ay};
    }
    double area=0;
    for (size_t i=0;i<n;++i)
    {
        const size_t j=(i+1)%n;
        if (!sideInside(outer,inner[i])) return false;
        if ((inner[j].x-inner[i].x)*(outer[j].x-outer[i].x)+
            (inner[j].y-inner[i].y)*(outer[j].y-outer[i].y)<=1e-8) return false;
        area+=inner[i].x*inner[j].y-inner[j].x*inner[i].y;
        for (size_t k=0;k<n;++k)
        {
            const size_t l=(k+1)%n;
            if (sideHit(inner[i],inner[j],outer[k],outer[l])) return false;
            if (k!=i && k!=j && l!=i && sideHit(inner[i],inner[j],inner[k],inner[l])) return false;
            if (k!=i && sideHit(outer[i],inner[i],outer[k],inner[k])) return false;
            if (k!=i && l!=i && sideHit(outer[i],inner[i],inner[k],inner[l])) return false;
        }
    }
    return area>1e-6;
}
inline bool sideContour(const IMAGE_MESH_OPTIONS &o,const std::vector<IMAGE_MESH_POINT> &contour,
                        std::vector<IMAGE_MESH_POINT> &inner,float &maximum,std::string &error)
{
    maximum=0;
    if (o.cropWidth<2 || o.cropHeight<2 || contour.size()<3) { error="Side band needs a crop of at least 2 x 2 pixels"; return false; }
    std::vector<SIDE_POINT> outer,trial;
    for (const auto &p:contour) outer.push_back({p.x*(o.cropWidth-1.0),p.y*(o.cropHeight-1.0)});
    double lo=0,hi=std::min(o.cropWidth-1.0,o.cropHeight-1.0)*.5;
    for (int i=0;i<26;++i)
    {
        const double mid=(lo+hi)*.5;
        if (sideOffset(o,outer,mid,trial)) lo=mid; else hi=mid;
    }
    maximum=static_cast<float>(std::max(0.0,lo-1e-4));
    if (!std::isfinite(o.sideInset) || o.sideInset<1 || o.sideInset>maximum ||
        !sideOffset(o,outer,o.sideInset,trial))
    { error="Side band width must be at least 1 pixel and no more than "+std::to_string(maximum)+" pixels for this contour"; return false; }
    inner.clear();
    for (const auto &p:trial) inner.push_back({static_cast<float>(p.x/(o.cropWidth-1)),static_cast<float>(p.y/(o.cropHeight-1))});
    return true;
}
inline void sideSplitEdge(std::vector<std::array<uint32_t,3>> &triangles,uint32_t a,uint32_t b,uint32_t mid)
{
    for (size_t i=0;i<triangles.size();++i)
        for (int e=0;e<3;++e)
        {
            const auto t=triangles[i];
            if ((t[e]==a && t[(e+1)%3]==b)||(t[e]==b && t[(e+1)%3]==a))
            {
                triangles[i]={t[e],mid,t[(e+2)%3]};
                triangles.push_back({mid,t[(e+1)%3],t[(e+2)%3]}); return;
            }
        }
}
inline void sideSplitRepeats(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t)
{
    const auto length=[&](uint32_t a,uint32_t b) {
        return std::hypot((t.points[b].x-t.points[a].x)*o.width,(t.points[b].y-t.points[a].y)*o.height);
    };
    double total=0;
    for (size_t i=0;i<t.boundary.size();++i) total+=length(t.boundary[i],t.boundary[(i+1)%t.boundary.size()]);
    std::vector<uint32_t> boundary;
    double walked=0;
    for (size_t i=0;i<t.boundary.size();++i)
    {
        const auto a=t.boundary[i],b=t.boundary[(i+1)%t.boundary.size()];
        const auto p=t.points[a],q=t.points[b];
        const double distance=length(a,b),u0=walked/total*o.sideRepeatU,u1=(walked+distance)/total*o.sideRepeatU;
        boundary.push_back(a); uint32_t previous=a;
        for (double cut=std::floor(u0+1e-6)+1;cut<u1-1e-6;cut+=1)
        {
            const double f=(cut-u0)/(u1-u0);
            const auto mid=static_cast<uint32_t>(t.points.size());
            t.points.push_back({static_cast<float>(p.x+f*(q.x-p.x)),static_cast<float>(p.y+f*(q.y-p.y))});
            sideSplitEdge(t.triangles,previous,b,mid);
            sideSplitEdge(t.backTriangles,previous,b,mid);
            boundary.push_back(mid); previous=mid;
        }
        walked+=distance;
    }
    t.boundary=std::move(boundary);
}
inline IMAGE_MESH_POINT sideMap(const IMAGE_MESH_POINT &p,const std::vector<IMAGE_MESH_POINT> &outer,
                               const std::vector<IMAGE_MESH_POINT> &inner)
{
    double best=1e100; IMAGE_MESH_POINT result{};
    for (size_t i=0;i<outer.size();++i)
    {
        const size_t j=(i+1)%outer.size();
        const double dx=outer[j].x-outer[i].x,dy=outer[j].y-outer[i].y;
        const double t=std::clamp(((p.x-outer[i].x)*dx+(p.y-outer[i].y)*dy)/(dx*dx+dy*dy),0.0,1.0);
        const double error=std::hypot(p.x-outer[i].x-t*dx,p.y-outer[i].y-t*dy);
        if (error<best)
        {
            best=error;
            result={static_cast<float>(inner[i].x+t*(inner[j].x-inner[i].x)),
                    static_cast<float>(inner[i].y+t*(inner[j].y-inner[i].y))};
        }
    }
    return result;
}
}}
#endif
