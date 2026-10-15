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

#ifndef MBM_IMAGE_MESH_BEVEL_H
#define MBM_IMAGE_MESH_BEVEL_H
#include <map>
namespace mbm { namespace image_mesh { namespace bevel {
struct MAPPING
{
    unsigned group=0;
    float frontRelief=0;
    bool hole=false;
};
// All loops keep material on their right in engine XY. Insetting therefore
// shrinks outer contours and expands holes with exactly the same operation.
struct RINGS
{
    std::vector<std::vector<SIDE_POINT>> loops,inner;
    TOPOLOGY core;
    double maximum=0;
};
inline bool inset(const std::vector<std::vector<SIDE_POINT>> &loops,double width,
                  std::vector<std::vector<SIDE_POINT>> &result)
{
    result.clear();
    for (const auto &loop:loops)
    {
        std::vector<SIDE_POINT> ring;
        for (size_t i=0;i<loop.size();++i)
        {
            const auto &a=loop[(i+loop.size()-1)%loop.size()],&b=loop[i],&c=loop[(i+1)%loop.size()];
            const double l=std::hypot(b.x-a.x,b.y-a.y),m=std::hypot(c.x-b.x,c.y-b.y);
            if (l<1e-9 || m<1e-9) return false;
            const double ax=-(b.y-a.y)/l,ay=(b.x-a.x)/l,bx=-(c.y-b.y)/m,by=(c.x-b.x)/m;
            const double denominator=1+ax*bx+ay*by;
            if (denominator<1e-8) return false;
            ring.push_back({b.x-width*(ax+bx)/denominator,b.y-width*(ay+by)/denominator});
        }
        for (size_t i=0;i<ring.size();++i)
        {
            size_t j=(i+1)%ring.size();
            if ((ring[j].x-ring[i].x)*(loop[j].x-loop[i].x)+(ring[j].y-ring[i].y)*(loop[j].y-loop[i].y)<=1e-8) return false;
            for (size_t k=i+1;k<ring.size();++k)
            {
                size_t l=(k+1)%ring.size();
                if (k!=j && l!=i && sideHit(ring[i],ring[j],ring[k],ring[l])) return false;
            }
        }
        result.push_back(std::move(ring));
    }
    for (size_t a=0;a<result.size();++a) for (size_t b=a+1;b<result.size();++b)
    {
        if (a==0 && !sideInside(result[0],result[b][0])) return false;
        if (a>0 && (sideInside(result[a],result[b][0]) || sideInside(result[b],result[a][0]))) return false;
        for (size_t i=0;i<result[a].size();++i) for (size_t j=0;j<result[b].size();++j)
            if (sideHit(result[a][i],result[a][(i+1)%result[a].size()],result[b][j],result[b][(j+1)%result[b].size()])) return false;
    }
    return true;
}
inline bool prepare(const IMAGE_MESH_OPTIONS &o,const TOPOLOGY &source,const HEIGHT_FIELD &field,
                    RINGS &rings,std::string &error)
{
    const auto fail=[&](const char *s) { error=s;return false; };
    if (o.voxelized || o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::CURVED || o.backOpen || o.backRelief)
        return fail("Bevel requires a closed extrusion with flat back");
    if (!std::isfinite(o.bevelWidth) || o.bevelWidth<.001f || o.bevelWidth>1000000)
        return fail("Bevel width must be finite and in [0.001,1000000] mesh units");
    if (o.bevelRounded && (o.bevelSegments<2 || o.bevelSegments>8)) return fail("Rounded bevel segments must be in [2,8]");
    const auto add=[&](const std::vector<IMAGE_MESH_POINT> &loop) {
        std::vector<SIDE_POINT> points;
        for (auto p:loop) points.push_back({(p.x-.5)*o.width,(.5-p.y)*o.height});
        rings.loops.push_back(std::move(points));
    };
    add(source.contour);for (const auto &h:source.holes) add(h);
    for (const auto &loop:rings.loops)
        if (loop.size()<3 || loop.size()>128) return fail("Bevel supports contours with 3..128 corners");
    // A conservative thickness bound is valid even between sampled boundary points.
    const unsigned faces=static_cast<unsigned>(o.bevelFront)+o.bevelBack;
    double upper=std::min(o.width,o.height)*.5;
    if (faces) upper=std::min(upper,static_cast<double>(o.depth)/faces);
    if (!o.lockBorder && o.heightSource==IMAGE_MESH_HEIGHT_SOURCE::MANUAL && !o.heightEditCount && !o.heightAreaCount)
        if (faces) upper=std::min(static_cast<double>(std::min(o.width,o.height))*.5,static_cast<double>(o.depth+field.surface(.5f,.5f,o)*o.relief)/faces);
    double lower=0;
    std::vector<std::vector<SIDE_POINT>> trial;
    for (unsigned i=0;i<28;++i)
    {
        checkpoint(o,"bevel",.3f);
        const double mid=(lower+upper)*.5;
        if (inset(rings.loops,mid,trial)) lower=mid;else upper=mid;
    }
    rings.maximum=lower*(1-1e-5);
    if (o.bevelWidth>rings.maximum)
    {
        char message[160];std::snprintf(message,sizeof(message),"Bevel width exceeds the safe limit: %.6g",rings.maximum);
        error=message;return false;
    }
    if (!inset(rings.loops,o.bevelWidth,rings.inner)) return fail("Invalid bevel corner");
    std::vector<std::vector<IMAGE_MESH_POINT>> normalized;
    for (const auto &loop:rings.inner)
    {
        std::vector<IMAGE_MESH_POINT> points;
        for (const auto &p:loop) points.push_back({static_cast<float>(p.x/o.width+.5),static_cast<float>(.5-p.y/o.height)});
        normalized.push_back(std::move(points));
    }
    std::vector<IMAGE_MESH_HOLE> holes;
    for (size_t i=1;i<normalized.size();++i) holes.push_back({normalized[i].data(),static_cast<uint32_t>(normalized[i].size())});
    IMAGE_MESH_OPTIONS core=o;core.bevel=false;core.shape=IMAGE_MESH_SHAPE::POLYGON;
    core.contour=normalized[0].data();core.contourCount=static_cast<uint32_t>(normalized[0].size());
    core.holes=holes.data();core.holeCount=static_cast<uint32_t>(holes.size());
    if (!buildTopology(core,rings.core,error,o.followImage?&field:nullptr)) return false;
    if (o.sideMode==IMAGE_MESH_SIDE::REPEAT) sideSplitRepeats(o,rings.core);
    return true;
}

template<class V,class UV> bool applyRings(const IMAGE_MESH_OPTIONS &o,const RINGS &rings,const HEIGHT_FIELD &field,
    std::vector<V> &vertices,std::vector<uint16_t> &indices,uint32_t &frontCount,uint32_t &backCount,
    uint32_t &backStart,uint32_t &sideStart,uint32_t &sideVertices,const UV &uv,std::string &error)
{
    struct TRI { V a,b,c;unsigned group; };
    std::vector<TRI> triangles;
    bool exceeded=false;
    const auto &core=rings.core;
    const auto make=[&](double x,double y,double z,unsigned group) {
        V v;v.position=VEC3(static_cast<float>(x),static_cast<float>(y),static_cast<float>(z));
        v.normal=VEC3(0,0,0);MAPPING p{group};
        v.uv=uv(v.position,p);return v;
    };
    const auto tri=[&](V a,V b,V c,unsigned group,bool preserveNormals=false) {
        const double x=b.position.x-a.position.x,y=b.position.y-a.position.y,z=b.position.z-a.position.z;
        const double u=c.position.x-a.position.x,v=c.position.y-a.position.y,w=c.position.z-a.position.z;
        const double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u,length=std::sqrt(nx*nx+ny*ny+nz*nz);
        if (length<1e-12) return;
        if (!preserveNormals && (!o.bevelRounded || (a.normal.x==0 && a.normal.y==0 && a.normal.z==0)))
            a.normal=b.normal=c.normal=VEC3(static_cast<float>(nx/length),static_cast<float>(ny/length),static_cast<float>(nz/length));
        if (triangles.size()>=o.maxTriangles) { exceeded=true;return; }
        triangles.push_back({a,b,c,group});
    };
    std::vector<bool> boundary(core.points.size(),false);
    for (auto i:core.boundary) boundary[i]=true;
    std::vector<V> front,back;
    for (size_t i=0;i<core.points.size();++i)
    {
        const auto &p=core.points[i];float height=field.surface(p.x,p.y,o)*o.relief;
        if (o.lockBorder)
        {
            if (boundary[i]) height=0;
            else if (o.borderWidth>0) height*=std::min(1.0f,borderDistance(p,core)/o.borderWidth);
        }
        front.push_back(make((p.x-.5f)*o.width,(.5f-p.y)*o.height,-o.depth*.5-height,0));
        back.push_back(make(front.back().position.x,front.back().position.y,o.depth*.5,1));
    }
    // Keep the relief cap's area-weighted smooth shading independent of bevel style.
    for (const auto &t:core.triangles)
    {
        const auto &a=front[t[0]].position,&b=front[t[1]].position,&c=front[t[2]].position;
        const VEC3 n((b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),
                     (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),
                     (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x));
        for (auto i:t) { front[i].normal.x+=n.x;front[i].normal.y+=n.y;front[i].normal.z+=n.z; }
    }
    for (size_t i=0;i<front.size();++i)
    {
        auto &n=front[i].normal;const double length=std::sqrt(static_cast<double>(n.x)*n.x+static_cast<double>(n.y)*n.y+static_cast<double>(n.z)*n.z);
        if (length>0) n=VEC3(static_cast<float>(n.x/length),static_cast<float>(n.y/length),static_cast<float>(n.z/length));
        back[i].normal=VEC3(0,0,1);
    }
    for (const auto &t:core.triangles)
    { tri(front[t[0]],front[t[1]],front[t[2]],0,true);tri(back[t[2]],back[t[1]],back[t[0]],1,true); }
    double perimeter=0,walked=0;
    for (size_t i=0;i<core.boundary.size();++i)
    {
        const auto &a=front[core.boundary[i]].position,&b=front[core.boundary[core.nextBoundary(i)]].position;
        perimeter+=std::hypot(b.x-a.x,b.y-a.y);
    }
    size_t first=0;
    for (size_t loop=0;loop<rings.inner.size();++loop)
    {
        checkpoint(o,"bevel",.96f);
        const size_t end=core.loopEnds.empty()?core.boundary.size():core.loopEnds[loop];
        const auto &inner=rings.inner[loop],&outer=rings.loops[loop];
        struct SAMPLE { uint32_t id;double dx,dy,u=0,corner=0; };
        std::vector<SAMPLE> samples;
        for (size_t i=first;i<end;++i)
        {
            const uint32_t id=core.boundary[i];const auto &p=front[id].position;
            const auto &nextPoint=front[core.boundary[core.nextBoundary(i)]].position;
            const double repeat=walked/perimeter*o.sideRepeatU;
            walked+=std::hypot(nextPoint.x-p.x,nextPoint.y-p.y);
            size_t edge=0;double best=1e300,t=0;
            for (size_t j=0;j<inner.size();++j)
            {
                const auto &a=inner[j],&b=inner[(j+1)%inner.size()];
                const double dx=b.x-a.x,dy=b.y-a.y;
                const double q=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy),0.0,1.0);
                const double d=std::hypot(p.x-a.x-q*dx,p.y-a.y-q*dy);
                if (d<best) { best=d;edge=j;t=q; }
            }
            size_t corner=inner.size();
            const double eps=std::max(o.width,o.height)*2e-6;
            for (size_t j=0;j<inner.size();++j)
                if (std::hypot(p.x-inner[j].x,p.y-inner[j].y)<eps) { corner=j;break; }
            if (corner<inner.size() && o.bevelCorners)
            {
                const auto &a=outer[(corner+outer.size()-1)%outer.size()],&b=outer[corner],&c=outer[(corner+1)%outer.size()];
                const double l=std::hypot(b.x-a.x,b.y-a.y),m=std::hypot(c.x-b.x,c.y-b.y);
                const double ax=-(b.y-a.y)/l,ay=(b.x-a.x)/l,bx=-(c.y-b.y)/m,by=(c.x-b.x)/m;
                const double turn=std::atan2(ax*by-ay*bx,ax*bx+ay*by);
                if (turn< -1e-7)
                {
                    const unsigned count=o.bevelRounded?o.bevelSegments:1;
                    const double start=std::atan2(ay,ax);
                    for (unsigned s=0;s<=count;++s)
                        samples.push_back({id,o.bevelWidth*std::cos(start+turn*s/count),o.bevelWidth*std::sin(start+turn*s/count),repeat,static_cast<double>(s)/count});
                    continue;
                }
            }
            const auto &a=outer[edge],&b=outer[(edge+1)%outer.size()];
            samples.push_back({id,a.x+(b.x-a.x)*t-p.x,a.y+(b.y-a.y)*t-p.y,repeat,0});
        }
        const unsigned steps=o.bevelRounded?o.bevelSegments:1;
        const auto ring=[&](unsigned face,unsigned step) {
            std::vector<V> out;
            const bool enabled=face==0?o.bevelFront:o.bevelBack;
            const double angle=1.5707963267948966*step/steps;
            const double horizontal=o.bevelRounded?std::sin(angle):static_cast<double>(step)/steps;
            const double vertical=o.bevelRounded?1-std::cos(angle):horizontal;
            for (const auto &s:samples)
            {
                V v=face==0?front[s.id]:back[s.id];
                if (step)
                {
                    v=make(v.position.x+s.dx*horizontal,v.position.y+s.dy*horizontal,
                        v.position.z+(face==0?1:-1)*(enabled?o.bevelWidth*vertical:0),face);
                }
                if (o.bevelRounded)
                {
                    const double length=std::hypot(s.dx,s.dy),h=enabled?std::sin(angle):0;
                    v.normal=VEC3(static_cast<float>(s.dx/length*h),static_cast<float>(s.dy/length*h),static_cast<float>((face==0?-1:1)*(enabled?std::cos(angle):1)));
                }
                out.push_back(v);
            }
            return out;
        };
        std::vector<V> wall[2];
        for (unsigned face=0;face<2;++face)
        {
            auto previous=ring(face,0);
            for (unsigned step=1;step<=steps;++step)
            {
                auto next=ring(face,step);
                for (size_t i=0;i<samples.size();++i)
                {
                    size_t j=(i+1)%samples.size();
                    if (face==0) { tri(previous[i],next[i],next[j],face);tri(previous[i],next[j],previous[j],face); }
                    else { tri(previous[i],next[j],next[i],face);tri(previous[i],previous[j],next[j],face); }
                }
                previous.swap(next);
            }
            wall[face]=std::move(previous);
        }
        for (size_t i=0;i<samples.size();++i)
        {
            size_t j=(i+1)%samples.size();
            V a=wall[0][i],b=wall[1][i],c=wall[1][j],d=wall[0][j];
            for (V *v:{&a,&b,&c,&d}) { MAPPING p{2};p.hole=loop>0;p.frontRelief=-front[(v==&a || v==&b)?samples[i].id:samples[j].id].position.z-o.depth*.5f;v->uv=uv(v->position,p);v->normal=VEC3(0,0,0); }
            if (o.bevelRounded && o.bevelCorners && samples[i].id==samples[j].id)
            {
                const auto normal=[](const SAMPLE &s) { const double l=std::hypot(s.dx,s.dy);return VEC3(static_cast<float>(s.dx/l),static_cast<float>(s.dy/l),0); };
                a.normal=b.normal=normal(samples[i]);c.normal=d.normal=normal(samples[j]);
            }
            if (o.sideMode!=IMAGE_MESH_SIDE::REPEAT) { tri(a,b,c,2);tri(a,c,d,2);continue; }
            const bool corner=samples[i].id==samples[j].id;
            const double u0=corner?samples[i].corner:samples[i].u;
            const double u1=corner?samples[j].corner:(j==0?walked/perimeter*o.sideRepeatU:samples[j].u);
            const double base=std::floor(u0+1e-5);
            const auto tile=[&](double u,double v) {
                if (o.sideTexture && *o.sideTexture) return VEC2(static_cast<float>(u),static_cast<float>(v));
                return VEC2(static_cast<float>((o.x+u*(field.width-1)+.5)/field.imageWidth),
                            static_cast<float>((o.y+v*(field.height-1)+.5)/field.imageHeight));
            };
            const auto at=[](const V &a,const V &b,double t) {
                if (t==0) return a;
                if (t==1) return b;
                V v=a;v.position.z=static_cast<float>(a.position.z+(b.position.z-a.position.z)*t);return v;
            };
            for (unsigned row=0;row<static_cast<unsigned>(std::ceil(o.sideRepeatV));++row)
            {
                const double v0=row,v1=std::min(v0+1,static_cast<double>(o.sideRepeatV));
                V aa=at(a,b,v0/o.sideRepeatV),bb=at(a,b,v1/o.sideRepeatV);
                V cc=at(d,c,v1/o.sideRepeatV),dd=at(d,c,v0/o.sideRepeatV);
                aa.uv=tile(std::clamp(u0-base,0.0,1.0),0);bb.uv=tile(std::clamp(u0-base,0.0,1.0),v1-v0);
                cc.uv=tile(std::clamp(u1-base,0.0,1.0),v1-v0);dd.uv=tile(std::clamp(u1-base,0.0,1.0),0);
                tri(aa,bb,cc,2);tri(aa,cc,dd,2);
            }
        }
        first=end;
    }
    if (exceeded) { error="Geometry budget exceeded by bevel triangles";return false; }
    vertices.clear();indices.clear();
    for (unsigned group=0;group<3;++group)
    {
        if (group==1) { frontCount=static_cast<uint32_t>(vertices.size());backStart=static_cast<uint32_t>(indices.size()); }
        if (group==2) { backCount=static_cast<uint32_t>(vertices.size())-frontCount;sideVertices=static_cast<uint32_t>(vertices.size());sideStart=static_cast<uint32_t>(indices.size()); }
        std::map<std::array<float,8>,uint32_t> weld;
        for (const auto &t:triangles) if (t.group==group) for (const V *v:{&t.a,&t.b,&t.c})
        {
            const auto &p=v->position,&n=v->normal;const auto &u=v->uv;
            auto e=weld.emplace(std::array<float,8>{p.x,p.y,p.z,n.x,n.y,n.z,u.x,u.y},static_cast<uint32_t>(vertices.size()));
            if (e.second) vertices.push_back(*v);
            if (vertices.size()>std::min(o.maxVertices,65535u)) { error="Geometry budget exceeded by bevel vertices";return false; }
            indices.push_back(static_cast<uint16_t>(e.first->second));
        }
    }
    return true;
}
} } }
#endif
