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


// c++ -std=c++17 -O2 this-file.cpp -o /tmp/planar-separation-test
#include "../../core_mbm/private/mesh-planar-separation.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
using namespace mbm::mesh_simplifier::planar;
int main()
{
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
    const SEPARATION_TRIANGLE a={{{0,0,0},{1,0,0},{0,1,0}}},b={{{0,0,1},{1,0,1},{0,1,1}}};
    assert(!strictlySeparated3D(a,b,0,[] { return true; }));
    std::cout << "PLANAR 3D SEPARATION FAST-MATH FALLBACK OK\n";return 0;
#endif
    const auto large=std::ldexp(1.L,std::numeric_limits<long double>::digits);
    const auto cancellation=separationProjection({1,1,1},{large,1,-large},{0,0,0});
    assert(cancellation.lo<=1 && cancellation.hi>=1);
    const auto increment=separationProjection({1,0,0},{large,0,0},{-1,0,0});
    assert(increment.lo<=large && increment.hi>large);
    const auto tiny=std::ldexp(1.L,-std::numeric_limits<long double>::digits/2-1);
    const auto rounded=separationProjection({1+tiny,0,0},{1-tiny,0,0},{0,0,0});
    assert(rounded.lo<1 && rounded.hi>=1); // Exact product is 1-tiny^2, below 1.
    std::mt19937 random(289);
    const auto tick=[] { return true; };
    for (unsigned trial=0;trial<10000;++trial)
    {
        SEPARATION_POINT axis,position,origin;int64_t exact=0;
        for (unsigned k=0;k<3;++k)
        {
            const int64_t a=static_cast<int>(random()%20001)-10000;
            const int64_t p=static_cast<int>(random()%200001)-100000;
            const int64_t o=static_cast<int>(random()%200001)-100000;
            axis[k]=a;position[k]=p;origin[k]=o;exact+=a*(p-o);
        }
        const auto range=separationProjection(axis,position,origin);
        assert(range.lo<=exact && range.hi>=exact);
        // Every pair shares a vertex; no direction can strictly separate them.
        SEPARATION_TRIANGLE a={origin,position,axis},b={origin,axis,position};
        for (unsigned k=0;k<3;++k) b[2][k]+=17;
        assert(!strictlySeparated3D(a,b,0,tick));assert(!strictlySeparated3D(b,a,0,tick));
    }
    const SEPARATION_TRIANGLE base={{{0,0,0},{2,0,0},{0,2,0}}};
    for (unsigned axis=0;axis<3;++axis) for (auto scale:{1e-6L,1.L,1e6L})
    {
        auto a=base,b=base;
        for (unsigned i=0;i<3;++i) for (unsigned k=0;k<3;++k)
        {
            a[i][(axis+k)%3]=(base[i][k]+16)*scale;
            b[i][(axis+k)%3]=(base[i][k]+16+(k==2?1:0))*scale;
        }
        assert(strictlySeparated3D(a,b,scale*1e-7L,tick));
        assert(strictlySeparated3D(b,a,scale*1e-7L,tick));
        assert(!strictlySeparated3D(a,b,scale*2,tick));
        assert(!strictlySeparated3D(a,b,0,[] { return false; }));
    }
    const SEPARATION_TRIANGLE crossing={{{.5,.5,-1},{1.5,.5,1},{.5,1.5,1}}};
    const SEPARATION_TRIANGLE touching={{{.5,.5,0},{1.5,.5,1},{.5,1.5,1}}};
    assert(!strictlySeparated3D(base,crossing,0,tick));
    assert(!strictlySeparated3D(crossing,base,0,tick));
    assert(!strictlySeparated3D(base,touching,0,tick));
    assert(!strictlySeparated3D(touching,base,0,tick));
    auto close=base;for (auto &p:close) p[2]=1e-14L;
    assert(!strictlySeparated3D(base,close,1e-12L,tick));
    assert(strictlySeparated3D(base,close,0,tick));
    auto invalid=base;invalid[1]=invalid[0];
    assert(!strictlySeparated3D(base,invalid,0,tick));
    invalid[1][0]=std::numeric_limits<long double>::infinity();
    assert(!strictlySeparated3D(base,invalid,0,tick));
    unsigned calls=0;
    assert(!strictlySeparated3D(base,base,0,[&] { return ++calls<4; }));assert(calls==4);
    std::cout << "PLANAR 3D SEPARATION OK: 10000 interval enclosures / shared-vertex pairs\n";
}
