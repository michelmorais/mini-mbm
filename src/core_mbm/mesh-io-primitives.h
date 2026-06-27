#ifndef MESH_IO_PRIMITIVES_H
#define MESH_IO_PRIMITIVES_H

// Shared little-endian field-by-field read/write primitives, field-blit-free (never struct-blitted),
// proven 32/64-bit-safe since mesh-v8-io.cpp. Used by both mesh-v8-io.cpp and mesh-v11-io.cpp so the
// byte-level helpers have exactly one definition.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <header-mesh.h> // util::MEM_CURSOR_V11

namespace util
{
namespace le_io
{
    inline bool hostLittleEndian() noexcept
    {
        const uint16_t one = 1;
        return *reinterpret_cast<const uint8_t*>(&one) == 1;
    }

    inline uint16_t bswap16(const uint16_t value) noexcept
    {
        return static_cast<uint16_t>((value >> 8) | (value << 8));
    }

    inline uint32_t bswap32(const uint32_t value) noexcept
    {
        return ((value & 0x000000FFu) << 24) |
               ((value & 0x0000FF00u) << 8) |
               ((value & 0x00FF0000u) >> 8) |
               ((value & 0xFF000000u) >> 24);
    }

    inline int16_t leToI16(const int16_t value) noexcept
    {
        if (hostLittleEndian())
            return value;
        const uint16_t raw = static_cast<uint16_t>(value);
        return static_cast<int16_t>(bswap16(raw));
    }

    inline uint16_t leToU16(const uint16_t value) noexcept
    {
        if (hostLittleEndian())
            return value;
        return bswap16(value);
    }

    inline int32_t leToI32(const int32_t value) noexcept
    {
        if (hostLittleEndian())
            return value;
        const uint32_t raw = static_cast<uint32_t>(value);
        return static_cast<int32_t>(bswap32(raw));
    }

    inline uint32_t leToU32(const uint32_t value) noexcept
    {
        if (hostLittleEndian())
            return value;
        return bswap32(value);
    }

    inline float leToF32(const float value) noexcept
    {
        if (hostLittleEndian())
            return value;
        uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        raw = bswap32(raw);
        float out = 0.0f;
        std::memcpy(&out, &raw, sizeof(out));
        return out;
    }

    inline bool readBytes(FILE *fp, void *value, const size_t bytes)
    {
        if (bytes == 0)
            return true;
        return std::fread(value, bytes, 1, fp) == 1;
    }

    inline bool writeBytes(FILE *fp, const void *value, const size_t bytes)
    {
        if (bytes == 0)
            return true;
        return std::fwrite(value, bytes, 1, fp) == 1;
    }

    // MEM_CURSOR_V11 itself is declared in header-mesh.h (public header-mesh.h, not this internal
    // header) - see its comment there for why.
    inline bool readBytes(MEM_CURSOR_V11 &cur, void *value, const size_t bytes)
    {
        if (bytes == 0)
            return true;
        if (cur.pos + bytes > cur.size)
            return false;
        std::memcpy(value, cur.data + cur.pos, bytes);
        cur.pos += bytes;
        return true;
    }

    inline bool readI16LE(FILE *fp, int16_t &out)
    {
        int16_t temp = 0;
        if (!readBytes(fp, &temp, sizeof(temp)))
            return false;
        out = leToI16(temp);
        return true;
    }

    inline bool writeI16LE(FILE *fp, const int16_t in)
    {
        const int16_t temp = leToI16(in);
        return writeBytes(fp, &temp, sizeof(temp));
    }

    inline bool readU16LE(FILE *fp, uint16_t &out)
    {
        uint16_t temp = 0;
        if (!readBytes(fp, &temp, sizeof(temp)))
            return false;
        out = leToU16(temp);
        return true;
    }

    inline bool writeU16LE(FILE *fp, const uint16_t in)
    {
        const uint16_t temp = leToU16(in);
        return writeBytes(fp, &temp, sizeof(temp));
    }

    inline bool readI32LE(FILE *fp, int32_t &out)
    {
        int32_t temp = 0;
        if (!readBytes(fp, &temp, sizeof(temp)))
            return false;
        out = leToI32(temp);
        return true;
    }

    inline bool writeI32LE(FILE *fp, const int32_t in)
    {
        const int32_t temp = leToI32(in);
        return writeBytes(fp, &temp, sizeof(temp));
    }

    inline bool readU32LE(FILE *fp, uint32_t &out)
    {
        uint32_t temp = 0;
        if (!readBytes(fp, &temp, sizeof(temp)))
            return false;
        out = leToU32(temp);
        return true;
    }

    inline bool writeU32LE(FILE *fp, const uint32_t in)
    {
        const uint32_t temp = leToU32(in);
        return writeBytes(fp, &temp, sizeof(temp));
    }

    inline bool readF32LE(FILE *fp, float &out)
    {
        float temp = 0.0f;
        if (!readBytes(fp, &temp, sizeof(temp)))
            return false;
        out = leToF32(temp);
        return true;
    }

    inline bool writeF32LE(FILE *fp, const float in)
    {
        const float temp = leToF32(in);
        return writeBytes(fp, &temp, sizeof(temp));
    }

    inline bool readI16LE(MEM_CURSOR_V11 &cur, int16_t &out)
    {
        int16_t temp = 0;
        if (!readBytes(cur, &temp, sizeof(temp)))
            return false;
        out = leToI16(temp);
        return true;
    }

    inline bool readU16LE(MEM_CURSOR_V11 &cur, uint16_t &out)
    {
        uint16_t temp = 0;
        if (!readBytes(cur, &temp, sizeof(temp)))
            return false;
        out = leToU16(temp);
        return true;
    }

    inline bool readI32LE(MEM_CURSOR_V11 &cur, int32_t &out)
    {
        int32_t temp = 0;
        if (!readBytes(cur, &temp, sizeof(temp)))
            return false;
        out = leToI32(temp);
        return true;
    }

    inline bool readU32LE(MEM_CURSOR_V11 &cur, uint32_t &out)
    {
        uint32_t temp = 0;
        if (!readBytes(cur, &temp, sizeof(temp)))
            return false;
        out = leToU32(temp);
        return true;
    }

    inline bool readF32LE(MEM_CURSOR_V11 &cur, float &out)
    {
        float temp = 0.0f;
        if (!readBytes(cur, &temp, sizeof(temp)))
            return false;
        out = leToF32(temp);
        return true;
    }
}
}

#endif
