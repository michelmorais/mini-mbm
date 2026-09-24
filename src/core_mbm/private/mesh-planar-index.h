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


#ifndef MBM_MESH_PLANAR_INDEX_H
#define MBM_MESH_PLANAR_INDEX_H
#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <vector>

namespace mbm::mesh_simplifier::planar
{
// Private request-local broad phase. Inclusive boxes retain every contact.
struct OBSTACLE_BOX
{
    std::array<long double,3> lo{},hi{};
};
class OBSTACLE_INDEX
{
    struct NODE
    {
        OBSTACLE_BOX box;
        size_t begin=0,end=0,left=0,right=0;
    };
    std::vector<NODE> nodes;
    std::vector<size_t> order;
    size_t buildNode(const std::vector<OBSTACLE_BOX> &boxes,size_t begin,size_t end,
                     const std::function<bool()> &cancel,bool &stopped)
    {
        const size_t id=nodes.size();nodes.push_back({});
        OBSTACLE_BOX box=boxes[order[begin]];
        for (size_t i=begin;i<end;++i)
        {
            if ((i&255)==0 && cancel && cancel()) { stopped=true;return id; }
            for (unsigned a=0;a<3;++a)
            {
                box.lo[a]=std::min(box.lo[a],boxes[order[i]].lo[a]);
                box.hi[a]=std::max(box.hi[a],boxes[order[i]].hi[a]);
            }
        }
        nodes[id].box=box;nodes[id].begin=begin;nodes[id].end=end;
        if (end-begin<=8) return id;
        unsigned axis=0;
        for (unsigned a=1;a<3;++a) if (box.hi[a]-box.lo[a]>box.hi[axis]-box.lo[axis]) axis=a;
        const size_t middle=begin+(end-begin)/2;
        std::nth_element(order.begin()+begin,order.begin()+middle,order.begin()+end,[&](size_t a,size_t b) {
            const auto x=boxes[a].lo[axis]+boxes[a].hi[axis],y=boxes[b].lo[axis]+boxes[b].hi[axis];
            return x<y || (x==y && a<b);
        });
        if (cancel && cancel()) { stopped=true;return id; }
        const auto left=buildNode(boxes,begin,middle,cancel,stopped);
        if (stopped) return id;
        const auto right=buildNode(boxes,middle,end,cancel,stopped);
        nodes[id].left=left;nodes[id].right=right;
        return id;
    }
public:
    bool build(const std::vector<OBSTACLE_BOX> &boxes,const std::function<bool()> &cancel={})
    {
        nodes.clear();order.resize(boxes.size());
        for (size_t i=0;i<order.size();++i)
        {
            if ((i&255)==0 && cancel && cancel()) return false;
            order[i]=i;
        }
        if (boxes.empty()) return true;
        bool stopped=false;buildNode(boxes,0,boxes.size(),cancel,stopped);
        if (stopped) nodes.clear();
        return !stopped;
    }
    // Returning false means incomplete: callers must discard all candidates.
    bool query(const std::vector<OBSTACLE_BOX> &boxes,const OBSTACLE_BOX &box,unsigned drop,
               std::vector<size_t> &result,const std::function<bool()> &tick) const
    {
        result.clear();
        if (nodes.empty()) return true;
        const auto overlaps=[&](const OBSTACLE_BOX &other) {
            for (unsigned a=0;a<3;++a) if (a!=drop &&
                (other.hi[a]<box.lo[a] || other.lo[a]>box.hi[a])) return false;
            return true;
        };
        std::vector<size_t> pending{0};
        while (!pending.empty())
        {
            if (!tick()) { result.clear();return false; }
            const auto id=pending.back();pending.pop_back();const auto &node=nodes[id];
            if (!overlaps(node.box)) continue;
            if (node.left!=0) { pending.push_back(node.right);pending.push_back(node.left);continue; }
            for (size_t i=node.begin;i<node.end;++i)
            {
                if (!tick()) { result.clear();return false; }
                if (overlaps(boxes[order[i]])) result.push_back(order[i]);
            }
        }
        std::sort(result.begin(),result.end());
        if (!tick()) { result.clear();return false; }
        return true;
    }
};
}
#endif
