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

#include "image-mesh-topology.h"
#include "image-mesh-height.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <map>
#include <numeric>

namespace mbm { namespace image_mesh {
namespace {
    constexpr double epsilon = 1e-10;
    double cross(const IMAGE_MESH_POINT &a, const IMAGE_MESH_POINT &b, const IMAGE_MESH_POINT &c)
    {
        return (static_cast<double>(b.x)-a.x)*(static_cast<double>(c.y)-a.y) -
               (static_cast<double>(b.y)-a.y)*(static_cast<double>(c.x)-a.x);
    }
    bool onSegment(const IMAGE_MESH_POINT &a, const IMAGE_MESH_POINT &b, const IMAGE_MESH_POINT &p)
    {
        return std::abs(cross(a,b,p))<=epsilon && p.x>=std::min(a.x,b.x)-epsilon &&
            p.x<=std::max(a.x,b.x)+epsilon && p.y>=std::min(a.y,b.y)-epsilon && p.y<=std::max(a.y,b.y)+epsilon;
    }
    bool intersects(const IMAGE_MESH_POINT &a, const IMAGE_MESH_POINT &b, const IMAGE_MESH_POINT &c, const IMAGE_MESH_POINT &d)
    {
        const double abC=cross(a,b,c), abD=cross(a,b,d), cdA=cross(c,d,a), cdB=cross(c,d,b);
        return (abC*abD<0 && cdA*cdB<0) || onSegment(a,b,c) || onSegment(a,b,d) || onSegment(c,d,a) || onSegment(c,d,b);
    }
    bool budget(const IMAGE_MESH_OPTIONS &o, uint64_t vertices, uint64_t triangles,
                const char *stage, std::string &error)
    {
        const uint32_t vertexLimit=std::min(o.maxVertices,65535u);
        if (vertices<=vertexLimit && triangles<=o.maxTriangles) return true;
        error=std::string("Geometry budget exceeded ")+stage+":";
        if (vertices>vertexLimit)
            error+=" vertices >= "+std::to_string(vertices)+", limit "+std::to_string(vertexLimit)+";";
        if (triangles>o.maxTriangles)
            error+=" triangles >= "+std::to_string(triangles)+", limit "+std::to_string(o.maxTriangles)+";";
        error+=" includes front, back and sides. Counts are lower bounds, not final totals. "
               "Engine vertex ceiling: 65535. Reduce columns/rows or ellipseSegments.";
        return false;
    }
    bool budget(const IMAGE_MESH_OPTIONS &o, const TOPOLOGY &t, const char *stage, std::string &error)
    {
        if (o.backOpen)
            return budget(o,t.points.size()+4*t.boundary.size(),t.triangles.size()+2*t.boundary.size(),stage,error);
        if (o.followImage && !o.backRelief && o.holeCount==0)
            return budget(o,t.points.size()+5*t.boundary.size(),t.triangles.size()+3*t.boundary.size()-2,stage,error);
        return budget(o,2*t.points.size()+4*t.boundary.size(),
                      2*t.triangles.size()+2*t.boundary.size(),stage,error);
    }
    uint64_t edgeKey(uint32_t a, uint32_t b)
    {
        return (static_cast<uint64_t>(std::min(a,b))<<32) | std::max(a,b);
    }
    bool insideRing(const std::vector<IMAGE_MESH_POINT> &ring,const IMAGE_MESH_POINT &p)
    {
        bool inside=false;
        for (size_t i=0,j=ring.size()-1;i<ring.size();j=i++)
        {
            const auto &a=ring[j],&b=ring[i];
            if (onSegment(a,b,p)) return false;
            if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
        }
        return inside;
    }
    bool addHoles(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t,std::string &error)
    {
        if (!o.holeCount) return true;
        const auto fail=[&](const char *why) { error=why;return false; };
        if (!o.holes || o.holeCount>16) return fail("At most 16 hole contours are supported");
        t.loopEnds.push_back(static_cast<uint32_t>(t.boundary.size()));
        for (uint32_t h=0;h<o.holeCount;++h)
        {
            const auto &input=o.holes[h];
            if (!input.points || input.count<3 || input.count>128) return fail("Each hole needs 3..128 points");
            std::vector<IMAGE_MESH_POINT> ring(input.points,input.points+input.count);
            double area=0;
            for (size_t i=0;i<ring.size();++i)
            {
                const auto &a=ring[i],&b=ring[(i+1)%ring.size()],&c=ring[(i+2)%ring.size()];
                if (!std::isfinite(a.x) || !std::isfinite(a.y) || a.x<0 || a.y<0 || a.x>1 || a.y>1)
                    return fail("Hole coordinates must be finite and within 0..1");
                if (std::hypot(a.x-b.x,a.y-b.y)<1e-6) return fail("Repeated or too close hole points");
                if (std::abs(cross(a,b,c))<=epsilon && !onSegment(a,c,b)) return fail("Hole contour backtracks");
                if (!insideRing(t.contour,a)) return fail("Hole must be strictly inside the outer contour");
                for (size_t j=i+1;j<ring.size();++j)
                    if (j!=(i+1)%ring.size() && (j+1)%ring.size()!=i && intersects(a,b,ring[j],ring[(j+1)%ring.size()]))
                        return fail("Hole contour crosses or touches itself");
                for (size_t j=0;j<t.contour.size();++j)
                    if (intersects(a,b,t.contour[j],t.contour[(j+1)%t.contour.size()])) return fail("Hole touches or crosses the outer contour");
                for (const auto &other:t.holes)
                {
                    if (insideRing(other,a) || insideRing(ring,other[0])) return fail("Holes cannot overlap or contain each other");
                    for (size_t j=0;j<other.size();++j)
                        if (intersects(a,b,other[j],other[(j+1)%other.size()])) return fail("Holes cannot touch or intersect");
                }
                area+=static_cast<double>(a.x)*b.y-static_cast<double>(a.y)*b.x;
            }
            if (std::abs(area)<1e-8) return fail("Hole has negligible area");
            if (area>0) std::reverse(ring.begin(),ring.end());
            t.holes.push_back(ring);
            for (const auto &p:ring) { t.boundary.push_back(static_cast<uint32_t>(t.points.size()));t.points.push_back(p); }
            t.loopEnds.push_back(static_cast<uint32_t>(t.boundary.size()));
        }
        return true;
    }
    bool bridgeHoles(const TOPOLOGY &t,std::vector<uint32_t> &path)
    {
        if (t.holes.empty()) return true;
        path.assign(t.boundary.begin(),t.boundary.begin()+t.loopEnds[0]);
        struct HOLE_ORDER { size_t loop; float x,y; };
        std::vector<HOLE_ORDER> order;
        for (size_t hole=1;hole<t.loopEnds.size();++hole)
        {
            const auto &first=t.points[t.boundary[t.loopEnds[hole-1]]];
            HOLE_ORDER entry{hole,first.x,first.y};
            for (uint32_t j=t.loopEnds[hole-1];j<t.loopEnds[hole];++j)
            {
                const auto &p=t.points[t.boundary[j]];
                if (p.x<entry.x || (p.x==entry.x && p.y<entry.y)) { entry.x=p.x;entry.y=p.y; }
            }
            order.push_back(entry);
        }
        // Join exposed holes first. An interior hole can be hidden from all outer
        // vertices by holes that have not yet been connected to the path.
        std::sort(order.begin(),order.end(),[](const HOLE_ORDER &a,const HOLE_ORDER &b) {
            return a.x<b.x || (a.x==b.x && a.y<b.y);
        });
        for (const auto &entry:order)
        {
            const size_t hole=entry.loop;
            const uint32_t start=t.loopEnds[hole-1],end=t.loopEnds[hole];
            double best=1e100;size_t chosen=0;uint32_t vertex=0;bool found=false;
            for (size_t i=0;i<path.size();++i) for (uint32_t j=start;j<end;++j)
            {
                const auto a=path[i],b=t.boundary[j];const auto &p=t.points[a],&q=t.points[b];
                const double length=std::hypot(p.x-q.x,p.y-q.y);
                if (length>=best) continue;
                // A bridge endpoint may occur more than once after joining holes.
                // Choose the occurrence whose local interior contains the bridge;
                // visibility alone can splice it into the wrong side of the path.
                const auto locallyInside=[&](uint32_t previous,uint32_t current,uint32_t next,const IMAGE_MESH_POINT &target) {
                    const auto &u=t.points[previous],&v=t.points[current],&w=t.points[next];
                    const double left=cross(u,v,target),right=cross(v,w,target);
                    if (cross(u,v,w)>=0) return left>=0 && right>=0;
                    return left>0 || right>0;
                };
                if (!locallyInside(path[(i+path.size()-1)%path.size()],a,path[(i+1)%path.size()],q) ||
                    !locallyInside(t.boundary[j==start?end-1:j-1],b,t.boundary[j+1==end?start:j+1],p)) continue;
                const IMAGE_MESH_POINT mid={(p.x+q.x)*.5f,(p.y+q.y)*.5f};
                if (!insideRing(t.contour,mid)) continue;
                bool blocked=false;
                for (const auto &r:t.holes) if (insideRing(r,mid)) { blocked=true;break; }
                const auto hits=[&](uint32_t c,uint32_t d) {
                    if (c==a || c==b || d==a || d==b)
                    {
                        const auto other=(c==a || c==b)?d:c;
                        return other!=a && other!=b && onSegment(p,q,t.points[other]);
                    }
                    return intersects(p,q,t.points[c],t.points[d]);
                };
                for (size_t k=0;k<t.boundary.size() && !blocked;++k)
                    blocked=hits(t.boundary[k],t.boundary[t.nextBoundary(k)]);
                for (size_t k=0;k<path.size() && !blocked;++k) blocked=hits(path[k],path[(k+1)%path.size()]);
                if (!blocked) { found=true;best=length;chosen=i;vertex=j; }
            }
            if (!found) return false;
            std::vector<uint32_t> joined;
            joined.insert(joined.end(),path.begin(),path.begin()+chosen+1);
            for (uint32_t k=0;k<end-start;++k) joined.push_back(t.boundary[start+(vertex-start+k)%(end-start)]);
            joined.push_back(t.boundary[vertex]);joined.push_back(path[chosen]);
            joined.insert(joined.end(),path.begin()+chosen+1,path.end());path=std::move(joined);
        }
        return true;
    }
    bool triangulateBoundary(const TOPOLOGY &t,std::vector<std::array<uint32_t,3>> &output)
    {
        auto remaining=t.boundary;
        if (!bridgeHoles(t,remaining)) return false;
        while (remaining.size()>3)
        {
            bool found=false;
            for (size_t i=0;i<remaining.size();++i)
            {
                const uint32_t a=remaining[(i+remaining.size()-1)%remaining.size()], b=remaining[i], c=remaining[(i+1)%remaining.size()];
                if (cross(t.points[a],t.points[b],t.points[c])<=epsilon) continue;
                bool contains=false;
                for (uint32_t v:remaining)
                {
                    if (v==a || v==b || v==c) continue;
                    if (cross(t.points[a],t.points[b],t.points[v])>=-epsilon &&
                        cross(t.points[b],t.points[c],t.points[v])>=-epsilon &&
                        cross(t.points[c],t.points[a],t.points[v])>=-epsilon) { contains=true; break; }
                }
                if (contains) continue;
                output.push_back({a,b,c}); remaining.erase(remaining.begin()+static_cast<std::ptrdiff_t>(i)); found=true; break;
            }
            if (!found) return false;
        }
        output.push_back({remaining[0],remaining[1],remaining[2]});
        return true;
    }

    double edgeLength(const IMAGE_MESH_POINT &a,const IMAGE_MESH_POINT &b,const IMAGE_MESH_OPTIONS &o)
    {
        const double x=(a.x-b.x)*o.columns,y=(a.y-b.y)*o.rows;
        return x*x+y*y;
    }
    float surface(const IMAGE_MESH_POINT &p,const HEIGHT_FIELD &field,const IMAGE_MESH_OPTIONS &o,const TOPOLOGY &t)
    {
        if (o.relief==0) return 0;
        float value=field.surface(p.x,p.y,o);
        if (o.lockBorder)
        {
            const float d=borderDistance(p,t);
            if (d<1e-7f) return 0;
            if (o.borderWidth>0) value*=std::min(1.0f,d/o.borderWidth);
        }
        return value;
    }
    bool needsDetail(const std::array<uint32_t,3> &tri,const TOPOLOGY &t,const HEIGHT_FIELD &field,const IMAGE_MESH_OPTIONS &o)
    {
        const auto &a=t.points[tri[0]],&b=t.points[tri[1]],&c=t.points[tri[2]];
        const double area=cross(a,b,c);
        const float ha=surface(a,field,o,t),hb=surface(b,field,o,t),hc=surface(c,field,o,t);
        const auto check=[&](const IMAGE_MESH_POINT &p)
        {
            const double wa=cross(b,c,p)/area,wb=cross(c,a,p)/area,wc=1-wa-wb;
            return wa>=-epsilon && wb>=-epsilon && wc>=-epsilon &&
                std::abs(surface(p,field,o,t)-(wa*ha+wb*hb+wc*hc))>o.heightTolerance;
        };
        if (check({(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3}) ||
            check({(a.x+b.x)/2,(a.y+b.y)/2}) || check({(b.x+c.x)/2,(b.y+c.y)/2}) || check({(c.x+a.x)/2,(c.y+a.y)/2})) return true;
        const uint32_t nx=std::min(std::max(1u,field.width-1),o.columns*2),ny=std::min(std::max(1u,field.height-1),o.rows*2);
        const uint32_t x0=static_cast<uint32_t>(std::ceil(std::min({a.x,b.x,c.x})*nx));
        const uint32_t x1=static_cast<uint32_t>(std::floor(std::max({a.x,b.x,c.x})*nx));
        const uint32_t y0=static_cast<uint32_t>(std::ceil(std::min({a.y,b.y,c.y})*ny));
        const uint32_t y1=static_cast<uint32_t>(std::floor(std::max({a.y,b.y,c.y})*ny));
        for (uint32_t y=y0;y<=y1;++y) for (uint32_t x=x0;x<=x1;++x)
            if (check({static_cast<float>(x)/nx,static_cast<float>(y)/ny})) return true;
        return false;
    }
    // Local Delaunay flips improve skinny triangles without moving image samples.
    void improveTriangles(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t)
    {
        for (unsigned pass=0;pass<3;++pass)
        {
            std::map<uint64_t,std::pair<size_t,uint32_t>> neighbors;
            std::vector<bool> changed(t.triangles.size(),false);
            bool any=false;
            for (size_t i=0;i<t.triangles.size();++i)
            {
                if ((i&127)==0) checkpoint(o,"topology",0.26f);
                const auto tri=t.triangles[i];
                for (unsigned e=0;e<3 && !changed[i];++e)
                {
                    const uint32_t a=tri[e],b=tri[(e+1)%3],c=tri[(e+2)%3];
                    const auto key=edgeKey(a,b); const auto it=neighbors.find(key);
                    if (it==neighbors.end()) { neighbors[key]={i,c}; continue; }
                    const size_t j=it->second.first; const uint32_t d=it->second.second;
                    if (changed[j]) continue;
                    const auto &pa=t.points[a],&pb=t.points[b],&pc=t.points[c],&pd=t.points[d];
                    if (cross(pc,pd,pb)<=epsilon || cross(pd,pc,pa)<=epsilon) continue;
                    const double ax=pa.x-pd.x,ay=pa.y-pd.y,bx=pb.x-pd.x,by=pb.y-pd.y,cx=pc.x-pd.x,cy=pc.y-pd.y;
                    const double det=(ax*ax+ay*ay)*(bx*cy-by*cx)-(bx*bx+by*by)*(ax*cy-ay*cx)+(cx*cx+cy*cy)*(ax*by-ay*bx);
                    if (det<=1e-12) continue;
                    t.triangles[i]={c,d,b}; t.triangles[j]={d,c,a}; changed[i]=changed[j]=true; any=true;
                }
            }
            if (!any) return;
        }
    }
    bool alignTransition(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t,const HEIGHT_FIELD &field,float iso,std::string &error)
    {
        if (iso<=0 || iso>=1) return true;
        std::map<uint64_t,uint32_t> cuts;
        const auto intersection=[&](uint32_t a,uint32_t b,float va,float vb)
        {
            if (std::abs(va-iso)<1e-6f) return a;
            if (std::abs(vb-iso)<1e-6f) return b;
            const auto key=edgeKey(a,b); auto it=cuts.find(key);
            if (it!=cuts.end()) return it->second;
            const auto pa=t.points[a],pb=t.points[b];
            float lo=0,hi=1;
            // Solve on the processed image, not just the endpoint intensities.
            for (unsigned n=0;n<20;++n)
            {
                const float f=(lo+hi)*0.5f,v=field.transition(pa.x+(pb.x-pa.x)*f,pa.y+(pb.y-pa.y)*f,o);
                if ((v<iso)==(va<iso)) lo=f; else hi=f;
            }
            const float f=(lo+hi)*0.5f;
            const uint32_t id=static_cast<uint32_t>(t.points.size());
            t.points.push_back({pa.x+(pb.x-pa.x)*f,pa.y+(pb.y-pa.y)*f}); cuts[key]=id; return id;
        };
        std::vector<std::array<uint32_t,3>> result;
        for (const auto &tri:t.triangles)
        {
            checkpoint(o,"alignment",0.48f);
            float v[3]; for (unsigned i=0;i<3;++i) v[i]=field.transition(t.points[tri[i]].x,t.points[tri[i]].y,o);
            if ((v[0]<iso)==(v[1]<iso) && (v[1]<iso)==(v[2]<iso)) { result.push_back(tri); continue; }
            for (unsigned side=0;side<2;++side)
            {
                std::vector<uint32_t> polygon;
                for (unsigned i=0;i<3;++i)
                {
                    const unsigned j=(i+1)%3;
                    if ((v[i]<iso)==(side==0)) polygon.push_back(tri[i]);
                    if ((v[i]<iso)!=(v[j]<iso)) polygon.push_back(intersection(tri[i],tri[j],v[i],v[j]));
                }
                polygon.erase(std::unique(polygon.begin(),polygon.end()),polygon.end());
                if (polygon.size()>1 && polygon.front()==polygon.back()) polygon.pop_back();
                for (size_t i=1;i+1<polygon.size();++i)
                    if (cross(t.points[polygon[0]],t.points[polygon[i]],t.points[polygon[i+1]])>0)
                        result.push_back({polygon[0],polygon[i],polygon[i+1]});
            }
            if (!budget(o,t,"while aligning grooves",error)) return false;
        }
        std::vector<uint32_t> boundary,ends;
        for (size_t i=0;i<t.boundary.size();++i)
        {
            const auto a=t.boundary[i],b=t.boundary[t.nextBoundary(i)]; boundary.push_back(a);
            auto it=cuts.find(edgeKey(a,b)); if (it!=cuts.end()) boundary.push_back(it->second);
            if (!t.loopEnds.empty() && t.nextBoundary(i)<=i) ends.push_back(static_cast<uint32_t>(boundary.size()));
        }
        t.boundary=std::move(boundary); t.loopEnds=std::move(ends); t.triangles=std::move(result);
        return budget(o,t,"while aligning grooves",error);
    }
    // Optimize the actual height approximation on both candidate triangulations.
    // Delaunay alone only sees XY and can connect opposite sides of a groove.
    void improveRelief(const IMAGE_MESH_OPTIONS &o, TOPOLOGY &t, const HEIGHT_FIELD &field)
    {
        std::vector<float> heights, intensities;
        for (const auto &p : t.points)
        {
            heights.push_back(surface(p,field,o,t));
            intensities.push_back(field.transition(p.x,p.y,o));
        }
        const auto constrained=[&](uint32_t a,uint32_t b)
        {
            const float half=o.twoLevels?o.grooveTransition*0.5f:0.0f;
            const float levels[]={o.grooveThreshold-half,o.grooveThreshold+half,0.0001f,0.9999f};
            for (float level : levels)
                if (std::abs(intensities[a]-level)<2e-5f && std::abs(intensities[b]-level)<2e-5f) return true;
            return false;
        };
        for (unsigned pass=0;pass<8;++pass)
        {
            std::map<uint64_t,std::pair<size_t,uint32_t>> neighbors;
            std::vector<bool> changed(t.triangles.size(),false);
            bool any=false;
            for (size_t i=0;i<t.triangles.size();++i)
            {
                if ((i&127)==0) checkpoint(o,"alignment",0.5f+0.04f*pass);
                const auto tri=t.triangles[i];
                for (unsigned e=0;e<3 && !changed[i];++e)
                {
                    const uint32_t a=tri[e],b=tri[(e+1)%3],c=tri[(e+2)%3];
                    const auto key=edgeKey(a,b); const auto it=neighbors.find(key);
                    if (it==neighbors.end()) { neighbors[key]={i,c}; continue; }
                    const size_t j=it->second.first; const uint32_t d=it->second.second;
                    if (changed[j] || constrained(a,b)) continue;
                    const auto &pa=t.points[a],&pb=t.points[b],&pc=t.points[c],&pd=t.points[d];
                    if (cross(pc,pd,pb)<=epsilon || cross(pd,pc,pa)<=epsilon) continue;
                    const std::array<uint32_t,3> oldTris[]={tri,t.triangles[j]},newTris[]={ {c,d,b},{d,c,a} };
                    const auto quality=[&](const std::array<uint32_t,3> &f)
                    {
                        const auto &u=t.points[f[0]],&v=t.points[f[1]],&w=t.points[f[2]];
                        const double uv=std::hypot((u.x-v.x)*o.width,(u.y-v.y)*o.height);
                        const double vw=std::hypot((v.x-w.x)*o.width,(v.y-w.y)*o.height);
                        const double wu=std::hypot((w.x-u.x)*o.width,(w.y-u.y)*o.height);
                        return cross(u,v,w)*o.width*o.height/(uv*uv+vw*vw+wu*wu);
                    };
                    if (std::min(quality(newTris[0]),quality(newTris[1]))+1e-8 <
                        std::min(quality(oldTris[0]),quality(oldTris[1]))) continue;
                    // Use the SAME samples for both diagonals, including both centroids
                    // and both diagonal midpoints. Never compare unrelated error probes.
                    std::array<IMAGE_MESH_POINT,6> probes;
                    unsigned probeIndex=0;
                    for (const auto &face : {oldTris[0],oldTris[1],newTris[0],newTris[1]})
                    {
                        const auto &p=t.points[face[0]],&q=t.points[face[1]],&r=t.points[face[2]];
                        probes[probeIndex++]={(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3};
                    }
                    probes[4]={(pa.x+pb.x)*0.5f,(pa.y+pb.y)*0.5f};
                    probes[5]={(pc.x+pd.x)*0.5f,(pc.y+pd.y)*0.5f};
                    double errors[2]={0,0};
                    for (const auto &p : probes)
                    {
                        const double actual=surface(p,field,o,t);
                        for (unsigned candidate=0;candidate<2;++candidate)
                        {
                            const auto *faces=candidate?newTris:oldTris;
                            for (unsigned k=0;k<2;++k)
                            {
                                const auto &f=faces[k]; const auto &u=t.points[f[0]],&v=t.points[f[1]],&w=t.points[f[2]];
                                const double area=cross(u,v,w),wa=cross(v,w,p)/area,wb=cross(w,u,p)/area,wc=1-wa-wb;
                                if (k==0 && (wa<-1e-6 || wb<-1e-6 || wc<-1e-6)) continue;
                                const double delta=actual-wa*heights[f[0]]-wb*heights[f[1]]-wc*heights[f[2]];
                                errors[candidate]+=delta*delta;
                                break;
                            }
                        }
                    }
                    if (errors[1]+1e-10>=errors[0]*0.99) continue;
                    t.triangles[i]=newTris[0]; t.triangles[j]=newTris[1];
                    changed[i]=changed[j]=true; any=true;
                }
            }
            if (!any) break;
        }
    }
    void splitEdges(TOPOLOGY &t,const std::map<uint64_t,uint32_t> &midpoints)
    {
        std::vector<uint32_t> boundary,ends;
        for (size_t i=0;i<t.boundary.size();++i)
        {
            const uint32_t a=t.boundary[i],b=t.boundary[t.nextBoundary(i)]; boundary.push_back(a);
            const auto it=midpoints.find(edgeKey(a,b)); if (it!=midpoints.end()) boundary.push_back(it->second);
            if (!t.loopEnds.empty() && t.nextBoundary(i)<=i) ends.push_back(static_cast<uint32_t>(boundary.size()));
        }
        std::vector<std::array<uint32_t,3>> triangles;
        for (const auto &tri:t.triangles)
        {
            uint32_t m[3]{}; bool split[3]{}; unsigned count=0;
            for (unsigned e=0;e<3;++e)
            {
                const auto it=midpoints.find(edgeKey(tri[e],tri[(e+1)%3]));
                if (it!=midpoints.end()) { split[e]=true; m[e]=it->second; ++count; }
            }
            if (count==0) triangles.push_back(tri);
            else if (count==3)
            {
                triangles.push_back({tri[0],m[0],m[2]}); triangles.push_back({m[0],tri[1],m[1]});
                triangles.push_back({m[2],m[1],tri[2]}); triangles.push_back({m[0],m[1],m[2]});
            }
            else
            {
                unsigned e=0;
                while (!(split[e] && (count==1 || split[(e+1)%3]))) ++e;
                const uint32_t a=tri[e],b=tri[(e+1)%3],c=tri[(e+2)%3],ab=m[e];
                if (count==1) { triangles.push_back({a,ab,c}); triangles.push_back({ab,b,c}); }
                else
                {
                    const uint32_t bc=m[(e+1)%3]; triangles.push_back({b,bc,ab});
                    triangles.push_back({a,ab,c}); triangles.push_back({ab,bc,c});
                }
            }
        }
        t.boundary=std::move(boundary); t.loopEnds=std::move(ends); t.triangles=std::move(triangles);
    }
    bool needsPaintDetail(const std::array<uint32_t,3> &tri,const TOPOLOGY &t,
                          const HEIGHT_FIELD &field,const IMAGE_MESH_OPTIONS &o)
    {
        const auto &a=t.points[tri[0]],&b=t.points[tri[1]],&c=t.points[tri[2]];
        const float minX=std::min({a.x,b.x,c.x}),maxX=std::max({a.x,b.x,c.x});
        const float minY=std::min({a.y,b.y,c.y}),maxY=std::max({a.y,b.y,c.y});
        if (!field.paintTouches(minX,minY,maxX,maxY)) return false;
        const double area=cross(a,b,c);
        const float ha=surface(a,field,o,t),hb=surface(b,field,o,t),hc=surface(c,field,o,t);
        const float tolerance=std::min(o.heightTolerance,0.02f);
        const auto check=[&](const IMAGE_MESH_POINT &p)
        {
            if (!field.paintAt(p.x,p.y)) return false;
            const double wa=cross(b,c,p)/area,wb=cross(c,a,p)/area,wc=1-wa-wb;
            return wa>=-epsilon && wb>=-epsilon && wc>=-epsilon &&
                std::abs(surface(p,field,o,t)-(wa*ha+wb*hb+wc*hc))>tolerance;
        };
        if (check({(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3}) ||
            check({(a.x+b.x)/2,(a.y+b.y)/2}) || check({(b.x+c.x)/2,(b.y+c.y)/2}) ||
            check({(c.x+a.x)/2,(c.y+a.y)/2})) return true;
        const uint32_t nx=std::max(1u,field.width-1)*2,ny=std::max(1u,field.height-1)*2;
        const uint32_t x0=std::max(field.paintMinX*2,static_cast<uint32_t>(std::ceil(minX*nx)));
        const uint32_t x1=std::min(field.paintMaxX*2,static_cast<uint32_t>(std::floor(maxX*nx)));
        const uint32_t y0=std::max(field.paintMinY*2,static_cast<uint32_t>(std::ceil(minY*ny)));
        const uint32_t y1=std::min(field.paintMaxY*2,static_cast<uint32_t>(std::floor(maxY*ny)));
        for (uint32_t y=y0;y<=y1;++y) for (uint32_t x=x0;x<=x1;++x)
        {
            if (x==x0) checkpoint(o,"refinement",0.8f);
            if (check({static_cast<float>(x)/nx,static_cast<float>(y)/ny})) return true;
        }
        return false;
    }
    bool refinePainting(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t,const HEIGHT_FIELD &field,std::string &error)
    {
        if (!field.hasPainting() || o.relief==0) return true;
        const auto length=[&](uint32_t a,uint32_t b)
        {
            const double x=(t.points[a].x-t.points[b].x)*std::max(1u,field.width-1);
            const double y=(t.points[a].y-t.points[b].y)*std::max(1u,field.height-1);
            return x*x+y*y;
        };
        // Run AFTER alignment/flips so later optimizations cannot erase the painted correction.
        for (unsigned pass=0;pass<48;++pass)
        {
            std::map<uint64_t,uint32_t> midpoints;
            for (const auto &tri:t.triangles)
            {
                checkpoint(o,"refinement",0.8f+0.08f*pass/48);
                if (!needsPaintDetail(tri,t,field,o)) continue;
                unsigned longest=0;
                for (unsigned e=1;e<3;++e)
                    if (length(tri[e],tri[(e+1)%3])>length(tri[longest],tri[(longest+1)%3])) longest=e;
                const uint32_t a=tri[longest],b=tri[(longest+1)%3];
                if (length(a,b)<=0.00390625) continue; // 1/16-pixel floor for narrow remapped transitions
                const uint64_t key=edgeKey(a,b);
                if (midpoints.count(key)) continue;
                const auto pa=t.points[a],pb=t.points[b];
                midpoints[key]=static_cast<uint32_t>(t.points.size());
                t.points.push_back({(pa.x+pb.x)*0.5f,(pa.y+pb.y)*0.5f});
                if (!budget(o,t,"during painted height refinement",error)) return false;
            }
            if (midpoints.empty()) return true;
            splitEdges(t,midpoints);
            if (!budget(o,t,"during painted height refinement",error)) return false;
        }
        error="Painted height refinement did not converge"; return false;
    }
    bool finishAdaptive(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t,const HEIGHT_FIELD &field,std::string &error)
    {
        const float half=o.twoLevels?o.grooveTransition*0.5f:0.0f;
        if (!alignTransition(o,t,field,o.grooveThreshold-half,error)) return false;
        if (o.twoLevels && !alignTransition(o,t,field,o.grooveThreshold+half,error)) return false;
        if (!o.twoLevels && (!alignTransition(o,t,field,0.0001f,error) || !alignTransition(o,t,field,0.9999f,error))) return false;
        improveRelief(o,t,field);
        if (!refinePainting(o,t,field,error)) return false;
        if (!o.backOpen && o.holeCount==0 && !triangulateBoundary(t,t.backTriangles)) { error="Cannot triangulate simplified back"; return false; }
        return budget(o,t,"after groove alignment",error);
    }

}

bool buildTopology(const IMAGE_MESH_OPTIONS &options, TOPOLOGY &t, std::string &error, const HEIGHT_FIELD *field)
{
    // Manual heights must not inherit hidden image-detection thresholds.
    IMAGE_MESH_OPTIONS o=options;
    if (o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL)
    {
        o.twoLevels=false;
        o.grooveThreshold=0.5f;
        o.grooveTransition=0.1f;
    }
    const auto fail=[&](const char *message) { error=message; return false; };
    if (o.shape==IMAGE_MESH_SHAPE::RECTANGLE && !field && o.holeCount==0)
    {
        const uint64_t size=static_cast<uint64_t>(o.columns+1)*(o.rows+1);
        if (!budget(o,(o.backOpen?size:2*size)+8*(o.columns+o.rows),
                    (o.backOpen?2ull:4ull)*o.columns*o.rows+4*(o.columns+o.rows),
                    "for rectangular grid",error)) return false;
        for (uint32_t r=0;r<=o.rows;++r) for (uint32_t c=0;c<=o.columns;++c)
            t.points.push_back({static_cast<float>(c)/o.columns,static_cast<float>(r)/o.rows});
        for (uint32_t r=0;r<o.rows;++r) for (uint32_t c=0;c<o.columns;++c)
        {
            const uint32_t a=r*(o.columns+1)+c,b=a+1,d=a+o.columns+1;
            t.triangles.push_back({a,b,d}); t.triangles.push_back({b,d+1,d});
        }
        for (uint32_t c=0;c<o.columns;++c) t.boundary.push_back(c);
        for (uint32_t r=0;r<o.rows;++r) t.boundary.push_back(r*(o.columns+1)+o.columns);
        for (uint32_t c=o.columns;c>0;--c) t.boundary.push_back(o.rows*(o.columns+1)+c);
        for (uint32_t r=o.rows;r>0;--r) t.boundary.push_back(r*(o.columns+1));
        t.contour={{0,0},{1,0},{1,1},{0,1}};
        return true;
    }
    if (o.shape==IMAGE_MESH_SHAPE::ELLIPSE)
    {
        if (o.ellipseSegments<8 || o.ellipseSegments>128) return fail("Ellipse segments must be in [8,128]");
        for (uint32_t i=0;i<o.ellipseSegments;++i)
        {
            const double angle=6.283185307179586*i/o.ellipseSegments;
            t.points.push_back({static_cast<float>(0.5+0.5*std::cos(angle)),static_cast<float>(0.5+0.5*std::sin(angle))});
        }
    }
    else if (o.shape==IMAGE_MESH_SHAPE::RECTANGLE) t.points={{0,0},{1,0},{1,1},{0,1}};
    else if (o.shape==IMAGE_MESH_SHAPE::POLYGON)
    {
        if (!o.contour || o.contourCount<3 || o.contourCount>128) return fail("Polygon needs 3..128 points");
        t.points.assign(o.contour,o.contour+o.contourCount);
    }
    else return fail("Unsupported shape");
    for (const auto &p:t.points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.y<0 || p.x>1 || p.y>1)
            return fail("Contour points must be finite normalized crop coordinates in [0,1]");
    for (size_t i=0;i<t.points.size();++i)
    {
        const size_t next=(i+1)%t.points.size();
        const auto &a=t.points[i], &b=t.points[next];
        if (std::hypot(a.x-b.x,a.y-b.y)<1e-6) return fail("Repeated or too close contour points");
        for (size_t j=i+1;j<t.points.size();++j)
        {
            const size_t after=(j+1)%t.points.size();
            if (j==next || after==i) continue;
            if (intersects(a,b,t.points[j],t.points[after])) return fail("Contour intersects or touches itself");
        }
    }
    // Collinear forward points are redundant; backtracking is invalid.
    bool removed=true;
    while (removed && t.points.size()>3)
    {
        removed=false;
        for (size_t i=0;i<t.points.size();++i)
        {
            const auto &a=t.points[(i+t.points.size()-1)%t.points.size()], &b=t.points[i], &c=t.points[(i+1)%t.points.size()];
            if (std::abs(cross(a,b,c))>epsilon) continue;
            if (!onSegment(a,c,b)) return fail("Contour backtracks along an edge");
            t.points.erase(t.points.begin()+static_cast<std::ptrdiff_t>(i)); removed=true; break;
        }
    }
    double area=0;
    for (size_t i=0;i<t.points.size();++i)
    {
        const auto &a=t.points[i], &b=t.points[(i+1)%t.points.size()];
        area+=static_cast<double>(a.x)*b.y-static_cast<double>(a.y)*b.x;
    }
    if (std::abs(area)<1e-8) return fail("Contour has zero or negligible area");
    if (area<0) std::reverse(t.points.begin(),t.points.end());
    t.contour=t.points;
    t.boundary.resize(t.points.size()); std::iota(t.boundary.begin(),t.boundary.end(),0u);
    if (!addHoles(o,t,error)) return false;
    if (o.shape==IMAGE_MESH_SHAPE::ELLIPSE && o.holeCount==0)
    {
        const uint32_t center=static_cast<uint32_t>(t.points.size());
        t.points.push_back({0.5f,0.5f});
        for (uint32_t i=0;i<center;++i)
            t.triangles.push_back({center,i,(i+1)%center});
    }
    else
    {
        if (!triangulateBoundary(t,t.triangles)) return fail("Cannot triangulate contour without degenerate triangles");
    }
    if (!budget(o,t,"by contour",error)) return false;
    // Shared midpoint splits preserve conformity, including on the perimeter.
    for (unsigned pass=0;pass<20;++pass)
    {
        checkpoint(o,"topology",0.26f+0.2f*pass/20);
        if (field) improveTriangles(o,t);
        std::map<uint64_t,uint32_t> midpoints;
        for (const auto &tri:t.triangles)
        {
            checkpoint(o,"topology",0.26f+0.2f*pass/20);
            unsigned longest=0;
            if (field)
            {
                for (unsigned e=1;e<3;++e)
                    if (edgeLength(t.points[tri[e]],t.points[tri[(e+1)%3]],o)>
                        edgeLength(t.points[tri[longest]],t.points[tri[(longest+1)%3]],o)) longest=e;
                if (edgeLength(t.points[tri[longest]],t.points[tri[(longest+1)%3]],o)<=2.000001 || !needsDetail(tri,t,*field,o)) continue;
            }
            for (unsigned e=0;e<3;++e)
            {
                if (field && e!=longest) continue;
                const uint32_t a=tri[e],b=tri[(e+1)%3];
                const auto pa=t.points[a],pb=t.points[b];
                if (edgeLength(pa,pb,o)<=2.000001) continue;
                const uint64_t key=edgeKey(a,b); if (midpoints.count(key)) continue;
                midpoints[key]=static_cast<uint32_t>(t.points.size());
                t.points.push_back({(pa.x+pb.x)*0.5f,(pa.y+pb.y)*0.5f});
                if (!budget(o,t,"during contour refinement",error)) return false;
            }
        }
        if (midpoints.empty()) return field?finishAdaptive(o,t,*field,error):true;
        splitEdges(t,midpoints);
        if (!budget(o,t,"during contour refinement",error)) return false;
    }
    return fail("Contour refinement did not converge");
}

float borderDistance(const IMAGE_MESH_POINT &p, const TOPOLOGY &t)
{
    double nearest=1;
    // Holes cut the height field; only the outer perimeter tapers its relief.
    const auto &contour=t.contour;
    for (size_t i=0;i<contour.size();++i)
    {
        const auto &a=contour[i], &b=contour[(i+1)%contour.size()];
        const double dx=static_cast<double>(b.x)-a.x,dy=static_cast<double>(b.y)-a.y;
        const double u=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy),0.0,1.0);
        nearest=std::min(nearest,std::hypot(p.x-a.x-u*dx,p.y-a.y-u*dy));
    }
    return static_cast<float>(nearest);
}
} }
