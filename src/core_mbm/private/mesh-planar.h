/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/


#ifndef MBM_MESH_PLANAR_H
#define MBM_MESH_PLANAR_H
#include "mesh-simplifier.h"
#include "mesh-planar-index.h"
#include "mesh-planar-separation.h"
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <limits>

namespace mbm::mesh_simplifier::planar
{
// Corner attributes stay separate from logical topology across material seams.
struct ATTRIBUTES
{
    std::vector<VEC2> uv;
    std::vector<VEC3> normal;
    std::vector<std::array<VEC3,3>> surroundings;
    std::vector<bool> locked;
    bool exact = false;
    bool reduceBoundaries = false;
    double tolerance = 1e-7;
    float angle = 0.05f;
};
struct REPORT
{
    uint32_t accepted = 0, rejected = 0, removed = 0;
    uint32_t holes = 0, attributes = 0, topology = 0, surroundings = 0, budget = 0;
    double error = 0, uvError = 0;
    uint32_t boundaryRemoved = 0;
    bool boundaryFallback = false;
};
struct REGION_CERTIFICATE
{
    std::vector<uint32_t> faces;
    std::map<uint32_t,uint32_t> next;
};
struct REGION_CERTIFICATES
{
    std::vector<REGION_CERTIFICATE> regions;
    size_t boundaryCount = 0;
    bool overflow = false;
};
struct POINT { long double x=0,y=0,z=0; };
inline POINT point(const VEC3 &p) { return {p.x,p.y,p.z}; }
inline POINT sub(POINT a,POINT b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline POINT cross(POINT a,POINT b)
{ return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline long double dot(POINT a,POINT b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline long double orient(POINT a,POINT b,POINT c)
{ return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
inline uint64_t edge(uint32_t a,uint32_t b)
{ if (a>b) std::swap(a,b);return (static_cast<uint64_t>(a)<<32)|b; }
struct EDGE { std::vector<uint32_t> faces; };
inline bool same(POINT a,POINT b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
inline bool segment(POINT p,POINT a,POINT b)
{
    return orient(a,b,p)==0 && p.x>=std::min(a.x,b.x) && p.x<=std::max(a.x,b.x) &&
        p.y>=std::min(a.y,b.y) && p.y<=std::max(a.y,b.y);
}
inline bool intersects(POINT a,POINT b,POINT c,POINT d,long double epsilon=0)
{
    if (std::max(a.x,b.x)<std::min(c.x,d.x) || std::max(c.x,d.x)<std::min(a.x,b.x) ||
        std::max(a.y,b.y)<std::min(c.y,d.y) || std::max(c.y,d.y)<std::min(a.y,b.y)) return false;
    const auto x=orient(a,b,c),y=orient(a,b,d),z=orient(c,d,a),w=orient(c,d,b);
    // Overlapping boxes with an uncertain orientation are a conservative contact.
    if (std::min({std::abs(x),std::abs(y),std::abs(z),std::abs(w)})<=epsilon) return true;
    return (((x>0 && y<0)||(x<0 && y>0)) && ((z>0 && w<0)||(z<0 && w>0))) ||
        segment(c,a,b)||segment(d,a,b)||segment(a,c,d)||segment(b,c,d);
}

// Only indices/groups change. Positions and corner attributes retain source identity.
// All work is detached; cancellation returns false without publishing output.
inline bool runRegions(const INPUT &in,const ATTRIBUTES &attr,INPUT &out,REPORT &report,
                std::string &error,const std::function<void(float)> &progress={},
                const std::function<bool()> &cancel={},
                REGION_CERTIFICATES *certified=nullptr,const std::vector<bool> *removable=nullptr)
{
    report={};
    if (!std::isfinite(attr.tolerance) || attr.tolerance<0 || attr.tolerance>0.01)
    { error="invalid planar tolerance";return false; }
    if (!std::isfinite(attr.angle) || attr.angle<0 || attr.angle>5)
    { error="invalid planar angle";return false; }
    // Preserve the original threshold for existing callers and saved projects.
    const long double minimumDot=attr.angle==0.05f ? 0.99999962L :
        std::cos(static_cast<long double>(attr.angle)*std::acos(-1.0L)/180.0L);
    const size_t count=in.indices.size()/3;
    const auto cancelled=[&]() {
        if (!cancel || !cancel()) return false;
        error="simplification cancelled";return true;
    };
    if (cancelled()) return false;
    if (in.indices.size()%3 || in.triangleGroups.size()!=count ||
        (!attr.uv.empty() && attr.uv.size()!=in.indices.size()) ||
        (!attr.normal.empty() && attr.normal.size()!=in.indices.size()) ||
        (!attr.locked.empty() && attr.locked.size()!=in.positions.size()))
    { error="invalid planar input attributes";return false; }
    for (auto i:in.indices) if (i>=in.positions.size())
    { error="invalid planar input index";return false; }
    for (const auto &p:in.positions) if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
    { error="non-finite planar position";return false; }
    for (const auto &p:attr.uv) if (!std::isfinite(p.x)||!std::isfinite(p.y))
    { error="non-finite planar UV";return false; }
    for (const auto &p:attr.normal) if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
    { error="non-finite planar normal";return false; }
    for (const auto &triangle:attr.surroundings) for (const auto &p:triangle)
        if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
        { error="non-finite planar surroundings";return false; }
    std::map<uint64_t,EDGE> edges;
    std::vector<std::array<uint32_t,3>> faces(count);
    for (uint32_t f=0;f<count;++f)
    {
        if ((f&255)==0 && cancelled()) return false;
        for (unsigned k=0;k<3;++k) faces[f][k]=in.indices[3*f+k];
        for (unsigned k=0;k<3;++k) edges[edge(faces[f][k],faces[f][(k+1)%3])].faces.push_back(f);
    }
    std::vector<OBSTACLE_BOX> obstacleBoxes;
    obstacleBoxes.reserve(count+attr.surroundings.size());
    for (size_t i=0;i<count+attr.surroundings.size();++i)
    {
        if ((i&255)==0 && cancelled()) return false;
        const auto triangle=i<count ? std::array<VEC3,3>{in.positions[faces[i][0]],
            in.positions[faces[i][1]],in.positions[faces[i][2]]} : attr.surroundings[i-count];
        OBSTACLE_BOX box;
        box.lo=box.hi={triangle[0].x,triangle[0].y,triangle[0].z};
        for (const auto &p:triangle)
        {
            const std::array<long double,3> value{p.x,p.y,p.z};
            for (unsigned a=0;a<3;++a)
            { box.lo[a]=std::min(box.lo[a],value[a]);box.hi[a]=std::max(box.hi[a],value[a]); }
        }
        obstacleBoxes.push_back(box);
    }
    OBSTACLE_INDEX obstacleIndex;
    if (!obstacleIndex.build(obstacleBoxes,cancelled)) return false;
    std::vector<size_t> obstacleCandidates;
    std::vector<bool> visited(count,false),removed(count,false);
    std::vector<std::array<uint32_t,3>> added;
    std::vector<uint32_t> addedGroups;
    // A fixed connected-domain scale prevents distant islands inflating tolerance.
    std::vector<uint32_t> component(count,UINT32_MAX);
    std::vector<std::pair<POINT,POINT>> bounds;
    for (uint32_t start=0;start<count;++start)
    {
        if (component[start]!=UINT32_MAX) continue;
        const uint32_t id=static_cast<uint32_t>(bounds.size());
        const auto first=point(in.positions[faces[start][0]]);
        bounds.push_back({first,first});
        std::vector<uint32_t> pending{start};component[start]=id;
        for (size_t i=0;i<pending.size();++i)
        {
            if ((i&255)==0 && cancelled()) return false;
            const auto f=pending[i];
            for (unsigned k=0;k<3;++k)
            {
                const auto p=point(in.positions[faces[f][k]]);
                auto &lo=bounds[id].first;auto &hi=bounds[id].second;
                lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
                hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
                const auto &adj=edges.at(edge(faces[f][k],faces[f][(k+1)%3])).faces;
                if (adj.size()!=2) continue;
                for (auto neighbor:adj) if (component[neighbor]==UINT32_MAX &&
                    in.triangleGroups[neighbor]==in.triangleGroups[start])
                { component[neighbor]=id;pending.push_back(neighbor); }
            }
        }
    }
    for (uint32_t seed=0;seed<count;++seed)
    {
        if (cancelled()) return false;
        if (progress) progress(static_cast<float>(seed)/std::max<size_t>(count,1));
        if (visited[seed]) continue;
        const auto group=in.triangleGroups[seed];
        const POINT origin=point(in.positions[faces[seed][0]]);
        const POINT normal=cross(sub(point(in.positions[faces[seed][1]]),origin),
                                 sub(point(in.positions[faces[seed][2]]),origin));
        const long double norm=std::sqrt(dot(normal,normal));
        if (!(norm>0)) { visited[seed]=true;++report.topology;++report.rejected;continue; }
        const auto extent=sub(bounds[component[seed]].second,bounds[component[seed]].first);
        const auto scale=std::sqrt(dot(extent,extent));
        const long double tolerance=attr.exact?0:scale*attr.tolerance;
        unsigned drop=2;
        if (std::abs(normal.x)>=std::abs(normal.y) && std::abs(normal.x)>=std::abs(normal.z)) drop=0;
        else if (std::abs(normal.y)>=std::abs(normal.z)) drop=1;
        const long double direction=(drop==0?normal.x:(drop==1?-normal.y:normal.z))>0?1:-1;
        const auto project=[&](POINT p) {
            p=sub(p,origin);
            if (drop==0) return POINT{p.y,p.z*direction,0};
            if (drop==1) return POINT{p.x,p.z*direction,0};
            return POINT{p.x,p.y*direction,0};
        };
        const auto distance=[&](POINT p) { return dot(normal,sub(p,origin))/norm; };
        const auto eligible=[&](uint32_t f) {
            if (in.triangleGroups[f]!=group) return false;
            for (auto v:faces[f]) if (std::abs(distance(point(in.positions[v])))>tolerance) return false;
            const auto n=cross(sub(point(in.positions[faces[f][1]]),point(in.positions[faces[f][0]])),
                               sub(point(in.positions[faces[f][2]]),point(in.positions[faces[f][0]])));
            return dot(n,normal)>0 && dot(n,normal)>=std::sqrt(dot(n,n))*norm*minimumDot;
        };
        std::vector<uint32_t> region{seed};visited[seed]=true;
        for (size_t r=0;r<region.size();++r)
        {
            if ((r&255)==0 && cancelled()) return false;
            for (unsigned k=0;k<3;++k)
            {
                const auto &adj=edges.at(edge(faces[region[r]][k],faces[region[r]][(k+1)%3])).faces;
                if (adj.size()!=2) continue;
                for (auto f:adj) if (!visited[f] && eligible(f)) { visited[f]=true;region.push_back(f); }
            }
        }
        if (region.size()<3) continue;
        // Bounded validation/ear clipping. Hitting a budget preserves this whole region.
        uint64_t work=0;
        bool over=false;
        const auto tick=[&]() {
            ++work;
            if (work>2000000 || ((work&1023)==0 && cancelled())) over=true;
            return !over;
        };
        std::set<uint32_t> members(region.begin(),region.end()),vertices;
        std::map<uint64_t,std::vector<std::pair<uint32_t,uint32_t>>> incidence;
        std::map<uint32_t,std::map<uint32_t,uint32_t>> links;
        std::map<uint32_t,uint32_t> next;
        bool valid=true, varyingNormals=false;
        long double maxDistance=0,maxUv=0,sourceArea=0;
        std::map<uint32_t,POINT> projected;
        std::set<std::array<long double,3>> unique;
        const POINT uvA=project(point(in.positions[faces[seed][0]]));
        const POINT uvB=project(point(in.positions[faces[seed][1]]));
        const POINT uvC=project(point(in.positions[faces[seed][2]]));
        const auto area=orient(uvA,uvB,uvC);
        for (auto f:region)
        {
            if (cancelled()) return false;
            for (unsigned k=0;k<3;++k)
            {
                auto v=faces[f][k],a=faces[f][(k+1)%3],b=faces[f][(k+2)%3];
                const auto p=point(in.positions[v]);
                if (vertices.insert(v).second)
                {
                    if (!unique.insert({p.x,p.y,p.z}).second) valid=false;
                    projected[v]=project(p);
                    maxDistance=std::max(maxDistance,std::abs(distance(p)));
                }
                incidence[edge(v,a)].push_back({v,a});
                if (!links[v].emplace(a,b).second) valid=false;
                if (!attr.normal.empty())
                {
                    const auto &n=attr.normal[3*f+k],&ref=attr.normal[3*seed];
                    const bool differs=n.x!=ref.x||n.y!=ref.y||n.z!=ref.z;
                    varyingNormals=varyingNormals||differs;
                    if (dot(point(n),point(ref))<=0) valid=false;
                    // Raw normals are interpolated before fragment normalization.
                    // Certify the complete affine field, never just boundary values.
                    // The curved generator deliberately retains its stricter policy.
                    if (attr.exact && differs) valid=false;
                    const auto p2=project(p);
                    const auto w0=orient(uvB,uvC,p2)/area,w1=orient(uvC,uvA,p2)/area,w2=1-w0-w1;
                    const auto &n1=attr.normal[3*seed+1],&n2=attr.normal[3*seed+2];
                    if (ref.x+w1*(static_cast<long double>(n1.x)-ref.x)+w2*(static_cast<long double>(n2.x)-ref.x)!=n.x ||
                        ref.y+w1*(static_cast<long double>(n1.y)-ref.y)+w2*(static_cast<long double>(n2.y)-ref.y)!=n.y ||
                        ref.z+w1*(static_cast<long double>(n1.z)-ref.z)+w2*(static_cast<long double>(n2.z)-ref.z)!=n.z) valid=false;
                }
                if (!attr.uv.empty())
                {
                    const auto p2=project(p);
                    const auto w0=orient(uvB,uvC,p2)/area,w1=orient(uvC,uvA,p2)/area,w2=1-w0-w1;
                    const auto &a0=attr.uv[3*seed],&a1=attr.uv[3*seed+1],&a2=attr.uv[3*seed+2],&av=attr.uv[3*f+k];
                    maxUv=std::max({maxUv,std::abs(w0*a0.x+w1*a1.x+w2*a2.x-av.x),
                                           std::abs(w0*a0.y+w1*a1.y+w2*a2.y-av.y)});
                    if (maxUv>5e-7L) valid=false;
                }
            }
            sourceArea+=orient(projected[faces[f][0]],projected[faces[f][1]],projected[faces[f][2]]);
        }
        if (varyingNormals && maxDistance!=0) valid=false;
        if (!valid) { ++report.attributes;++report.rejected;continue; }
        for (const auto &e:incidence)
        {
            const auto &list=e.second;
            if (edges.at(e.first).faces.size()>2) valid=false;
            if (list.size()==1) { if (!next.emplace(list[0].first,list[0].second).second) valid=false; }
            else if (list.size()!=2 || list[0].first!=list[1].second || list[0].second!=list[1].first) valid=false;
        }
        // Each vertex link must be one connected path (boundary) or cycle (interior).
        for (const auto &entry:links)
        {
            const auto &link=entry.second;
            std::set<uint32_t> destinations;
            for (const auto &e:link) if (!destinations.insert(e.second).second) valid=false;
            auto start=link.begin()->first;
            for (const auto &e:link) if (!destinations.count(e.first)) { start=e.first;break; }
            auto current=start;size_t traversed=0;
            do
            {
                const auto it=link.find(current);if (it==link.end()) break;
                current=it->second;++traversed;
            } while (current!=start && traversed<=link.size());
            if (traversed!=link.size()) valid=false;
            if (!next.count(entry.first) && current!=start) valid=false;
            if (!attr.locked.empty() && attr.locked[entry.first] && !next.count(entry.first)) valid=false;
        }
        // Recover all directed loops without merging touching or repeated vertices.
        std::vector<std::vector<uint32_t>> loops;
        std::set<uint32_t> boundaryVisited;
        for (const auto &entry:next)
        {
            if (!valid || boundaryVisited.count(entry.first)) continue;
            std::vector<uint32_t> loop;
            auto current=entry.first;
            do
            {
                if (!boundaryVisited.insert(current).second) { valid=false;break; }
                loop.push_back(current);
                const auto it=next.find(current);
                if (it==next.end()) { valid=false;break; }
                current=it->second;
            } while (current!=entry.first);
            if (loop.size()<3) valid=false;
            loops.push_back(std::move(loop));
        }
        if (!valid || loops.empty() || boundaryVisited.size()!=next.size() ||
            vertices.size()+region.size()+loops.size()!=incidence.size()+2)
        { ++report.topology;++report.rejected;continue; }
        if (next.size()>2048 || loops.size()>17) { ++report.budget;++report.rejected;continue; }
        size_t expectedTriangles=next.size()+2*(loops.size()-1)-2;
        if ((!certified || certified->overflow) && expectedTriangles>=region.size()) continue;
        const long double areaEpsilon=scale*scale*1e-14L;
        size_t outer=loops.size();
        for (size_t l=0;l<loops.size();++l)
        {
            long double signedArea=0;
            const auto origin2=projected[loops[l][0]];
            for (size_t i=1;i+1<loops[l].size();++i)
                signedArea+=orient(origin2,projected[loops[l][i]],projected[loops[l][i+1]]);
            if (std::abs(signedArea)<=areaEpsilon) valid=false;
            if (signedArea>0)
            {
                if (outer!=loops.size()) valid=false;
                outer=l;
            }
        }
        if (!valid || outer==loops.size())
        { if (loops.size()>1) ++report.holes;else ++report.topology;++report.rejected;continue; }
        std::swap(loops[0],loops[outer]);
        std::vector<uint32_t> ring;
        for (const auto &loop:loops) ring.insert(ring.end(),loop.begin(),loop.end());
        // Intersections include ring/ring contacts. Shared adjacent endpoints are
        // allowed, but doubled-back adjacent segments are not.
        for (size_t i=0;i<ring.size() && valid;++i)
            for (size_t j=i+1;j<ring.size();++j)
            {
                if (!tick()) { valid=false;break; }
                const auto a=ring[i],b=next.at(a),c=ring[j],d=next.at(c);
                if (b==c || d==a)
                {
                    const auto first=b==c?a:c,shared=b==c?b:a,last=b==c?d:b;
                    if (segment(projected[first],projected[shared],projected[last]) ||
                        segment(projected[last],projected[first],projected[shared])) valid=false;
                }
                else if (intersects(projected[a],projected[b],projected[c],projected[d],areaEpsilon)) valid=false;
                if (!valid) break;
            }
        // -1 outside, 0 boundary/uncertain, +1 inside. All edge visits are budgeted.
        const auto inside=[&](const std::vector<uint32_t> &loop,POINT p) {
            bool result=false;
            for (size_t i=0;i<loop.size();++i)
            {
                if (!tick()) return 0;
                const auto a=projected[loop[i]],b=projected[loop[(i+1)%loop.size()]];
                if (segment(p,a,b)) return 0;
                if ((a.y>p.y)!=(b.y>p.y))
                {
                    const auto side=orient(a,b,p);
                    if ((side>0)==(b.y>a.y)) result=!result;
                }
            }
            return result?1:-1;
        };
        for (size_t h=1;h<loops.size() && valid;++h)
        {
            if (inside(loops[0],projected[loops[h][0]])!=1) valid=false;
            for (size_t j=1;j<loops.size() && valid;++j)
                if (h!=j && inside(loops[j],projected[loops[h][0]])!=-1) valid=false;
        }
        // The coordinated mask removes only exact collinear samples. Preserve
        // original contacts for the obstacle certificate; their segment union is
        // unchanged, and the coordinator verifies every incident region together.
        std::map<uint32_t,uint32_t> originalNext;
        std::vector<uint32_t> originalRing;
        if (removable)
        {
            originalNext=next;originalRing=ring;
            next.clear();ring.clear();
            for (auto &loop:loops)
            {
                loop.erase(std::remove_if(loop.begin(),loop.end(),[&](uint32_t v) { return (*removable)[v]; }),loop.end());
                if (loop.size()<3) { valid=false;break; }
                for (size_t i=0;i<loop.size();++i) next.emplace(loop[i],loop[(i+1)%loop.size()]);
                ring.insert(ring.end(),loop.begin(),loop.end());
            }
            expectedTriangles=next.size()+2*(loops.size()-1)-2;
        }
        // Splice only visible bridges in the correct local interior cone. The
        // repeated endpoint IDs describe a cut, not duplicate output vertices.
        auto remaining=loops[0];
        for (size_t h=1;h<loops.size() && valid;++h)
        {
            if (cancelled()) return false;
            const auto &hole=loops[h];
            long double best=std::numeric_limits<long double>::infinity();
            size_t chosen=0,vertex=0;bool found=false;
            const auto locallyInside=[&](uint32_t previous,uint32_t current,uint32_t following,POINT target) {
                const auto u=projected[previous],v=projected[current],w=projected[following];
                const auto left=orient(u,v,target),right=orient(v,w,target);
                if (orient(u,v,w)>=0) return left>areaEpsilon && right>areaEpsilon;
                return left>areaEpsilon || right>areaEpsilon;
            };
            for (size_t i=0;i<remaining.size() && !over;++i)
                for (size_t j=0;j<hole.size();++j)
                {
                    if (!tick()) break;
                    const auto a=remaining[i],b=hole[j];
                    const auto p=projected[a],q=projected[b],delta=sub(p,q);
                    const auto length=dot(delta,delta);
                    if (length>=best) continue;
                    if (!locallyInside(remaining[(i+remaining.size()-1)%remaining.size()],a,
                                       remaining[(i+1)%remaining.size()],q) ||
                        !locallyInside(hole[(j+hole.size()-1)%hole.size()],b,hole[(j+1)%hole.size()],p)) continue;
                    const POINT mid{(p.x+q.x)/2,(p.y+q.y)/2,0};
                    bool blocked=inside(loops[0],mid)!=1;
                    for (size_t k=1;k<loops.size() && !blocked;++k) blocked=inside(loops[k],mid)!=-1;
                    const auto hits=[&](uint32_t c,uint32_t d) {
                        if (!tick()) return true;
                        if (c==a || c==b || d==a || d==b)
                        {
                            const auto other=(c==a || c==b)?d:c;
                            const auto shared=(c==a || c==b)?c:d;
                            const auto target=shared==a?b:a;
                            return segment(projected[other],p,q) ||
                                segment(projected[target],projected[shared],projected[other]);
                        }
                        return intersects(p,q,projected[c],projected[d],areaEpsilon);
                    };
                    for (auto c:ring) { if (blocked) break;blocked=hits(c,next.at(c)); }
                    for (size_t k=0;k<remaining.size() && !blocked;++k)
                        blocked=hits(remaining[k],remaining[(k+1)%remaining.size()]);
                    if (!blocked) { found=true;best=length;chosen=i;vertex=j; }
                }
            if (!found || over) { valid=false;break; }
            std::vector<uint32_t> joined;
            joined.insert(joined.end(),remaining.begin(),remaining.begin()+chosen+1);
            for (size_t k=0;k<hole.size();++k) joined.push_back(hole[(vertex+k)%hole.size()]);
            joined.push_back(hole[vertex]);joined.push_back(remaining[chosen]);
            joined.insert(joined.end(),remaining.begin()+chosen+1,remaining.end());
            remaining=std::move(joined);
        }
        if (!valid)
        {
            if (over) ++report.budget;
            else if (loops.size()>1) ++report.holes;
            else ++report.topology;
            ++report.rejected;continue;
        }
        // Ear clipping preserves all boundary samples, including collinear ones.
        std::vector<std::array<uint32_t,3>> replacement;
        while (valid && remaining.size()>3)
        {
            if (cancelled()) return false;
            bool found=false;
            for (size_t i=0;i<remaining.size();++i)
            {
                const auto a=remaining[(i+remaining.size()-1)%remaining.size()],b=remaining[i],c=remaining[(i+1)%remaining.size()];
                if (orient(projected[a],projected[b],projected[c])<=areaEpsilon) continue;
                bool contains=false;
                for (auto v:remaining)
                {
                    if (!tick()) { valid=false;break; }
                    if (v==a||v==b||v==c) continue;
                    if (orient(projected[a],projected[b],projected[v])>=-areaEpsilon &&
                        orient(projected[b],projected[c],projected[v])>=-areaEpsilon &&
                        orient(projected[c],projected[a],projected[v])>=-areaEpsilon) { contains=true;break; }
                }
                if (!valid) break;
                if (contains) continue;
                replacement.push_back({a,b,c});remaining.erase(remaining.begin()+static_cast<ptrdiff_t>(i));found=true;break;
            }
            if (!found) valid=false;
        }
        if (valid) replacement.push_back({remaining[0],remaining[1],remaining[2]});
        long double newArea=0;
        for (const auto &f:replacement)
        {
            const auto a=orient(projected[f[0]],projected[f[1]],projected[f[2]]);
            const auto n=cross(sub(point(in.positions[f[1]]),point(in.positions[f[0]])),sub(point(in.positions[f[2]]),point(in.positions[f[0]])));
            if (a<=areaEpsilon || dot(n,normal)<std::sqrt(dot(n,n))*norm*minimumDot) valid=false;
            newArea+=a;
        }
        // Positive faces with the exact oriented boundary and paired interior
        // edges have the original domain's winding/coverage, including every hole.
        // Check links too: a bridge must not leave a disconnected vertex fan.
        std::map<uint64_t,std::vector<std::pair<uint32_t,uint32_t>>> resultEdges;
        std::map<uint32_t,std::map<uint32_t,uint32_t>> resultLinks;
        std::set<std::array<uint32_t,3>> resultFaces;
        if (replacement.size()!=expectedTriangles) valid=false;
        for (const auto &f:replacement)
        {
            if (!tick()) { valid=false;break; }
            auto sorted=f;std::sort(sorted.begin(),sorted.end());
            if (!resultFaces.insert(sorted).second) valid=false;
            for (unsigned k=0;k<3;++k)
            {
                const auto a=f[k],b=f[(k+1)%3],c=f[(k+2)%3];
                resultEdges[edge(a,b)].push_back({a,b});
                if (!resultLinks[a].emplace(b,c).second) valid=false;
            }
        }
        size_t resultBoundary=0;
        for (const auto &entry:resultEdges)
        {
            if (!tick()) { valid=false;break; }
            const auto &list=entry.second;
            if (list.size()==1)
            {
                ++resultBoundary;
                const auto it=next.find(list[0].first);
                if (it==next.end() || it->second!=list[0].second) valid=false;
            }
            else if (list.size()!=2 || list[0].first!=list[1].second || list[0].second!=list[1].first) valid=false;
        }
        if (resultBoundary!=next.size() || resultLinks.size()!=next.size()) valid=false;
        for (const auto &entry:resultLinks)
        {
            if (!tick()) { valid=false;break; }
            const auto &link=entry.second;
            std::set<uint32_t> destinations;
            for (const auto &e:link) if (!destinations.insert(e.second).second) valid=false;
            auto start=link.begin()->first;
            size_t starts=0;
            for (const auto &e:link) if (!destinations.count(e.first)) { start=e.first;++starts; }
            auto current=start;size_t traversed=0;
            while (traversed<=link.size())
            {
                const auto it=link.find(current);if (it==link.end()) break;
                current=it->second;++traversed;
            }
            if (starts!=1 || traversed!=link.size()) valid=false;
        }
        if (std::abs(newArea-sourceArea)>areaEpsilon*region.size()) valid=false;
        if (!valid) { if (over) ++report.budget;else ++report.topology;++report.rejected;continue; }
        // Conservative whole-frame obstacle test. A slab-separated neighbor is safe.
        // Contacts are allowed only on unchanged boundary vertices/segments in an exact plane.
        POINT lo=projected[ring[0]],hi=lo;
        for (auto v:ring) { const auto p=projected[v];lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y); }
        const auto &contactRing=removable?originalRing:ring;
        const auto &contactNext=removable?originalNext:next;
        const auto obstacle=[&](const std::array<VEC3,3> &tri) {
            if (!tick()) return true;
            long double d[3];POINT p[3];
            for (unsigned k=0;k<3;++k) { d[k]=distance(point(tri[k]));p[k]=project(point(tri[k])); }
            if (std::min({d[0],d[1],d[2]})>tolerance || std::max({d[0],d[1],d[2]})<-tolerance) return false;
            if (std::max({p[0].x,p[1].x,p[2].x})<lo.x || std::min({p[0].x,p[1].x,p[2].x})>hi.x ||
                std::max({p[0].y,p[1].y,p[2].y})<lo.y || std::min({p[0].y,p[1].y,p[2].y})>hi.y) return false;
            if (maxDistance!=0) return true;
            const bool coplanar=d[0]==0 && d[1]==0 && d[2]==0;
            // Preserve the existing coplanar box-boundary contact rule.
            if (coplanar && (std::max({p[0].x,p[1].x,p[2].x})<=lo.x || std::min({p[0].x,p[1].x,p[2].x})>=hi.x ||
                std::max({p[0].y,p[1].y,p[2].y})<=lo.y || std::min({p[0].y,p[1].y,p[2].y})>=hi.y)) return false;
            // Disjoint full projections imply disjoint 3D geometry, even when an
            // inclined obstacle crosses the seed plane inside a hole or notch.
            const auto strictlySeparated=[&]() {
                // Overlapping boxes need not mean overlapping domains (holes/notches).
                // Require a strict separating edge for every candidate triangle.
                // Closed contacts and numerically uncertain pairs stay protected.
                const auto signedArea=orient(p[0],p[1],p[2]);
                if (std::abs(signedArea)<=areaEpsilon) return false;
                const auto separated=[&](const POINT *a,const POINT *b,long double sign) {
                    for (unsigned k=0;k<3;++k)
                        if (sign*orient(a[k],a[(k+1)%3],b[0])<-areaEpsilon &&
                            sign*orient(a[k],a[(k+1)%3],b[1])<-areaEpsilon &&
                            sign*orient(a[k],a[(k+1)%3],b[2])<-areaEpsilon) return true;
                    return false;
                };
                for (const auto &face:replacement)
                {
                    if (!tick()) return false;
                    const POINT q[3]={projected.at(face[0]),projected.at(face[1]),projected.at(face[2])};
                    if (!separated(q,p,1) && !separated(p,q,signedArea>0?1:-1)) return false;
                }
                return true;
            };
            if (strictlySeparated()) return false;
            if (over || coplanar) return true;
            // A 3D gap can exist even when the full projections overlap.
            // Outward interval projections certify it without constructing an
            // intersection segment. Keep the previous contact fallback on failure.
            const auto spatiallySeparated=[&]() {
                SEPARATION_TRIANGLE obstacleTriangle;
                for (unsigned k=0;k<3;++k) obstacleTriangle[k]={tri[k].x,tri[k].y,tri[k].z};
                const auto obstacleNormal=cross(sub(point(tri[1]),point(tri[0])),sub(point(tri[2]),point(tri[0])));
                if (dot(obstacleNormal,obstacleNormal)<=areaEpsilon*areaEpsilon) return false;
                const auto clearance=std::max(tolerance,scale*1e-12L);
                for (const auto &face:replacement)
                {
                    SEPARATION_TRIANGLE candidate;
                    for (unsigned k=0;k<3;++k)
                    { const auto &v=in.positions[face[k]];candidate[k]={v.x,v.y,v.z}; }
                    if (!strictlySeparated3D(candidate,obstacleTriangle,clearance,tick)) return false;
                }
                return true;
            };
            if (spatiallySeparated()) return false;
            if (over) return true;
            if (std::min({d[0],d[1],d[2]})<0 && std::max({d[0],d[1],d[2]})>0) return true;
            std::vector<uint32_t> contact;
            for (unsigned k=0;k<3;++k) if (std::abs(d[k])<=tolerance)
            {
                bool found=false;
                for (auto v:contactRing) if (same(point(tri[k]),point(in.positions[v]))) { contact.push_back(v);found=true;break; }
                if (!found) return true;
            }
            if (contact.size()==2 && contactNext.at(contact[0])!=contact[1] && contactNext.at(contact[1])!=contact[0]) return true;
            return false;
        };
        // Query the same projected rectangle as the old whole-frame scan.
        // Ignoring the dropped axis preserves slab/near-plane conservatism.
        OBSTACLE_BOX queryBox;
        const auto first=point(in.positions[ring[0]]);
        queryBox.lo=queryBox.hi={first.x,first.y,first.z};
        for (auto v:ring)
        {
            const auto &p=in.positions[v];const std::array<long double,3> value{p.x,p.y,p.z};
            for (unsigned a=0;a<3;++a)
            { queryBox.lo[a]=std::min(queryBox.lo[a],value[a]);queryBox.hi[a]=std::max(queryBox.hi[a],value[a]); }
        }
        valid=obstacleIndex.query(obstacleBoxes,queryBox,drop,obstacleCandidates,tick);
        for (auto id:obstacleCandidates)
        {
            if (!valid) break;
            if (cancelled()) return false;
            if (id<count)
            {
                if (members.count(static_cast<uint32_t>(id))) continue;
                valid=!obstacle({in.positions[faces[id][0]],in.positions[faces[id][1]],in.positions[faces[id][2]]});
            }
            else valid=!obstacle(attr.surroundings[id-count]);
        }
        if (!valid) { if (over) ++report.budget;else ++report.surroundings;++report.rejected;continue; }
        if (certified && maxDistance==0 && !certified->overflow)
        {
            if (certified->regions.size()>=4096 || certified->boundaryCount+next.size()>32768)
                certified->overflow=true;
            else
            {
                certified->boundaryCount+=next.size();
                certified->regions.push_back({region,next});
            }
        }
        if (replacement.size()>=region.size()) continue;
        for (auto f:region) removed[f]=true;
        added.insert(added.end(),replacement.begin(),replacement.end());
        addedGroups.insert(addedGroups.end(),replacement.size(),group);
        ++report.accepted;report.removed+=static_cast<uint32_t>(region.size()-replacement.size());
        // Both piecewise-linear fields are within maxDistance of the SAME plane;
        // at any shared projected point their distance is bounded by twice that.
        report.error=std::max(report.error,static_cast<double>(2*maxDistance*norm / std::abs(drop==0?normal.x:(drop==1?normal.y:normal.z))));
        report.uvError=std::max(report.uvError,static_cast<double>(2*maxUv));
    }
    if (cancelled()) return false;
    INPUT result=in;result.indices.clear();result.triangleGroups.clear();
    for (uint32_t f=0;f<count;++f) if (!removed[f])
    {
        result.indices.insert(result.indices.end(),faces[f].begin(),faces[f].end());
        result.triangleGroups.push_back(in.triangleGroups[f]);
    }
    for (size_t f=0;f<added.size();++f)
    {
        result.indices.insert(result.indices.end(),added[f].begin(),added[f].end());
        result.triangleGroups.push_back(addedGroups[f]);
    }
    out=std::move(result);
    if (progress) progress(1);
    return true;
}
// Coordinate aliases by exact geometry, without welding material/attribute charts.
// The optional second pass is committed only if every dependent region certifies.
inline bool run(const INPUT &in,const ATTRIBUTES &attr,INPUT &out,REPORT &report,
                std::string &error,const std::function<void(float)> &progress={},
                const std::function<bool()> &cancel={})
{
    if (!attr.reduceBoundaries) return runRegions(in,attr,out,report,error,progress,cancel);
    const auto cancelled=[&]() {
        if (!cancel || !cancel()) return false;
        error="simplification cancelled";return true;
    };
    INPUT baseline;REPORT baselineReport;REGION_CERTIFICATES certificates;
    if (!runRegions(in,attr,baseline,baselineReport,error,
        [&](float p) { if (progress) progress(p*.45f); },cancel,&certificates)) return false;
    const auto keepBaseline=[&](bool fallback) {
        if (cancelled()) return false;
        baselineReport.boundaryFallback=fallback;
        out=std::move(baseline);report=baselineReport;
        if (progress) progress(1);
        return true;
    };
    if (certificates.overflow) return keepBaseline(true);
    using POSITION_KEY=std::array<float,3>;
    const auto key=[&](uint32_t v) { const auto &p=in.positions[v];return POSITION_KEY{p.x,p.y,p.z}; };
    struct CONTACT
    {
        std::array<POSITION_KEY,2> neighbors;
        std::vector<uint32_t> vertices;
        std::set<uint32_t> regions;
        bool valid=true, initialized=false;
    };
    std::map<POSITION_KEY,CONTACT> contacts;
    std::vector<uint32_t> owner(in.indices.size()/3,UINT32_MAX);
    for (uint32_t r=0;r<certificates.regions.size();++r)
    {
        if (cancelled()) return false;
        const auto &region=certificates.regions[r];
        for (auto f:region.faces) owner[f]=r;
        std::map<uint32_t,uint32_t> previous;
        for (const auto &e:region.next) previous.emplace(e.second,e.first);
        for (const auto &e:region.next)
        {
            const auto v=e.first,a=previous.at(v),b=e.second;
            auto &contact=contacts[key(v)];
            std::array<POSITION_KEY,2> neighbors{key(a),key(b)};
            if (neighbors[1]<neighbors[0]) std::swap(neighbors[0],neighbors[1]);
            if (contact.initialized && contact.neighbors!=neighbors) contact.valid=false;
            contact.neighbors=neighbors;contact.initialized=true;
            contact.vertices.push_back(v);contact.regions.insert(r);
            const auto p=point(in.positions[v]),pa=point(in.positions[a]),pb=point(in.positions[b]);
            const auto n=cross(sub(p,pa),sub(pb,pa));
            if (dot(n,n)!=0 || dot(sub(p,pa),sub(p,pb))>=0) contact.valid=false;
        }
    }
    // Any incident face outside a certified exact-plane boundary locks all aliases.
    for (size_t i=0;i<in.indices.size();++i)
    {
        if ((i&1023)==0 && cancelled()) return false;
        const auto v=in.indices[i];const auto it=contacts.find(key(v));
        if (it==contacts.end()) continue;
        const auto r=owner[i/3];
        if (r==UINT32_MAX || !certificates.regions[r].next.count(v) ||
            (!attr.locked.empty() && attr.locked[v])) it->second.valid=false;
    }
    for (const auto &triangle:attr.surroundings)
    {
        if (cancelled()) return false;
        for (const auto &p:triangle)
        {
            const auto it=contacts.find({p.x,p.y,p.z});
            if (it!=contacts.end()) it->second.valid=false;
        }
    }
    std::vector<bool> removable(in.positions.size(),false);
    std::set<uint32_t> dependentSeeds;
    uint32_t removedPositions=0;
    for (const auto &entry:contacts)
    {
        const auto &c=entry.second;
        if (!c.valid || c.regions.empty() || c.regions.size()>2) continue;
        for (auto v:c.vertices) removable[v]=true;
        for (auto r:c.regions) dependentSeeds.insert(certificates.regions[r].faces.front());
        ++removedPositions;
    }
    if (cancelled()) return false;
    if (progress) progress(.55f);
    if (!removedPositions) return keepBaseline(false);
    INPUT candidate;REPORT candidateReport;REGION_CERTIFICATES completed;
    if (!runRegions(in,attr,candidate,candidateReport,error,
        [&](float p) { if (progress) progress(.55f+p*.45f); },cancel,&completed,&removable)) return false;
    if (completed.overflow) return keepBaseline(true);
    for (const auto &region:completed.regions) dependentSeeds.erase(region.faces.front());
    if (!dependentSeeds.empty() || candidate.indices.size()>=baseline.indices.size()) return keepBaseline(true);
    if (cancelled()) return false;
    candidateReport.boundaryRemoved=removedPositions;
    out=std::move(candidate);report=candidateReport;
    if (progress) progress(1);
    return true;
}

}
#endif
