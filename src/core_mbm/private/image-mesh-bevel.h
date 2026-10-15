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

#ifndef IMAGE_MESH_BEVEL_H
#define IMAGE_MESH_BEVEL_H

#include <map>
#include <tuple>

// Private clipping workspace. No mesh buffers or containers escape the generator.
namespace mbm { namespace image_mesh { namespace bevel {
struct PLANE
{
    double x=0,y=0,z=0,d=0;
    unsigned group=0;
    double minAlong=0,maxAlong=1;
};
inline double dot(const PLANE &p,const VEC3 &v) { return p.x*v.x+p.y*v.y+p.z*v.z; }
inline bool planes(const IMAGE_MESH_OPTIONS &o,const TOPOLOGY &t,std::vector<PLANE> &out,std::string &error)
{
    const auto fail=[&](const char *message) { error=message;return false; };
    if (!std::isfinite(o.bevelWidth) || o.bevelWidth<.001f || o.bevelWidth>1000000)
        return fail("Bevel width must be finite and in [0.001,1000000] mesh units");
    if (o.voxelized || o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::CURVED || o.holeCount ||
        o.backOpen || o.backRelief || !o.lockBorder)
        return fail("Bevel requires a closed convex extrusion without holes, with locked border and flat back");
    if (!o.bevelFront && !o.bevelBack && !o.bevelCorners) return true;
    const auto &c=t.contour;
    if (c.size()<3 || c.size()>128) return fail("Bevel supports convex contours with 3..128 corners");
    const double width=o.bevelWidth;
    if (width*(static_cast<unsigned>(o.bevelFront)+o.bevelBack)>=o.depth)
        return fail("Bevel width is too large for the block thickness");
    std::vector<VEC3> points;
    VEC3 center(0,0,0);
    for (const auto &p:c)
    {
        points.emplace_back((p.x-.5f)*o.width,(.5f-p.y)*o.height,0);
        center.x+=points.back().x/static_cast<float>(c.size());
        center.y+=points.back().y/static_cast<float>(c.size());
    }
    std::vector<PLANE> edges;
    for (size_t i=0;i<c.size();++i)
    {
        const auto &a=points[i],&b=points[(i+1)%c.size()];
        const double dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
        if (length<=2*width) return fail("Bevel width must be smaller than half of every contour edge");
        // Contours are clockwise in engine XY, so left normals face outwards.
        PLANE edge{-dy/length,dx/length,0,0,2};edge.d=dot(edge,a);
        for (const auto &p:points)
            if (dot(edge,p)>edge.d+1e-6*std::max(o.width,o.height))
                return fail("Bevel currently requires a convex contour");
        if (edge.d-dot(edge,center)<=width) return fail("Bevel width is too large for the contour interior");
        edges.push_back(edge);
        if (o.bevelFront) out.push_back({edge.x,edge.y,-1,edge.d+o.depth*.5-width,0});
        if (o.bevelBack) out.push_back({edge.x,edge.y,1,edge.d+o.depth*.5-width,1});
    }
    if (o.bevelCorners)
        for (size_t i=0;i<c.size();++i)
        {
            const auto &a=edges[(i+c.size()-1)%c.size()],&b=edges[i];
            const double nx=a.x+b.x,ny=a.y+b.y,length=std::hypot(nx,ny);
            if (length<1e-8) return fail("Invalid bevel corner");
            PLANE p{nx/length,ny/length,0,0,2};
            p.d=dot(p,points[i])-width;
            if (p.d<=dot(p,center)) return fail("Bevel width is too large for a contour corner");
            out.push_back(p);
        }
    return true;
}

template<class V> struct FACE { std::vector<V> vertices;unsigned group=0; };
template<class V> V interpolate(const V &a,const V &b,double t)
{
    V v=a;
    const auto lerp=[&](float x,float y) { return static_cast<float>(x+(static_cast<double>(y)-x)*t); };
    v.position=VEC3(lerp(a.position.x,b.position.x),lerp(a.position.y,b.position.y),lerp(a.position.z,b.position.z));
    v.normal=VEC3(lerp(a.normal.x,b.normal.x),lerp(a.normal.y,b.normal.y),lerp(a.normal.z,b.normal.z));
    v.uv=VEC2(lerp(a.uv.x,b.uv.x),lerp(a.uv.y,b.uv.y));return v;
}

// Clip each triangle/polygon, then close the exact intersection loop. Retaining
// collinear intersection vertices avoids T junctions at the new hard normal seam.
template<class V,class UV> bool cut(std::vector<FACE<V>> &faces,const PLANE &plane,double epsilon,
                                    const UV &uv,std::string &error,const IMAGE_MESH_OPTIONS &o)
{
    using KEY=std::array<long long,3>;
    std::map<KEY,uint32_t> pointMap;
    std::vector<VEC3> points;
    const auto id=[&](V &v) {
        const auto &p=v.position;
        KEY key{std::llround(p.x/epsilon),std::llround(p.y/epsilon),std::llround(p.z/epsilon)};
        for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for (int z=-1;z<=1;++z)
        {
            const auto found=pointMap.find({key[0]+x,key[1]+y,key[2]+z});
            if (found==pointMap.end()) continue;
            const auto &q=points[found->second];
            if (std::abs(p.x-q.x)<=epsilon && std::abs(p.y-q.y)<=epsilon && std::abs(p.z-q.z)<=epsilon)
            { v.position=q;return found->second; }
        }
        auto inserted=pointMap.emplace(key,static_cast<uint32_t>(points.size()));
        if (inserted.second) points.push_back(p);
        v.position=points[inserted.first->second];return inserted.first->second;
    };
    std::map<std::pair<uint32_t,uint32_t>,unsigned> segments;
    std::vector<FACE<V>> clipped;clipped.reserve(faces.size()+1);
    bool removed=false;
    size_t processed=0;
    for (const auto &face:faces)
    {
        if (++processed%256==0) checkpoint(o,"bevel",.96f);
        FACE<V> result;result.group=face.group;
        for (size_t i=0;i<face.vertices.size();++i)
        {
            V a=face.vertices[i],b=face.vertices[(i+1)%face.vertices.size()];
            double da=dot(plane,a.position)-plane.d,db=dot(plane,b.position)-plane.d;
            if (std::abs(da)<epsilon) da=0;
            if (std::abs(db)<epsilon) db=0;
            if (da<=0) { if (da==0) id(a); result.vertices.push_back(a); }
            else removed=true;
            if ((da<0 && db>0) || (da>0 && db<0))
            {
                V v=interpolate(a,b,da/(da-db));id(v);result.vertices.push_back(v);
            }
        }
        for (size_t i=0;i<result.vertices.size() && result.vertices.size()>1;)
        {
            const auto &a=result.vertices[i].position,&b=result.vertices[(i+1)%result.vertices.size()].position;
            if (a.x==b.x && a.y==b.y && a.z==b.z) result.vertices.erase(result.vertices.begin()+static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
        if (result.vertices.size()<3) continue;
        double areaX=0,areaY=0,areaZ=0;
        for (size_t i=1;i+1<result.vertices.size();++i)
        {
            const auto &a=result.vertices[0].position,&b=result.vertices[i].position,&c=result.vertices[i+1].position;
            const double bx=static_cast<double>(b.x)-a.x,by=static_cast<double>(b.y)-a.y,bz=static_cast<double>(b.z)-a.z;
            const double cx=static_cast<double>(c.x)-a.x,cy=static_cast<double>(c.y)-a.y,cz=static_cast<double>(c.z)-a.z;
            areaX+=by*cz-bz*cy;areaY+=bz*cx-bx*cz;areaZ+=bx*cy-by*cx;
        }
        if (areaX==0 && areaY==0 && areaZ==0) continue;
        for (size_t i=0;i<result.vertices.size();++i)
        {
            auto &a=result.vertices[i],&b=result.vertices[(i+1)%result.vertices.size()];
            if (std::abs(dot(plane,a.position)-plane.d)>2*epsilon ||
                std::abs(dot(plane,b.position)-plane.d)>2*epsilon) continue;
            uint32_t ia=id(a),ib=id(b);
            if (ia!=ib) ++segments[std::minmax(ia,ib)];
        }
        clipped.push_back(std::move(result));
    }
    if (!removed) return true;
    std::map<uint32_t,std::vector<uint32_t>> neighbors;
    for (const auto &edge:segments)
        if (edge.second==1)
        {
            neighbors[edge.first.first].push_back(edge.first.second);
            neighbors[edge.first.second].push_back(edge.first.first);
        }
    if (neighbors.size()<3) { error="Bevel removed the surface or produced an empty cut";return false; }
    for (const auto &n:neighbors)
        if (n.second.size()!=2) { error="Bevel intersection is not a closed loop; reduce width or relief";return false; }
    std::vector<uint32_t> loop;
    uint32_t current=neighbors.begin()->first,previous=UINT32_MAX;
    do
    {
        loop.push_back(current);
        const auto &n=neighbors[current];
        const uint32_t next=n[0]==previous?n[1]:n[0];previous=current;current=next;
    } while (current!=loop.front() && loop.size()<=neighbors.size());
    if (loop.size()!=neighbors.size()) { error="Bevel cut has multiple loops; reduce width or relief";return false; }
    const double length=std::sqrt(plane.x*plane.x+plane.y*plane.y+plane.z*plane.z);
    const VEC3 normal(static_cast<float>(plane.x/length),static_cast<float>(plane.y/length),static_cast<float>(plane.z/length));
    // U lies horizontally in the cutting plane; V = N cross U.
    const double horizontal=std::hypot(plane.x,plane.y);
    const VEC3 u(static_cast<float>(-plane.y/horizontal),static_cast<float>(plane.x/horizontal),0);
    const VEC3 v(-normal.z*u.y,normal.z*u.x,normal.x*u.y-normal.y*u.x);
    std::vector<IMAGE_MESH_POINT> projected;
    for (uint32_t i:loop)
    {
        const auto &p=points[i];projected.push_back({p.x*u.x+p.y*u.y,p.x*v.x+p.y*v.y+p.z*v.z});
    }
    double area=0;
    for (size_t i=0;i<loop.size();++i)
    { const auto &a=projected[i],&b=projected[(i+1)%loop.size()];area+=static_cast<double>(a.x)*b.y-static_cast<double>(b.x)*a.y; }
    if (area<0) { std::reverse(projected.begin(),projected.end());std::reverse(loop.begin(),loop.end()); }
    PLANE mapping=plane;
    mapping.minAlong=1e300;mapping.maxAlong=-1e300;
    for (uint32_t index:loop)
    {
        const auto &p=points[index];const double along=-plane.y*p.x+plane.x*p.y;
        mapping.minAlong=std::min(mapping.minAlong,along);mapping.maxAlong=std::max(mapping.maxAlong,along);
    }
    FACE<V> cap;cap.group=plane.group;
    for (uint32_t index:loop)
    {
        V vertex;vertex.position=points[index];vertex.normal=normal;vertex.uv=uv(vertex.position,mapping);
        cap.vertices.push_back(vertex);
    }
    clipped.push_back(std::move(cap));
    faces.swap(clipped);return true;
}

template<class V,class UV> bool apply(const IMAGE_MESH_OPTIONS &o,const std::vector<PLANE> &planes,
    std::vector<V> &vertices,std::vector<uint16_t> &indices,uint32_t &frontCount,uint32_t &backCount,
    uint32_t &backStart,uint32_t &sideStart,uint32_t &sideVertices,const UV &uv,std::string &error)
{
    std::vector<FACE<V>> faces;faces.reserve(indices.size()/3);
    for (size_t i=0;i<indices.size();i+=3)
    {
        FACE<V> face;face.group=i<backStart?0:(i<sideStart?1:2);
        for (unsigned j=0;j<3;++j)
        {
            V vertex=vertices[indices[i+j]];
            const auto &n=vertex.normal;const double length=std::sqrt(static_cast<double>(n.x)*n.x+static_cast<double>(n.y)*n.y+static_cast<double>(n.z)*n.z);
            vertex.normal=VEC3(static_cast<float>(n.x/length),static_cast<float>(n.y/length),static_cast<float>(n.z/length));
            face.vertices.push_back(vertex);
        }
        faces.push_back(std::move(face));
    }
    const double epsilon=std::max({o.width,o.height,o.depth+o.relief})*1e-7;
    for (size_t i=0;i<planes.size();++i)
    {
        checkpoint(o,"bevel",.96f+.015f*static_cast<float>(i)/static_cast<float>(planes.size()));
        if (!cut(faces,planes[i],epsilon,uv,error,o)) return false;
    }
    vertices.clear();indices.clear();
    for (unsigned group=0;group<3;++group)
    {
        if (group==1) { frontCount=static_cast<uint32_t>(vertices.size());backStart=static_cast<uint32_t>(indices.size()); }
        if (group==2) { backCount=static_cast<uint32_t>(vertices.size())-frontCount;sideVertices=static_cast<uint32_t>(vertices.size());sideStart=static_cast<uint32_t>(indices.size()); }
        std::map<std::array<float,8>,uint32_t> welded;
        for (const auto &face:faces)
        {
            if (face.group!=group) continue;
            std::vector<uint32_t> faceIndices;
            for (const auto &v:face.vertices)
            {
                std::array<float,8> key{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y};
                auto entry=welded.emplace(key,static_cast<uint32_t>(vertices.size()));
                if (entry.second) vertices.push_back(v);
                faceIndices.push_back(entry.first->second);
            }
            if (vertices.size()>std::min(o.maxVertices,65535u)) { error="Geometry budget exceeded by bevel vertices";return false; }
            std::vector<std::array<uint32_t,3>> triangles;
            if (face.vertices.size()==3) triangles.push_back({0,1,2});
            else
            {
                VEC3 normal(0,0,0);
                for (size_t i=0;i<face.vertices.size();++i)
                {
                    const auto &a=face.vertices[i].position,&b=face.vertices[(i+1)%face.vertices.size()].position;
                    normal.x+=(a.y-b.y)*(a.z+b.z);normal.y+=(a.z-b.z)*(a.x+b.x);normal.z+=(a.x-b.x)*(a.y+b.y);
                }
                unsigned axis=0;
                if (std::abs(normal.y)>std::abs(normal.x)) axis=1;
                if (std::abs(normal.z)>(axis==0?std::abs(normal.x):std::abs(normal.y))) axis=2;
                const float component=axis==0?normal.x:(axis==1?normal.y:normal.z);
                std::vector<IMAGE_MESH_POINT> projection;
                for (const auto &v:face.vertices)
                {
                    const auto &p=v.position;
                    IMAGE_MESH_POINT point=axis==0?IMAGE_MESH_POINT{p.y,p.z}:(axis==1?IMAGE_MESH_POINT{p.z,p.x}:IMAGE_MESH_POINT{p.x,p.y});
                    if (component<0) point.y=-point.y;
                    projection.push_back(point);
                }
                if (!triangulatePolygon(projection,triangles)) { error="Cannot triangulate final bevel face";return false; }
            }
            for (const auto &tri:triangles)
            {
                const auto &a=face.vertices[tri[0]].position,&b=face.vertices[tri[1]].position,&c=face.vertices[tri[2]].position;
                const double bx=static_cast<double>(b.x)-a.x,by=static_cast<double>(b.y)-a.y,bz=static_cast<double>(b.z)-a.z;
                const double cx=static_cast<double>(c.x)-a.x,cy=static_cast<double>(c.y)-a.y,cz=static_cast<double>(c.z)-a.z;
                if (by*cz-bz*cy==0 && bz*cx-bx*cz==0 && bx*cy-by*cx==0)
                { error="Degenerate bevel triangle; change the width or contour resolution";return false; }
                for (uint32_t i:tri) indices.push_back(static_cast<uint16_t>(faceIndices[i]));
            }
            if (indices.size()/3>o.maxTriangles) { error="Geometry budget exceeded by bevel triangles";return false; }
        }
    }
    return true;
}
} } }
#endif
