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

#ifndef IMAGE_MESH_CURVED_HIERARCHY_H
#define IMAGE_MESH_CURVED_HIERARCHY_H
#include "image-mesh-topology.h"
#include "image-mesh-progress.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mbm { namespace image_mesh {
// Prepared worker-owned geometry. Evaluation is allocation-free and independent
// of sibling order. Coordinates below use the final mesh's metric, not pixels.
struct CURVED_HIERARCHY
{
    struct POINT { double x=0,y=0; };
    struct CURVED_DOMAIN
    {
        std::vector<IMAGE_MESH_POINT> normalized;
        std::vector<POINT> points;
        std::vector<uint32_t> locals;
        uint32_t parent=0,depth=0;
        int target=-1;
        double thickness=0,minX=0,minY=0,maxX=0,maxY=0;
        IMAGE_MESH_CURVED_PROFILE profile=IMAGE_MESH_CURVED_PROFILE::LINEAR;
        uint32_t bezierPoints=2;
        double bezier1=0,bezier2=1,bezier3=1,bezier4=1;
        bool inherited=false;
    };
    std::vector<CURVED_DOMAIN> domains;
    double tolerance=1e-6;
    static double cross(POINT a,POINT b,POINT c)
    { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
    static POINT projection(POINT p,POINT a,POINT b)
    {
        const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
        const double t=length>0?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.0,1.0):0;
        return {a.x+dx*t,a.y+dy*t};
    }
    static double distance(POINT a,POINT b) { return std::hypot(a.x-b.x,a.y-b.y); }
    POINT nearest(const CURVED_DOMAIN &d,POINT p) const
    {
        POINT best=d.points[0];double minimum=distance(best,p);
        const size_t edges=d.points.size()>2?d.points.size():d.points.size()-1;
        for (size_t i=0;i<edges;++i)
        {
            const POINT q=projection(p,d.points[i],d.points[(i+1)%d.points.size()]);
            const double length=distance(q,p);
            if (length<minimum) { minimum=length;best=q; }
        }
        return best;
    }
    bool contains(const CURVED_DOMAIN &d,POINT p,bool strict=false) const
    {
        if (p.x<d.minX-tolerance || p.x>d.maxX+tolerance || p.y<d.minY-tolerance || p.y>d.maxY+tolerance) return false;
        if (distance(nearest(d,p),p)<=tolerance) return !strict;
        if (d.points.size()<3) return false;
        bool inside=false;
        for (size_t i=0,j=d.points.size()-1;i<d.points.size();j=i++)
        {
            const POINT a=d.points[i],b=d.points[j];
            if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
        }
        return inside;
    }
    bool intersects(POINT a,POINT b,POINT c,POINT d) const
    {
        if (distance(projection(a,c,d),a)<=tolerance || distance(projection(b,c,d),b)<=tolerance ||
            distance(projection(c,a,b),c)<=tolerance || distance(projection(d,a,b),d)<=tolerance) return true;
        return cross(a,b,c)*cross(a,b,d)<0 && cross(c,d,a)*cross(c,d,b)<0;
    }
    bool boundariesIntersect(const CURVED_DOMAIN &a,const CURVED_DOMAIN &b) const
    {
        const size_t edges=a.points.size()>2?a.points.size():a.points.size()-1;
        for (size_t i=0;i<edges;++i) for (size_t j=0;j<b.points.size();++j)
            if (intersects(a.points[i],a.points[(i+1)%a.points.size()],b.points[j],b.points[(j+1)%b.points.size()])) return true;
        return false;
    }
    bool ancestor(uint32_t a,uint32_t b) const
    {
        while (b) { b=domains[b].parent;if (a==b) return true; }
        return false;
    }
    bool prepare(const IMAGE_MESH_OPTIONS &o,const std::vector<IMAGE_MESH_POINT> &outer,std::string &error)
    {
        const auto fail=[&](uint32_t i,const char *message) { error="Curved node "+std::to_string(i)+": "+message;return false; };
        if (o.curvedNodeCount>32 || (o.curvedNodeCount && !o.curvedNodes)) return fail(0,"at most 32 nodes are supported");
        tolerance=std::max(o.width,o.height)*1e-7;
        domains.resize(o.curvedNodeCount+1);domains[0].normalized=outer;domains[0].thickness=o.curvedEdge;
        for (uint32_t i=0;i<domains.size();++i)
        {
            checkpoint(o,"heights",0.1f);
            auto &d=domains[i];
            if (i)
            {
                const auto &node=o.curvedNodes[i-1];
                if (node.polyline) return fail(i,"Polyline targets require interior transition");
                if (node.parent>=i || !node.points || node.count<1 || node.count>128 ||
                    (node.inherited && node.count<3) || !std::isfinite(node.thickness) || node.thickness<0.001f || node.thickness>1000000)
                    return fail(i,"invalid owner, contour or thickness");
                if (node.profile!=IMAGE_MESH_CURVED_PROFILE::LINEAR && node.profile!=IMAGE_MESH_CURVED_PROFILE::SMOOTH &&
                    node.profile!=IMAGE_MESH_CURVED_PROFILE::BEZIER) return fail(i,"invalid transition profile");
                if (node.bezierPoints<2 || node.bezierPoints>4) return fail(i,"Bezier needs 2, 3 or 4 internal controls");
                for (float value:{node.bezier1,node.bezier2,node.bezier3,node.bezier4})
                    if (!std::isfinite(value) || value<0 || value>1) return fail(i,"Bezier controls must each be in [0,1]");
                d.profile=node.profile;d.bezier1=node.bezier1;d.bezier2=node.bezier2;
                d.bezierPoints=node.bezierPoints;d.bezier3=node.bezier3;d.bezier4=node.bezier4;
                d.parent=node.parent;d.inherited=node.inherited;d.thickness=node.thickness;
                d.depth=domains[d.parent].depth+1;
                if (d.depth>8) return fail(i,"maximum hierarchy depth is 8");
                if (domains[d.parent].points.size()<3) return fail(i,"points and lines are terminal targets");
                d.normalized.assign(node.points,node.points+node.count);
                if (node.count>=3)
                {
                    IMAGE_MESH_OPTIONS shape=o;shape.heightSource=IMAGE_MESH_HEIGHT_SOURCE::MANUAL;
                    shape.holes=nullptr;shape.holeCount=0; // Cutouts do not constrain virtual controls.
                    shape.shape=IMAGE_MESH_SHAPE::POLYGON;shape.contour=node.points;shape.contourCount=node.count;
                    shape.columns=shape.rows=1;shape.maxVertices=65535;shape.maxTriangles=131070;
                    TOPOLOGY t;
                    if (!buildTopology(shape,t,error)) { error="Curved node "+std::to_string(i)+": "+error;return false; }
                    d.normalized=std::move(t.contour);
                }
                auto &parent=domains[d.parent];
                if (d.inherited) parent.locals.push_back(i);
                else
                {
                    if (parent.target>=0) return fail(i,"the owner already has a target");
                    parent.target=static_cast<int>(i);
                }
            }
            for (const auto &p:d.normalized)
            {
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.x>1 || p.y<0 || p.y>1)
                    return fail(i,"points must be finite normalized coordinates in [0,1]");
                d.points.push_back({static_cast<double>(p.x)*o.width,static_cast<double>(p.y)*o.height});
            }
            d.minX=d.maxX=d.points[0].x;d.minY=d.maxY=d.points[0].y;
            for (POINT p:d.points) { d.minX=std::min(d.minX,p.x);d.maxX=std::max(d.maxX,p.x);d.minY=std::min(d.minY,p.y);d.maxY=std::max(d.maxY,p.y); }
            if (!i) continue;
            if (!d.inherited && d.points.size()>2)
                for (size_t j=0;j<d.points.size();++j)
                    if (cross(d.points[j],d.points[(j+1)%d.points.size()],d.points[(j+2)%d.points.size()]) < -tolerance*tolerance)
                        return fail(i,"closed targets must be convex");
            const auto &parent=domains[d.parent];
            if (d.points.size()==2 && distance(d.points[0],d.points[1])<=tolerance) return fail(i,"line endpoints must be distinct");
            for (POINT p:d.points)
            {
                if (!contains(parent,p,true)) return fail(i,"shape must be strictly inside its owner");
                if (d.inherited) continue;
                for (size_t j=0;j<parent.points.size();++j)
                {
                    const POINT a=parent.points[j],b=parent.points[(j+1)%parent.points.size()];
                    if (cross(a,b,p)/distance(a,b)<=tolerance) return fail(i,"target must see the entire owner contour");
                }
            }
            if (boundariesIntersect(d,parent)) return fail(i,"shape touches or crosses its owner");
        }
        for (uint32_t i=1;i<domains.size();++i) for (uint32_t j=i+1;j<domains.size();++j)
        {
            const auto &a=domains[i],&b=domains[j];
            if (!a.inherited || !b.inherited || ancestor(i,j) || ancestor(j,i)) continue;
            if (boundariesIntersect(a,b) || contains(a,b.points[0]) || contains(b,a.points[0]))
                return fail(j,"independent local regions overlap; nesting requires an explicit owner");
        }
        return true;
    }
    double evaluate(uint32_t owner,POINT p) const
    {
        const auto &d=domains[owner];
        for (uint32_t local:d.locals) if (contains(domains[local],p)) return evaluate(local,p);
        return base(owner,p);
    }
    double base(uint32_t owner,POINT p) const
    {
        const auto &d=domains[owner];
        if (d.target<0) return d.inherited?base(d.parent,p):d.thickness;
        const auto &target=domains[static_cast<size_t>(d.target)];
        if (contains(target,p)) return target.points.size()>2?evaluate(static_cast<uint32_t>(d.target),p):target.thickness;
        const POINT q=nearest(target,p);
        const double dx=p.x-q.x,dy=p.y-q.y,length=std::hypot(dx,dy);
        if (length<=tolerance) return target.thickness;
        double hit=std::numeric_limits<double>::infinity();
        for (size_t i=0;i<d.points.size();++i)
        {
            const POINT a=d.points[i],b=d.points[(i+1)%d.points.size()];
            const double ex=b.x-a.x,ey=b.y-a.y,det=dx*ey-dy*ex;
            if (std::abs(det)<1e-20) continue;
            const double ray=((a.x-q.x)*ey-(a.y-q.y)*ex)/det;
            const double edge=((a.x-q.x)*dy-(a.y-q.y)*dx)/det;
            if (ray>0 && edge>=-1e-7 && edge<=1.0000001) hit=std::min(hit,ray);
        }
        if (!std::isfinite(hit)) return d.inherited?base(d.parent,p):d.thickness;
        const POINT border{q.x+hit*dx,q.y+hit*dy};
        const double h=d.inherited?base(d.parent,border):d.thickness;
        const double t=std::clamp(1-1/hit,0.0,1.0);
        double weight=t;
        if (target.profile==IMAGE_MESH_CURVED_PROFILE::SMOOTH) weight=t*t*(3-2*t);
        else if (target.profile==IMAGE_MESH_CURVED_PROFILE::BEZIER)
        {
            if (target.bezierPoints==2)
            {
                const double u=1-t;
                weight=3*u*u*t*target.bezier1+3*u*t*t*target.bezier2+t*t*t;
            }
            else
            {
                double values[6]={0,target.bezier1,target.bezier2,target.bezier3,target.bezier4,1};
                const uint32_t degree=target.bezierPoints+1;values[degree]=1;
                for (uint32_t remaining=degree;remaining>0;--remaining)
                    for (uint32_t i=0;i<remaining;++i) values[i]+=(values[i+1]-values[i])*t;
                weight=values[0];
            }
        }
        return h+(target.thickness-h)*weight;
    }
    float level(float u,float v,const IMAGE_MESH_OPTIONS &o) const
    {
        const double scale=o.relief*(o.curvedSymmetric?2:1);
        if (scale==0) return 0;
        return static_cast<float>(std::clamp((evaluate(0,{static_cast<double>(u)*o.width,static_cast<double>(v)*o.height})-o.depth)/scale,0.0,1.0));
    }
};
} }
#endif
