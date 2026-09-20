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
        if (o.followImage)
            return budget(o,t.points.size()+5*t.boundary.size(),t.triangles.size()+3*t.boundary.size()-2,stage,error);
        return budget(o,2*t.points.size()+4*t.boundary.size(),
                      2*t.triangles.size()+2*t.boundary.size(),stage,error);
    }
    uint64_t edgeKey(uint32_t a, uint32_t b)
    {
        return (static_cast<uint64_t>(std::min(a,b))<<32) | std::max(a,b);
    }
    bool triangulateBoundary(const TOPOLOGY &t,std::vector<std::array<uint32_t,3>> &output)
    {
        auto remaining=t.boundary;
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
        float value=field.mapped(field.sample(p.x,p.y),o);
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
    void improveTriangles(TOPOLOGY &t)
    {
        for (unsigned pass=0;pass<3;++pass)
        {
            std::map<uint64_t,std::pair<size_t,uint32_t>> neighbors;
            std::vector<bool> changed(t.triangles.size(),false);
            bool any=false;
            for (size_t i=0;i<t.triangles.size();++i)
            {
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
                const float f=(lo+hi)*0.5f,v=field.sample(pa.x+(pb.x-pa.x)*f,pa.y+(pb.y-pa.y)*f);
                if ((v<iso)==(va<iso)) lo=f; else hi=f;
            }
            const float f=(lo+hi)*0.5f;
            const uint32_t id=static_cast<uint32_t>(t.points.size());
            t.points.push_back({pa.x+(pb.x-pa.x)*f,pa.y+(pb.y-pa.y)*f}); cuts[key]=id; return id;
        };
        std::vector<std::array<uint32_t,3>> result;
        for (const auto &tri:t.triangles)
        {
            float v[3]; for (unsigned i=0;i<3;++i) v[i]=field.sample(t.points[tri[i]].x,t.points[tri[i]].y);
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
        std::vector<uint32_t> boundary;
        for (size_t i=0;i<t.boundary.size();++i)
        {
            const auto a=t.boundary[i],b=t.boundary[(i+1)%t.boundary.size()]; boundary.push_back(a);
            auto it=cuts.find(edgeKey(a,b)); if (it!=cuts.end()) boundary.push_back(it->second);
        }
        t.boundary=std::move(boundary); t.triangles=std::move(result);
        return budget(o,t,"while aligning grooves",error);
    }
    bool finishAdaptive(const IMAGE_MESH_OPTIONS &o,TOPOLOGY &t,const HEIGHT_FIELD &field,std::string &error)
    {
        const float half=o.twoLevels?o.grooveTransition*0.5f:0.0f;
        if (!alignTransition(o,t,field,o.grooveThreshold-half,error)) return false;
        if (o.twoLevels && !alignTransition(o,t,field,o.grooveThreshold+half,error)) return false;
        if (!triangulateBoundary(t,t.backTriangles)) { error="Cannot triangulate simplified back"; return false; }
        return budget(o,t,"after groove alignment",error);
    }

}

bool buildTopology(const IMAGE_MESH_OPTIONS &o, TOPOLOGY &t, std::string &error, const HEIGHT_FIELD *field)
{
    const auto fail=[&](const char *message) { error=message; return false; };
    if (o.shape==IMAGE_MESH_SHAPE::RECTANGLE && !field)
    {
        const uint64_t size=static_cast<uint64_t>(o.columns+1)*(o.rows+1);
        if (!budget(o,2*size+8*(o.columns+o.rows),4ull*o.columns*o.rows+4*(o.columns+o.rows),
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
    if (o.shape==IMAGE_MESH_SHAPE::ELLIPSE)
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
        if (field) improveTriangles(t);
        std::map<uint64_t,uint32_t> midpoints;
        for (const auto &tri:t.triangles)
        {
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
        std::vector<uint32_t> boundary;
        for (size_t i=0;i<t.boundary.size();++i)
        {
            const uint32_t a=t.boundary[i],b=t.boundary[(i+1)%t.boundary.size()]; boundary.push_back(a);
            const auto it=midpoints.find(edgeKey(a,b)); if (it!=midpoints.end()) boundary.push_back(it->second);
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
        t.boundary=std::move(boundary); t.triangles=std::move(triangles);
        if (!budget(o,t,"during contour refinement",error)) return false;
    }
    return fail("Contour refinement did not converge");
}

float borderDistance(const IMAGE_MESH_POINT &p, const TOPOLOGY &t)
{
    double nearest=1;
    for (size_t i=0;i<t.contour.size();++i)
    {
        const auto &a=t.contour[i], &b=t.contour[(i+1)%t.contour.size()];
        const double dx=static_cast<double>(b.x)-a.x,dy=static_cast<double>(b.y)-a.y;
        const double u=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy),0.0,1.0);
        nearest=std::min(nearest,std::hypot(p.x-a.x-u*dx,p.y-a.y-u*dy));
    }
    return static_cast<float>(nearest);
}
} }
