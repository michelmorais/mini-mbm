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


// Standalone structural test: c++ -std=c++17 -O2 this-file.cpp -o /tmp/planar-index-test
#include "../../core_mbm/private/mesh-planar-index.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace mbm::mesh_simplifier::planar;
int main()
{
    std::mt19937 random(287);
    std::vector<OBSTACLE_BOX> boxes(4097);
    for (auto &box:boxes) for (unsigned a=0;a<3;++a)
    {
        box.lo[a]=static_cast<int>(random()%1000)-500;
        box.hi[a]=box.lo[a]+random()%30;
    }
    boxes[0]={{{0,0,0}},{{0,0,0}}}; // Point contacts and zero extent.
    boxes[1]={{{-1,-1,-1}},{{1,1,1}}};
    OBSTACLE_INDEX index;assert(index.build(boxes));
    size_t visits=0;
    for (unsigned drop=0;drop<3;++drop) for (unsigned trial=0;trial<400;++trial)
    {
        OBSTACLE_BOX query=boxes[trial];
        if (trial%2==0) query.lo=query.hi; // Exact face/edge/point contact.
        std::vector<size_t> actual,expected;
        assert(index.query(boxes,query,drop,actual,[&] { ++visits;return true; }));
        for (size_t i=0;i<boxes.size();++i)
        {
            bool overlaps=true;
            for (unsigned a=0;a<3;++a) if (a!=drop &&
                (boxes[i].hi[a]<query.lo[a] || boxes[i].lo[a]>query.hi[a])) overlaps=false;
            if (overlaps) expected.push_back(i);
        }
        assert(actual==expected);
    }
    assert(visits<4097ULL*1200/2); // Verify structural pruning, not wall-clock noise.
    for (unsigned stop: {1U,10U,100U})
    {
        unsigned checks=0;
        assert(!index.build(boxes,[&] { return ++checks>=stop; }));
    }
    assert(index.build(boxes));
    OBSTACLE_BOX full={{{-1000,-1000,-1000}},{{1000,1000,1000}}};
    for (unsigned stop: {1U,10U,100U})
    {
        unsigned checks=0;std::vector<size_t> output{999};
        assert(!index.query(boxes,full,2,output,[&] { return ++checks<stop; }));
        assert(output.empty());
    }
    std::vector<size_t> output;
    assert(index.query(boxes,full,2,output,[] { return true; }));assert(output.size()==boxes.size());
    assert(index.build({}));assert(index.query({},full,2,output,[] { return true; }));assert(output.empty());
    std::cout << "PLANAR INDEX OK: 1200 brute-force comparisons; visits=" << visits << '\n';
}
