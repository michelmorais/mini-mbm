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


#ifndef MBM_MESH_PLANAR_SEPARATION_H
#define MBM_MESH_PLANAR_SEPARATION_H
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>

namespace mbm::mesh_simplifier::planar
{
using SEPARATION_POINT=std::array<long double,3>;
using SEPARATION_TRIANGLE=std::array<SEPARATION_POINT,3>;
struct SEPARATION_INTERVAL { long double lo=0,hi=0; };
inline long double separationDown(long double value)
{ return std::nextafter(value,-std::numeric_limits<long double>::infinity()); }
inline long double separationUp(long double value)
{ return std::nextafter(value,std::numeric_limits<long double>::infinity()); }
// Enclose dot(axis, position-origin), treating the computed axis as an exact
// direction. Its agreement with an ideal triangle normal is not needed for safety.
inline SEPARATION_INTERVAL separationProjection(const SEPARATION_POINT &axis,
    const SEPARATION_POINT &position,const SEPARATION_POINT &origin)
{
    SEPARATION_INTERVAL result;
    for (unsigned k=0;k<3;++k)
    {
        const auto delta=position[k]-origin[k];
        const auto a=axis[k]*separationDown(delta),b=axis[k]*separationUp(delta);
        result.lo=separationDown(result.lo+separationDown(std::min(a,b)));
        result.hi=separationUp(result.hi+separationUp(std::max(a,b)));
    }
    return result;
}
inline SEPARATION_POINT separationSub(const SEPARATION_POINT &a,const SEPARATION_POINT &b)
{ return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
inline SEPARATION_POINT separationCross(const SEPARATION_POINT &a,const SEPARATION_POINT &b)
{ return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
inline bool separationDirection(const SEPARATION_POINT &axis)
{
    return std::isfinite(axis[0]) && std::isfinite(axis[1]) && std::isfinite(axis[2]) &&
        (axis[0]!=0 || axis[1]!=0 || axis[2]!=0);
}
// Sufficient certificate only: touching, degenerate or uncertain pairs return false.
inline bool strictlySeparated3D(const SEPARATION_TRIANGLE &a,const SEPARATION_TRIANGLE &b,
    long double clearance,const std::function<bool()> &tick)
{
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
    // Reassociation/finite-only assumptions invalidate outward interval bounds.
    return false;
#endif
    if (!std::isfinite(clearance) || clearance<0) return false;
    for (const auto &triangle:{a,b}) for (const auto &p:triangle)
        for (auto value:p) if (!std::isfinite(value)) return false;
    SEPARATION_TRIANGLE ea,eb;
    for (unsigned k=0;k<3;++k)
    { ea[k]=separationSub(a[(k+1)%3],a[k]);eb[k]=separationSub(b[(k+1)%3],b[k]); }
    const auto na=separationCross(ea[0],ea[1]),nb=separationCross(eb[0],eb[1]);
    if (!separationDirection(na) || !separationDirection(nb)) return false;
    const auto onAxis=[&](const SEPARATION_POINT &axis) {
        if (!separationDirection(axis)) return false;
        long double length=0;
        for (auto value:axis) length=separationUp(length+std::abs(value));
        const auto guard=separationUp(clearance*length);
        if (!std::isfinite(guard)) return false;
        auto left=separationProjection(axis,a[0],a[0]);
        auto right=separationProjection(axis,b[0],a[0]);
        for (unsigned k=0;k<3;++k)
        {
            const auto x=separationProjection(axis,a[k],a[0]),y=separationProjection(axis,b[k],a[0]);
            if (!std::isfinite(x.lo) || !std::isfinite(x.hi) ||
                !std::isfinite(y.lo) || !std::isfinite(y.hi)) return false;
            left.lo=std::min(left.lo,x.lo);left.hi=std::max(left.hi,x.hi);
            right.lo=std::min(right.lo,y.lo);right.hi=std::max(right.hi,y.hi);
        }
        return separationUp(left.hi+guard)<right.lo || separationUp(right.hi+guard)<left.lo;
    };
    for (const auto &axis:{na,nb})
    { if (!tick()) return false;if (onAxis(axis)) return true; }
    for (const auto &x:ea) for (const auto &y:eb)
    { if (!tick()) return false;if (onAxis(separationCross(x,y))) return true; }
    return false;
}
}
#endif
