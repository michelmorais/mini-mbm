/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "facial-animation.h"
#include "../mesh-v11-io.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace mbm::facial
{
    namespace
    {
        bool fail(std::string &error, const char *reason) { error = reason; return false; }
        bool finite(const VEC3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
        bool weight(float value) { return std::isfinite(value) && value >= 0 && value <= 1; }
        bool name(const std::string &value, bool empty = false)
        {
            if ((!empty && value.empty()) || value.size() > 1024) return false;
            // Reject NUL, overlong encodings, surrogates and out-of-range UTF-8 code points.
            for (size_t i = 0; i < value.size();)
            {
                uint32_t c = static_cast<unsigned char>(value[i++]);
                if (c == 0) return false;
                if (c < 128) continue;
                unsigned count = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc2 ? 1 : 0;
                if (!count || c > 0xf4 || i + count > value.size()) return false;
                const uint32_t minimum = count == 3 ? 0x10000 : count == 2 ? 0x800 : 0x80;
                c &= (1u << (6-count)) - 1u;
                for (unsigned n = 0; n < count; ++n)
                {
                    const unsigned char next = static_cast<unsigned char>(value[i++]);
                    if ((next & 0xc0) != 0x80) return false;
                    c = (c << 6) | (next & 0x3f);
                }
                if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
            }
            return true;
        }
        bool readVector(util::MEM_CURSOR_V11 &c, VEC3 &v)
        { return util::le_io::readF32LE(c,v.x) && util::le_io::readF32LE(c,v.y) && util::le_io::readF32LE(c,v.z); }
        bool writeVector(FILE *f, const VEC3 &v)
        { return util::le_io::writeF32LE(f,v.x) && util::le_io::writeF32LE(f,v.y) && util::le_io::writeF32LE(f,v.z); }
        void buildSparseIndex(TARGET &target)
        {
            target.activeVertices.clear();
            target.activeVertices.reserve(target.positions.size());
            for (uint32_t index = 0; index < target.positions.size(); ++index)
            {
                const VEC3 &position = target.positions[index];
                const bool positionChanged = position.x != 0.0f || position.y != 0.0f || position.z != 0.0f;
                bool normalChanged = false;
                if (!target.normals.empty())
                {
                    const VEC3 &normal = target.normals[index];
                    normalChanged = normal.x != 0.0f || normal.y != 0.0f || normal.z != 0.0f;
                }
                if (positionChanged || normalChanged) target.activeVertices.push_back(index);
            }
            target.sparseReady = true;
            target.activeVertices.shrink_to_fit();
        }
    }

    uint64_t geometrySignature(const VEC3 *p, const VEC3 *n, uint32_t count,
                               const uint16_t *indices, uint32_t indexCount) noexcept
    {
        uint64_t hash = 14695981039346656037ull;
        const auto word = [&hash](uint32_t value) {
            for (unsigned i=0;i<4;++i) { hash ^= (value >> (8*i)) & 255u; hash *= 1099511628211ull; }
        };
        word(count); word(indexCount); word(n ? 1u : 0u);
        for (uint32_t i=0;i<count;++i)
        {
            for (const VEC3 *v : {p+i, n ? n+i : nullptr})
            {
                if (!v) continue;
                for (float value : {v->x,v->y,v->z})
                {
                    uint32_t bits = 0;
                    if (value != 0) std::memcpy(&bits,&value,4); // Canonicalize signed zero.
                    word(bits);
                }
            }
        }
        for (uint32_t i=0;i<indexCount;++i) word(indices[i]);
        return hash;
    }

    bool validate(const ASSET &a, std::string &error)
    {
        if (!a.meshId || !a.geometrySignature || !a.vertexCount || a.vertexCount > MAX_VERTICES ||
            a.targets.empty() || a.targets.size() > MAX_TARGETS || a.clips.size() > MAX_CLIPS)
            return fail(error,"facial-invalid-header-or-count");
        std::unordered_set<uint64_t> ids;
        std::unordered_set<std::string> names;
        uint64_t bytes = 24;
        for (const TARGET &t : a.targets)
        {
            if (!t.id || !ids.insert(t.id).second || !name(t.name) || !names.insert(t.name).second ||
                !name(t.convention,true) || !weight(t.defaultWeight))
                return fail(error,"facial-invalid-target-identity-name-or-default");
            if (t.positions.size()!=a.vertexCount || (!t.normals.empty() && t.normals.size()!=a.vertexCount))
                return fail(error,"facial-target-vertex-count-mismatch");
            bytes += 20 + t.name.size() + t.convention.size() + 12ull*(t.positions.size()+t.normals.size());
            if (bytes > MAX_SECTION_BYTES) return fail(error,"facial-target-byte-budget");
            for (const VEC3 &v : t.positions) if (!finite(v)) return fail(error,"facial-non-finite-delta");
            for (const VEC3 &v : t.normals) if (!finite(v)) return fail(error,"facial-non-finite-normal-delta");
        }
        ids.clear(); names.clear(); bytes=12;
        uint64_t values=0;
        for (const CLIP &clip : a.clips)
        {
            if (!clip.id || !ids.insert(clip.id).second || !name(clip.name) || !names.insert(clip.name).second ||
                !std::isfinite(clip.duration) || clip.duration<0 || clip.interpolation>1 || clip.keys.empty())
                return fail(error,"facial-invalid-clip-header");
            values += static_cast<uint64_t>(clip.keys.size())*a.targets.size();
            bytes += 26 + clip.name.size() + clip.keys.size()*(4ull + a.targets.size()*4ull);
            if (values>MAX_WEIGHT_VALUES || bytes>MAX_SECTION_BYTES) return fail(error,"facial-clip-budget");
            float previous=-1;
            for (const KEY &key : clip.keys)
            {
                if (!std::isfinite(key.time) || key.time<0 || key.time>clip.duration || key.time<=previous ||
                    key.weights.size()!=a.targets.size()) return fail(error,"facial-invalid-key-time-or-target-count");
                previous=key.time;
                for (float v : key.weights) if (!weight(v)) return fail(error,"facial-invalid-key-weight");
            }
            if (clip.keys.front().time!=0 || clip.keys.back().time!=clip.duration)
                return fail(error,"facial-clip-endpoints-required");
        }
        error.clear(); return true;
    }

    bool validateGeometry(const ASSET &a, const VEC3 *p, const VEC3 *n, uint32_t count,
                           const uint16_t *indices, uint32_t indexCount, std::string &error)
    {
        if (!validate(a,error)) return false;
        if (!p || count!=a.vertexCount || (indexCount && !indices)) return fail(error,"facial-base-count-mismatch");
        for (uint32_t i=0;i<count;++i)
            if (!finite(p[i]) || (n && !finite(n[i]))) return fail(error,"facial-non-finite-base");
        for (uint32_t i=0;i<indexCount;++i)
            if (indices[i]>=count) return fail(error,"facial-base-index-out-of-range");
        if (geometrySignature(p,n,count,indices,indexCount)!=a.geometrySignature)
            return fail(error,"facial-base-signature-mismatch");
        return true;
    }

    bool readTargets(util::MEM_CURSOR_V11 &c, uint16_t version, ASSET &out, std::string &error)
    {
        using namespace util::le_io;
        ASSET a; uint32_t count=0;
        if (version!=1 || c.size>MAX_SECTION_BYTES || !readU64LE(c,a.meshId) ||
            !readU64LE(c,a.geometrySignature) || !readU32LE(c,a.vertexCount) || !readU32LE(c,count) ||
            !count || count>MAX_TARGETS || !a.vertexCount || a.vertexCount>MAX_VERTICES)
            return fail(error,"facial-invalid-target-section-header");
        for (uint32_t i=0;i<count;++i)
        {
            TARGET t; uint32_t flags=0;
            if (!readU64LE(c,t.id) || !util::readStringV11(c,t.name) || !util::readStringV11(c,t.convention) ||
                !readF32LE(c,t.defaultWeight) || !readU32LE(c,flags) || flags>1 ||
                c.pos > c.size || 12ull*a.vertexCount*(1+flags)>c.size-c.pos)
                return fail(error,"facial-truncated-target-or-invalid-flags");
            t.positions.resize(a.vertexCount);
            if (flags) t.normals.resize(a.vertexCount);
            for (VEC3 &v : t.positions) if (!readVector(c,v)) return fail(error,"facial-truncated-delta");
            for (VEC3 &v : t.normals) if (!readVector(c,v)) return fail(error,"facial-truncated-normal");
            buildSparseIndex(t);
            a.targets.push_back(std::move(t));
        }
        if (c.pos!=c.size) return fail(error,"facial-trailing-target-bytes");
        if (!validate(a,error)) return false;
        out=std::move(a); return true;
    }

    bool readClips(util::MEM_CURSOR_V11 &c, uint16_t version, ASSET &out, std::string &error)
    {
        using namespace util::le_io;
        uint64_t id=0; uint32_t count=0;
        if (version!=1 || c.size>MAX_SECTION_BYTES || !readU64LE(c,id) || id!=out.meshId ||
            out.targets.empty() || !readU32LE(c,count) || !count || count>MAX_CLIPS)
            return fail(error,"facial-invalid-clip-section-header-or-mesh-id");
        std::vector<CLIP> clips; uint64_t values=0;
        for (uint32_t i=0;i<count;++i)
        {
            CLIP clip; uint32_t loop=0, keys=0;
            if (!readU64LE(c,clip.id) || !util::readStringV11(c,clip.name) || !readF32LE(c,clip.duration) ||
                !readU32LE(c,loop) || loop>1 || !readU32LE(c,clip.interpolation) || !readU32LE(c,keys))
                return fail(error,"facial-truncated-clip");
            values+=static_cast<uint64_t>(keys)*out.targets.size();
            if (!keys || values>MAX_WEIGHT_VALUES || c.pos > c.size ||
                static_cast<uint64_t>(keys)*(4+4*out.targets.size())>c.size-c.pos)
                return fail(error,"facial-invalid-key-count-or-budget");
            clip.loop=loop!=0;
            for (uint32_t k=0;k<keys;++k)
            {
                KEY key;
                if (!readF32LE(c,key.time)) return fail(error,"facial-truncated-key");
                key.weights.resize(out.targets.size());
                for (float &w : key.weights) if (!readF32LE(c,w)) return fail(error,"facial-truncated-weights");
                clip.keys.push_back(std::move(key));
            }
            clips.push_back(std::move(clip));
        }
        if (c.pos!=c.size) return fail(error,"facial-trailing-clip-bytes");
        // Validate without copying dense target arrays; restore original clips on failure.
        clips.swap(out.clips);
        if (validate(out,error)) return true;
        clips.swap(out.clips); return false;
    }

    bool writeTargets(FILE *f, const ASSET &a)
    {
        using namespace util::le_io;
        std::string error;
        if (!validate(a,error) || !writeU64LE(f,a.meshId) || !writeU64LE(f,a.geometrySignature) ||
            !writeU32LE(f,a.vertexCount) || !writeU32LE(f,static_cast<uint32_t>(a.targets.size()))) return false;
        for (const TARGET &t : a.targets)
        {
            if (!writeU64LE(f,t.id) || !util::writeStringV11(f,t.name) || !util::writeStringV11(f,t.convention) ||
                !writeF32LE(f,t.defaultWeight) || !writeU32LE(f,t.normals.empty()?0:1)) return false;
            for (const VEC3 &v : t.positions) if (!writeVector(f,v)) return false;
            for (const VEC3 &v : t.normals) if (!writeVector(f,v)) return false;
        }
        return true;
    }
    bool writeClips(FILE *f, const ASSET &a)
    {
        using namespace util::le_io;
        std::string error;
        if (!validate(a,error) || a.clips.empty() || !writeU64LE(f,a.meshId) ||
            !writeU32LE(f,static_cast<uint32_t>(a.clips.size()))) return false;
        for (const CLIP &clip : a.clips)
        {
            if (!writeU64LE(f,clip.id) || !util::writeStringV11(f,clip.name) || !writeF32LE(f,clip.duration) ||
                !writeU32LE(f,clip.loop?1:0) || !writeU32LE(f,clip.interpolation) ||
                !writeU32LE(f,static_cast<uint32_t>(clip.keys.size()))) return false;
            for (const KEY &key : clip.keys)
            {
                if (!writeF32LE(f,key.time)) return false;
                for (float w : key.weights) if (!writeF32LE(f,w)) return false;
            }
        }
        return true;
    }

    bool sampleClip(const ASSET &a, const uint32_t clipIndex, const float time,
                    std::vector<float> &weights, std::string &error)
    {
        if (clipIndex >= a.clips.size() || !std::isfinite(time) || time < 0.0f)
            return fail(error, "facial-clip-index-or-time-invalid");
        const CLIP &clip = a.clips[clipIndex];
        if (clip.keys.empty()) return fail(error, "facial-clip-has-no-keys");
        const float clamped = std::min(time, clip.duration);
        const KEY *left = &clip.keys.front();
        const KEY *right = left;
        for (size_t index = 1; index < clip.keys.size(); ++index)
        {
            if (clamped <= clip.keys[index].time)
            {
                right = &clip.keys[index];
                left = &clip.keys[index - 1];
                break;
            }
            left = right = &clip.keys[index];
        }
        weights.resize(a.targets.size());
        if (left == right || clip.interpolation == 0 || right->time <= left->time)
        {
            weights = left->weights;
            error.clear();
            return true;
        }
        const float alpha = (clamped - left->time) / (right->time - left->time);
        for (size_t index = 0; index < weights.size(); ++index)
            weights[index] = left->weights[index] + (right->weights[index] - left->weights[index]) * alpha;
        error.clear();
        return true;
    }

    bool deform(const ASSET &a, const std::vector<VEC3> &basePositions, const std::vector<VEC3> &baseNormals,
                 const std::vector<float> &weights, std::vector<VEC3> &p, std::vector<VEC3> &n, std::string &error)
    {
        if (basePositions.size()!=a.vertexCount || weights.size()!=a.targets.size() ||
            (!baseNormals.empty() && baseNormals.size()!=a.vertexCount)) return fail(error,"facial-deform-count-mismatch");
        p=basePositions; n=baseNormals;
        bool active=false;
        for (size_t target=0;target<weights.size();++target)
        {
            if (!weight(weights[target])) return fail(error,"facial-invalid-weight");
            const TARGET &t=a.targets[target];
            if (t.positions.size()!=p.size() || (!n.empty() && t.normals.size()!=n.size()))
                return fail(error,"facial-normal-deltas-required");
            const float w=weights[target];
            if (w==0) continue;
            active=true;
            if (t.sparseReady)
            {
                for (const uint32_t i : t.activeVertices)
                {
                    p[i] += t.positions[i] * w;
                    if (!n.empty()) n[i] += t.normals[i] * w;
                }
            }
            else
            {
                for (size_t i=0;i<p.size();++i)
                {
                    p[i]+=t.positions[i]*w;
                    if (!n.empty()) n[i]+=t.normals[i]*w;
                }
            }
        }
        for (size_t i=0;i<p.size();++i)
        {
            if (!finite(p[i])) return fail(error,"facial-deformed-position-overflow");
            if (n.empty()) continue;
            const float length=n[i].x*n[i].x+n[i].y*n[i].y+n[i].z*n[i].z;
            if (!std::isfinite(length) || length<1e-12f) return fail(error,"facial-degenerate-normal");
            if (active) n[i] = n[i] * (1.0f/std::sqrt(length));
        }
        error.clear(); return true;
    }
}
