#include "mesh-v8-io.h"
#include "mesh-io-primitives.h"

#include <cstring>

using namespace util::le_io;

namespace util
{
    bool readHeaderV8(FILE *fp, util::HEADER &out)
    {
        int32_t version = 0;
        uint32_t magic = 0;
        int32_t reserved = 0;
        int32_t backBufferWidth = 0;
        int32_t backBufferHeight = 0;
        int32_t extraHeader = 0;
        if (!readBytes(fp, out.name, sizeof(out.name)) ||
            !readBytes(fp, out.typeApp, sizeof(out.typeApp)) ||
            !readI32LE(fp, version) ||
            !readU32LE(fp, magic) ||
            !readI32LE(fp, reserved) ||
            !readI32LE(fp, backBufferWidth) ||
            !readI32LE(fp, backBufferHeight) ||
            !readI32LE(fp, extraHeader))
        {
            return false;
        }
        out.version = version;
        out.magic = magic;
        out.reserved = reserved;
        out.backBufferWidth = backBufferWidth;
        out.backBufferHeight = backBufferHeight;
        out.extraHeader = extraHeader;
        return true;
    }

    bool writeHeaderV8(FILE *fp, const util::HEADER &in)
    {
        return writeBytes(fp, in.name, sizeof(in.name)) &&
               writeBytes(fp, in.typeApp, sizeof(in.typeApp)) &&
             writeI32LE(fp, in.version) &&
             writeU32LE(fp, in.magic) &&
             writeI32LE(fp, in.reserved) &&
             writeI32LE(fp, in.backBufferWidth) &&
             writeI32LE(fp, in.backBufferHeight) &&
             writeI32LE(fp, in.extraHeader);
    }

    bool readHeaderImgV8(FILE *fp, util::HEADER_IMG &out)
    {
        if (!readU32LE(fp, out.width) ||
            !readU32LE(fp, out.height) ||
            !readU16LE(fp, out.depth) ||
            !readU16LE(fp, out.channel) ||
            !readU32LE(fp, out.lenght) ||
            !readBytes(fp, out.alphaColor, sizeof(out.alphaColor)))
        {
            return false;
        }
        return true;
    }

    bool writeHeaderImgV8(FILE *fp, const util::HEADER_IMG &in)
    {
        return writeU32LE(fp, in.width) &&
               writeU32LE(fp, in.height) &&
               writeU16LE(fp, in.depth) &&
               writeU16LE(fp, in.channel) &&
               writeU32LE(fp, in.lenght) &&
               writeBytes(fp, in.alphaColor, sizeof(in.alphaColor));
    }

    bool readDetailMeshV8(FILE *fp, util::DETAIL_MESH &out)
    {
        int32_t type = 0;
        int32_t totalBounding = 0;
        if (!readI32LE(fp, type) ||
            !readI32LE(fp, totalBounding))
        {
            return false;
        }
        out.type = type;
        out.totalBounding = totalBounding;
        return true;
    }

    bool writeDetailMeshV8(FILE *fp, const util::DETAIL_MESH &in)
    {
        return writeI32LE(fp, in.type) &&
               writeI32LE(fp, in.totalBounding);
    }

    bool readVec2V8(FILE *fp, mbm::VEC2 &out)
    {
        return readF32LE(fp, out.x) &&
               readF32LE(fp, out.y);
    }

    bool writeVec2V8(FILE *fp, const mbm::VEC2 &in)
    {
        return writeF32LE(fp, in.x) &&
               writeF32LE(fp, in.y);
    }

    bool readVec3V8(FILE *fp, mbm::VEC3 &out)
    {
        return readF32LE(fp, out.x) &&
               readF32LE(fp, out.y) &&
               readF32LE(fp, out.z);
    }

    bool writeVec3V8(FILE *fp, const mbm::VEC3 &in)
    {
        return writeF32LE(fp, in.x) &&
               writeF32LE(fp, in.y) &&
               writeF32LE(fp, in.z);
    }

    bool readVec3ArrayV8(FILE *fp, mbm::VEC3 *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readVec3V8(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeVec3ArrayV8(FILE *fp, const mbm::VEC3 *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeVec3V8(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readCubeV8(FILE *fp, mbm::CUBE &out)
    {
        return readVec3V8(fp, out.halfDim) &&
               readVec3V8(fp, out.absCenter);
    }

    bool writeCubeV8(FILE *fp, const mbm::CUBE &in)
    {
        return writeVec3V8(fp, in.halfDim) &&
               writeVec3V8(fp, in.absCenter);
    }

    bool readSphereV8(FILE *fp, mbm::SPHERE &out)
    {
        return readF32LE(fp, out.ray) &&
               readF32LE(fp, out.absCenter[0]) &&
               readF32LE(fp, out.absCenter[1]) &&
               readF32LE(fp, out.absCenter[2]);
    }

    bool writeSphereV8(FILE *fp, const mbm::SPHERE &in)
    {
        return writeF32LE(fp, in.ray) &&
               writeF32LE(fp, in.absCenter[0]) &&
               writeF32LE(fp, in.absCenter[1]) &&
               writeF32LE(fp, in.absCenter[2]);
    }

    bool readCubeComplexV8(FILE *fp, mbm::CUBE_COMPLEX &out)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!readF32LE(fp, out.p[i].x) ||
                !readF32LE(fp, out.p[i].y) ||
                !readF32LE(fp, out.p[i].z))
            {
                return false;
            }
        }
        return true;
    }

    bool writeCubeComplexV8(FILE *fp, const mbm::CUBE_COMPLEX &in)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!writeF32LE(fp, in.p[i].x) ||
                !writeF32LE(fp, in.p[i].y) ||
                !writeF32LE(fp, in.p[i].z))
            {
                return false;
            }
        }
        return true;
    }

    bool readTriangleV8(FILE *fp, mbm::TRIANGLE &out)
    {
        return readVec3ArrayV8(fp, out.point, 3) &&
               readVec2V8(fp, out.position);
    }

    bool writeTriangleV8(FILE *fp, const mbm::TRIANGLE &in)
    {
        return writeVec3ArrayV8(fp, in.point, 3) &&
               writeVec2V8(fp, in.position);
    }

    bool readTriangleLegacyNoPosV8(FILE *fp, mbm::TRIANGLE &out)
    {
        out.position.x = 0.0f;
        out.position.y = 0.0f;
        return readVec3ArrayV8(fp, out.point, 3);
    }
}
