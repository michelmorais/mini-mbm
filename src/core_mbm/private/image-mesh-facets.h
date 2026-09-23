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

#ifndef IMAGE_MESH_FACETS_H
#define IMAGE_MESH_FACETS_H
#include "image-mesh-topology.h"
#include "image-mesh-progress.h"
#include <cmath>
#include <limits>
namespace mbm { namespace image_mesh {
// A coarse piecewise-planar field, shared by geometry, hole cuts and height maps.
// The bins avoid scanning every facet for every pixel in the height map.
struct FACET_FIELD
{
    TOPOLOGY topology;
    std::vector<float> heights;
    std::array<std::vector<uint32_t>,1024> bins;
    static int cell(double v) { return std::clamp(static_cast<int>(v*32),0,31); }
    bool prepare(const IMAGE_MESH_OPTIONS &o,const std::vector<IMAGE_MESH_POINT> &contour,std::string &error)
    {
        const auto fail=[&](const char *message) { error=message;return false; };
        if (o.curvedHierarchy) return fail("Faceting requires the point/circle profile, without a target hierarchy");
        if (o.curvedFacetSectors<8 || o.curvedFacetSectors>128 || o.curvedFacetRings<1 || o.curvedFacetRings>16)
            return fail("Faceting requires 8..128 sectors and 1..16 rings");
        for (size_t i=0;i<contour.size();++i)
        {
            const auto &a=contour[i],&b=contour[(i+1)%contour.size()],&c=contour[(i+2)%contour.size()];
            if ((static_cast<double>(b.x)-a.x)*(c.y-b.y)-(static_cast<double>(b.y)-a.y)*(c.x-b.x)<-1e-10)
                return fail("Automatic faceting requires a convex outer contour");
        }
        constexpr double pi=3.14159265358979323846;
        struct RAY { double angle;int corner; };
        std::vector<RAY> rays;
        for (uint32_t i=0;i<o.curvedFacetSectors;++i) rays.push_back({2*pi*i/o.curvedFacetSectors,-1});
        for (size_t i=0;i<contour.size();++i)
        {
            double a=std::atan2((contour[i].y-o.curvedY)*o.height,(contour[i].x-o.curvedX)*o.width);
            if (a<0) a+=2*pi;
            rays.push_back({a,static_cast<int>(i)});
        }
        std::sort(rays.begin(),rays.end(),[](const RAY &a,const RAY &b){return a.angle<b.angle;});
        std::vector<RAY> unique;
        for (const auto &ray:rays)
        {
            if (!unique.empty() && ray.angle-unique.back().angle<1e-7)
            { if (ray.corner>=0) unique.back()=ray; }
            else unique.push_back(ray);
        }
        if (unique.size()>1 && unique.front().angle+2*pi-unique.back().angle<1e-7)
        { if (unique.back().corner>=0) unique.front()=unique.back();unique.pop_back(); }
        std::vector<IMAGE_MESH_POINT> outer;
        for (const auto &ray:unique)
        {
            checkpoint(o,"facets",0.2f);
            if (ray.corner>=0) { outer.push_back(contour[ray.corner]);continue; }
            const double dx=std::cos(ray.angle)/o.width,dy=std::sin(ray.angle)/o.height;
            double hit=std::numeric_limits<double>::infinity();
            for (size_t i=0;i<contour.size();++i)
            {
                const auto &a=contour[i],&b=contour[(i+1)%contour.size()];
                const double ax=a.x-o.curvedX,ay=a.y-o.curvedY,ex=b.x-a.x,ey=b.y-a.y;
                const double det=dx*ey-dy*ex;
                if (std::abs(det)<1e-20) continue;
                const double r=(ax*ey-ay*ex)/det,e=(ax*dy-ay*dx)/det;
                if (r>0 && e>=-1e-7 && e<=1.0000001) hit=std::min(hit,r);
            }
            if (!std::isfinite(hit)) return fail("Faceting could not reach the outer contour");
            outer.push_back({static_cast<float>(o.curvedX+hit*dx),static_cast<float>(o.curvedY+hit*dy)});
        }
        topology=TOPOLOGY{};topology.contour=contour;heights.clear();
        topology.points.push_back({o.curvedX,o.curvedY});
        const auto level=[&](float fraction) { return o.curvedTarget==o.curvedEdge?0.0f:(o.curvedTarget>o.curvedEdge?1-fraction:fraction); };
        heights.push_back(level(0));
        const uint32_t n=static_cast<uint32_t>(outer.size());
        const uint32_t layers=o.curvedFacetRings+(o.curvedRadius>0?1:0);
        for (uint32_t layer=0;layer<layers;++layer)
        {
            const float fraction=static_cast<float>(layer+(o.curvedRadius>0?0:1))/o.curvedFacetRings;
            for (const auto &p:outer)
            {
                const double distance=std::hypot((p.x-o.curvedX)*o.width,(p.y-o.curvedY)*o.height);
                const double f=o.curvedRadius/distance+(1-o.curvedRadius/distance)*fraction;
                topology.points.push_back(layer+1==layers?p:IMAGE_MESH_POINT{
                    static_cast<float>(o.curvedX+(p.x-o.curvedX)*f),static_cast<float>(o.curvedY+(p.y-o.curvedY)*f)});
                heights.push_back(level(fraction));
            }
        }
        for (uint32_t i=0;i<n;++i)
        {
            const uint32_t j=(i+1)%n;
            topology.triangles.push_back({0,1+i,1+j});
            for (uint32_t layer=1;layer<layers;++layer)
            {
                const uint32_t a=1+(layer-1)*n+i,b=1+(layer-1)*n+j,c=a+n,d=b+n;
                topology.triangles.push_back({a,c,d});topology.triangles.push_back({a,d,b});
            }
            topology.boundary.push_back(1+(layers-1)*n+i);
        }
        topology.loopEnds.push_back(n);
        for (auto &bin:bins) bin.clear();
        for (uint32_t i=0;i<topology.triangles.size();++i)
        {
            const auto &f=topology.triangles[i];
            const auto &a=topology.points[f[0]],&b=topology.points[f[1]],&c=topology.points[f[2]];
            const double determinant=(static_cast<double>(b.x)-a.x)*(c.y-a.y)-(static_cast<double>(b.y)-a.y)*(c.x-a.x);
            if (determinant<=1e-12) return fail("Faceting creates a degenerate face; reduce sectors/rings or move the center");
            for (int y=cell(std::min({a.y,b.y,c.y})-1e-6);y<=cell(std::max({a.y,b.y,c.y})+1e-6);++y)
                for (int x=cell(std::min({a.x,b.x,c.x})-1e-6);x<=cell(std::max({a.x,b.x,c.x})+1e-6);++x)
                    bins[y*32+x].push_back(i);
        }
        return true;
    }
    float level(float x,float y) const
    {
        for (auto i:bins[cell(y)*32+cell(x)])
        {
            const auto &f=topology.triangles[i];
            const auto &a=topology.points[f[0]],&b=topology.points[f[1]],&c=topology.points[f[2]];
            const double det=(static_cast<double>(b.x)-a.x)*(c.y-a.y)-(static_cast<double>(b.y)-a.y)*(c.x-a.x);
            const double wb=((static_cast<double>(x)-a.x)*(c.y-a.y)-(static_cast<double>(y)-a.y)*(c.x-a.x))/det;
            const double wc=((static_cast<double>(b.x)-a.x)*(y-a.y)-(static_cast<double>(b.y)-a.y)*(x-a.x))/det;
            const double wa=1-wb-wc;
            if (wa>=-1e-5 && wb>=-1e-5 && wc>=-1e-5)
                return static_cast<float>(std::clamp(wa*heights[f[0]]+wb*heights[f[1]]+wc*heights[f[2]],0.0,1.0));
        }
        return 0;
    }
};
} }
#endif
