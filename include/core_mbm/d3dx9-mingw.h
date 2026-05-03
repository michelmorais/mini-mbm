/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef D3DX9_MINGW_H
#define D3DX9_MINGW_H

/*
 * MinGW compatibility shim for D3DX9 shader APIs.
 *
 * d3dx9.h is part of the legacy DirectX SDK (June 2010) and is not distributed
 * with MinGW-w64.  This header provides the small subset of D3DX9 types and
 * functions used by the mini-mbm DirectX 9 backend, implemented on top of:
 *   - d3dcompiler.h  (D3DCompile — available in MinGW-w64 via directx-headers)
 *   - d3d9.h         (IDirect3DDevice9, D3DMATRIX, etc.)
 *
 * Replacements provided:
 *   D3DXHANDLE          → LPCVOID  (register index packed as pointer)
 *   D3DXMATRIX          → D3DMATRIX (layout-identical 4×4 float matrix)
 *   ID3DXBuffer         → ID3DBlob  (same COM interface)
 *   D3DXSHADER_DEBUG    → D3DCOMPILE_DEBUG    (same numeric value 0x0001)
 *   D3DXSHADER_SKIPVALIDATION → D3DCOMPILE_SKIP_VALIDATION (0x0004)
 *   D3DX_SDK_VERSION    → 43 (DirectX SDK June 2010 value)
 *   D3DXCheckVersion()  → stub (always returns TRUE)
 *   ID3DXConstantTable  → MBM implementation: parses the CTAB section embedded
 *                         in compiled D3D9 shader bytecode; dispatches to
 *                         IDirect3DDevice9::Set{Pixel|Vertex}ShaderConstantF.
 *   D3DXCompileShader() → wrapper around D3DCompile(); creates the constant
 *                         table from the resulting bytecode.
 */

#ifdef __MINGW32__

#include <d3d9.h>
#include <d3dcompiler.h>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstring>

/* ── Type aliases ───────────────────────────────────────────────────────────── */

/* D3DX9 defines D3DXHANDLE as LPCVOID; keep the same typedef so all casts
 * compile without changes.  We encode the register index as a non-null pointer:
 *   handle = (LPCVOID)(uintptr_t)(registerIndex + 1)
 * so that register 0 is distinguishable from a "not found" nullptr. */
typedef LPCVOID D3DXHANDLE;

/* D3DXMATRIX is layout-compatible with D3DMATRIX (both are 16 contiguous
 * floats in row-major order). */
typedef D3DMATRIX D3DXMATRIX;

/* ID3DBlob has the same COM interface as ID3DXBuffer
 * (GetBufferPointer / GetBufferSize / Release). */
typedef ID3DBlob ID3DXBuffer;

/* ── Compile-flag macros ────────────────────────────────────────────────────── */

#ifndef D3DXSHADER_DEBUG
#  define D3DXSHADER_DEBUG            D3DCOMPILE_DEBUG            /* 0x0001 */
#endif
#ifndef D3DXSHADER_SKIPVALIDATION
#  define D3DXSHADER_SKIPVALIDATION   D3DCOMPILE_SKIP_VALIDATION  /* 0x0004 */
#endif
#ifndef D3DX_SDK_VERSION
#  define D3DX_SDK_VERSION 43  /* DirectX SDK June 2010 */
#endif

/* ── D3DXCheckVersion stub ──────────────────────────────────────────────────── */

/* The engine uses this as an informational "is D3DX runtime present?" check.
 * Under MinGW we bypass D3DX entirely, so always return TRUE. */
inline BOOL D3DXCheckVersion(UINT /*d3dSdkVer*/, UINT /*d3dxSdkVer*/) { return TRUE; }

/* ── Per-constant metadata stored after CTAB parsing ───────────────────────── */

struct MBM_D3DX_CONSTANT_ENTRY
{
    UINT registerIndex;   /* zero-based D3D9 register index */
    UINT registerCount;   /* number of float4 registers occupied */
    WORD registerSet;     /* 0=bool, 1=int4, 2=float4, 3=sampler */
};

/* ── ID3DXConstantTable MinGW replacement ───────────────────────────────────── */
/*
 * Parses the CTAB (constant-table) comment block embedded in D3D9 shader
 * bytecode and implements the handful of ID3DXConstantTable methods that the
 * engine uses:
 *   GetConstantByName, SetDefaults, SetFloat, SetFloatArray, SetMatrix, Release
 *
 * D3D9 bytecode CTAB layout (all values little-endian DWORD unless noted):
 *   Payload[0..3]   = FourCC "CTAB" (0x42415443)  ← ctab pointer base
 *   Payload[4..7]   = D3DXSHADER_CONSTANTTABLE.Size   (= 28)
 *   Payload[8..11]  = Creator offset  (from payload base)
 *   Payload[12..15] = Version
 *   Payload[16..19] = Constants       (number of constants)
 *   Payload[20..23] = ConstantInfo offset (from payload base)
 *   Payload[24..27] = Flags
 *   Payload[28..31] = Target offset
 *
 * Per-constant entry (20 bytes) at ctab + ConstantInfo + i*20:
 *   [0..3]   Name offset (from payload base)
 *   [4..5]   RegisterSet  (WORD)
 *   [6..7]   RegisterIndex (WORD)
 *   [8..9]   RegisterCount (WORD)
 *   [10..11] Reserved
 *   [12..15] TypeInfo offset
 *   [16..19] DefaultValue offset
 */
class ID3DXConstantTable  /* NOLINT — name intentionally matches D3DX9 API */
{
public:
    explicit ID3DXConstantTable(const void *bytecode,
                                SIZE_T      byteLen,
                                bool        isPixelShader) noexcept
        : m_isPS(isPixelShader)
    {
        if (bytecode && byteLen >= 4)
            scanBytecodeForCtab(static_cast<const DWORD *>(bytecode),
                                byteLen / sizeof(DWORD));
    }

    /* COM-style release: deletes this object and returns 0. */
    ULONG Release() { delete this; return 0; }

    /* No-op: default values are baked into the bytecode; nothing to push. */
    HRESULT SetDefaults(IDirect3DDevice9 * /*pDevice*/) { return S_OK; }

    /* Return a handle (packed register index + 1) for the named constant.
     * Returns nullptr if not found. */
    D3DXHANDLE GetConstantByName(LPCSTR /*pContainer*/, LPCSTR pName) const
    {
        if (!pName) return nullptr;
        auto it = m_constants.find(pName);
        if (it == m_constants.end()) return nullptr;
        /* +1 so that register 0 maps to a non-null handle value. */
        return reinterpret_cast<D3DXHANDLE>(
            static_cast<uintptr_t>(it->second.registerIndex + 1u));
    }

    /* Upload a single float (padded to float4) to the constant register. */
    HRESULT SetFloat(IDirect3DDevice9 *pDevice,
                     D3DXHANDLE        hConstant,
                     FLOAT             f)
    {
        if (!hConstant) return E_FAIL;
        UINT  reg     = decodeHandle(hConstant);
        FLOAT data[4] = { f, 0.0f, 0.0f, 0.0f };
        return dispatch(pDevice, reg, data, 1);
    }

    /* Upload an array of floats (padded to full float4 registers).
     * 'count' is the number of individual FLOAT elements, not register count. */
    HRESULT SetFloatArray(IDirect3DDevice9 *pDevice,
                          D3DXHANDLE        hConstant,
                          const FLOAT      *pf,
                          UINT              count)
    {
        if (!hConstant || !pf || count == 0) return E_FAIL;
        UINT  reg     = decodeHandle(hConstant);
        UINT  numRegs = (count + 3u) / 4u;
        /* Copy into a zero-padded staging buffer (max 4 registers = 16 floats). */
        FLOAT padded[16] = {};
        const UINT toCopy = (count < 16u) ? count : 16u;
        memcpy(padded, pf, toCopy * sizeof(FLOAT));
        return dispatch(pDevice, reg, padded, numRegs);
    }

    /* Upload a 4×4 matrix (4 consecutive float4 registers). */
    HRESULT SetMatrix(IDirect3DDevice9 *pDevice,
                      D3DXHANDLE        hConstant,
                      const D3DXMATRIX *pMatrix)
    {
        if (!hConstant || !pMatrix) return E_FAIL;
        UINT reg = decodeHandle(hConstant);
        /* Cast to float* — D3DMATRIX is 16 contiguous floats in row-major order,
         * which is exactly what SetVertexShaderConstantF expects. */
        return dispatch(pDevice, reg,
                        reinterpret_cast<const FLOAT *>(pMatrix), 4u);
    }

private:
    bool m_isPS;
    std::unordered_map<std::string, MBM_D3DX_CONSTANT_ENTRY> m_constants;

    /* Decode handle back to a zero-based register index. */
    static UINT decodeHandle(D3DXHANDLE h) noexcept
    {
        return static_cast<UINT>(reinterpret_cast<uintptr_t>(h)) - 1u;
    }

    /* Dispatch to the correct D3D9 constant-upload method. */
    HRESULT dispatch(IDirect3DDevice9 *pDevice,
                     UINT              reg,
                     const FLOAT      *pData,
                     UINT              numRegs) const
    {
        return m_isPS
            ? pDevice->SetPixelShaderConstantF(reg, pData, numRegs)
            : pDevice->SetVertexShaderConstantF(reg, pData, numRegs);
    }

    /* Scan the D3D9 shader bytecode for a CTAB comment and parse it.
     *
     * Bytecode format (DWORDs):
     *   [0]     version token
     *   [1…]    instruction tokens
     *             comment = 0x0000FFFE | (wordCount << 16)
     *             end     = 0x0000FFFF
     */
    void scanBytecodeForCtab(const DWORD *tokens, SIZE_T numDwords) noexcept
    {
        static const DWORD CTAB_FOURCC = 0x42415443u; /* "CTAB" */
        static const DWORD COMMENT_OP  = 0x0000FFFEu;
        static const DWORD END_OP      = 0x0000FFFFu;

        for (SIZE_T i = 1; i < numDwords; )
        {
            DWORD tok = tokens[i];
            if ((tok & 0x0000FFFFu) == END_OP) break;

            if ((tok & 0x0000FFFFu) == COMMENT_OP)
            {
                DWORD wordCount = (tok >> 16u) & 0x7FFFu;
                if (wordCount > 0 &&
                    (i + wordCount) < numDwords &&
                    tokens[i + 1] == CTAB_FOURCC)
                {
                    /* tokens[i+1] is the FourCC — that IS the ctab base pointer.
                     * All offsets in the CTAB are measured from this address. */
                    const BYTE *ctab    = reinterpret_cast<const BYTE *>(&tokens[i + 1]);
                    SIZE_T      ctabLen = static_cast<SIZE_T>(wordCount) * sizeof(DWORD);
                    parseCtabBlock(ctab, ctabLen);
                    return; /* only one CTAB comment per shader */
                }
                i += 1u + wordCount;
            }
            else
            {
                ++i;
            }
        }
    }

    /* Parse the raw CTAB binary block (ctab[0..3] = FourCC "CTAB"). */
    void parseCtabBlock(const BYTE *ctab, SIZE_T ctabLen) noexcept
    {
        /* Minimum: FourCC(4) + D3DXSHADER_CONSTANTTABLE header (28) = 32 bytes. */
        if (ctabLen < 32u) return;

        /* ctab[0..3] = FourCC "CTAB" (4 bytes, NOT part of the struct).
         * D3DXSHADER_CONSTANTTABLE starts at ctab+4; ALL offsets stored inside
         * the CTAB fields (ConstantInfo, Creator, Name, …) are relative to this
         * base address — NOT to ctab itself.
         *
         * D3DXSHADER_CONSTANTTABLE layout (starting at hdr = ctab+4):
         *   hdr+0   Size         (4)
         *   hdr+4   Creator      (4) ← offset from hdr
         *   hdr+8   Version      (4)
         *   hdr+12  Constants    (4) ← number of constants
         *   hdr+16  ConstantInfo (4) ← offset from hdr to first entry
         *   hdr+20  Flags        (4)
         *   hdr+24  Target       (4)
         */
        const BYTE *hdr = ctab + 4u;
        const SIZE_T hdrLen = ctabLen - 4u; /* usable space starting at hdr */

        DWORD numConstants    = *reinterpret_cast<const DWORD *>(hdr + 12u);
        DWORD constantInfoOff = *reinterpret_cast<const DWORD *>(hdr + 16u);

        if (numConstants == 0) return;

        /* Bounds check: constant array must fit inside the CTAB block. */
        if (static_cast<SIZE_T>(constantInfoOff) + numConstants * 20u > hdrLen) return;

        for (DWORD c = 0; c < numConstants; ++c)
        {
            const BYTE *ci = hdr + constantInfoOff + c * 20u;

            /* Per-constant fields (see struct layout in file header comment). */
            DWORD nameOff  = *reinterpret_cast<const DWORD *>(ci + 0u);
            WORD  regSet   = *reinterpret_cast<const WORD  *>(ci + 4u);
            WORD  regIdx   = *reinterpret_cast<const WORD  *>(ci + 6u);
            WORD  regCount = *reinterpret_cast<const WORD  *>(ci + 8u);

            /* nameOff is also relative to hdr. */
            if (static_cast<SIZE_T>(nameOff) >= hdrLen) continue;
            const char *name = reinterpret_cast<const char *>(hdr + nameOff);
            if (!name || name[0] == '\0') continue;

            MBM_D3DX_CONSTANT_ENTRY entry;
            entry.registerIndex = static_cast<UINT>(regIdx);
            entry.registerCount = static_cast<UINT>(regCount);
            entry.registerSet   = regSet;
            m_constants.emplace(name, entry);
        }
    }
};

/* ── D3DXCompileShader wrapper ──────────────────────────────────────────────── */
/*
 * Wraps D3DCompile() and, on success, constructs an ID3DXConstantTable from
 * the compiled bytecode.  The signature matches the D3DX9 version.
 *
 * Note: pDefines and pInclude are accepted but ignored; the engine always
 * passes nullptr for both.
 */
inline HRESULT D3DXCompileShader(
    LPCSTR              pSrcData,
    UINT                srcDataLen,
    const void *        /*pDefines*/,
    const void *        /*pInclude*/,
    LPCSTR              pFunctionName,
    LPCSTR              pTarget,
    DWORD               flags,
    ID3DXBuffer       **ppShader,
    ID3DXBuffer       **ppErrorMsgs,
    ID3DXConstantTable **ppConstantTable)
{
    HRESULT hr = D3DCompile(
        pSrcData, srcDataLen,
        nullptr,  /* pSourceName  — no filename needed */
        nullptr,  /* pDefines     — no macro expansion */
        nullptr,  /* pInclude     — no #include support */
        pFunctionName, pTarget,
        static_cast<UINT>(flags), 0u,
        ppShader, ppErrorMsgs);

    if (ppConstantTable)
    {
        if (SUCCEEDED(hr) && ppShader && *ppShader)
        {
            bool isPS = (pTarget[0] == 'p' || pTarget[0] == 'P');
            *ppConstantTable = new ID3DXConstantTable(
                (*ppShader)->GetBufferPointer(),
                (*ppShader)->GetBufferSize(),
                isPS);
        }
        else
        {
            *ppConstantTable = nullptr;
        }
    }
    return hr;
}

/* ── D3DX9 texture filter / default constants ──────────────────────────────── */

#define D3DX_DEFAULT         ((UINT) -1)   /* let D3DX choose */
#define D3DX_FILTER_NONE     0x00000001u   /* no filtering          */
#define D3DX_FILTER_POINT    0x00000002u   /* nearest-neighbour     */
#define D3DX_FILTER_LINEAR   0x00000003u   /* bilinear              */
#define D3DX_FILTER_TRIANGLE 0x00000004u   /* triangle filter       */
#define D3DX_FILTER_BOX      0x00000005u   /* box / average filter  */

/* ── D3DXIMAGE_INFO ─────────────────────────────────────────────────────────── */

typedef enum D3DXIMAGE_FILEFORMAT {
    D3DXIFF_BMP         = 0,
    D3DXIFF_JPG         = 1,
    D3DXIFF_TGA         = 2,
    D3DXIFF_PNG         = 3,
    D3DXIFF_DDS         = 4,
    D3DXIFF_FORCE_DWORD = 0x7fffffff
} D3DXIMAGE_FILEFORMAT;

struct D3DXIMAGE_INFO {
    UINT                 Width;
    UINT                 Height;
    UINT                 Depth;
    UINT                 MipLevels;
    D3DFORMAT            Format;
    D3DRESOURCETYPE      ResourceType;
    D3DXIMAGE_FILEFORMAT ImageFileFormat;
};

/* ── stbi forward declarations (implementation lives in third-party/stb/stb.c) */

extern "C" {
    extern unsigned char *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern int            stbi_info(char const *filename, int *x, int *y, int *comp);
    extern void           stbi_image_free(void *retval_from_stbi_load);
}

/* ── D3DXGetImageInfoFromFileA ──────────────────────────────────────────────── */

inline HRESULT D3DXGetImageInfoFromFileA(LPCSTR pSrcFile, D3DXIMAGE_INFO *pSrcInfo)
{
    if (!pSrcFile || !pSrcInfo) return E_POINTER;
    int w = 0, h = 0, comp = 0;
    if (!stbi_info(pSrcFile, &w, &h, &comp))
        return D3DERR_INVALIDCALL;
    pSrcInfo->Width           = static_cast<UINT>(w);
    pSrcInfo->Height          = static_cast<UINT>(h);
    pSrcInfo->Depth           = 1u;
    pSrcInfo->MipLevels       = 1u;
    pSrcInfo->Format          = D3DFMT_A8R8G8B8;
    pSrcInfo->ResourceType    = D3DRTYPE_TEXTURE;
    pSrcInfo->ImageFileFormat = D3DXIFF_PNG; /* approximate — not used by engine */
    return S_OK;
}

/* ── D3DXCreateTextureFromFileExA ───────────────────────────────────────────── */
/*
 * Loads an image via stb_image and uploads it into a new IDirect3DTexture9.
 * stb_image always gives RGBA8; D3DFMT_A8R8G8B8 memory order is B,G,R,A so
 * we swap channels during the copy.  D3DFMT_UNKNOWN is treated as A8R8G8B8.
 */
inline HRESULT D3DXCreateTextureFromFileExA(
    IDirect3DDevice9  *pDevice,
    LPCSTR             pSrcFile,
    UINT               Width,
    UINT               Height,
    UINT               MipLevels,
    DWORD              /*Usage*/,
    D3DFORMAT          Format,
    D3DPOOL            Pool,
    DWORD              /*Filter*/,
    DWORD              /*MipFilter*/,
    D3DCOLOR           /*ColorKey*/,
    D3DXIMAGE_INFO    *pSrcInfo,
    PALETTEENTRY      * /*pPalette*/,
    IDirect3DTexture9 **ppTexture)
{
    if (!pDevice || !pSrcFile || !ppTexture) return E_POINTER;
    *ppTexture = nullptr;

    int imgW = 0, imgH = 0, comp = 0;
    unsigned char *pixels = stbi_load(pSrcFile, &imgW, &imgH, &comp, 4);
    if (!pixels) return D3DERR_INVALIDCALL;

    if (Width  == 0u || Width  == D3DX_DEFAULT) Width  = static_cast<UINT>(imgW);
    if (Height == 0u || Height == D3DX_DEFAULT) Height = static_cast<UINT>(imgH);
    if (MipLevels == D3DX_DEFAULT)              MipLevels = 1u;

    D3DFORMAT texFmt = (Format == D3DFMT_UNKNOWN) ? D3DFMT_A8R8G8B8 : Format;

    IDirect3DTexture9 *pTex = nullptr;
    HRESULT hr = pDevice->CreateTexture(Width, Height, MipLevels, 0u, texFmt, Pool, &pTex, nullptr);
    if (FAILED(hr)) { stbi_image_free(pixels); return hr; }

    D3DLOCKED_RECT rc;
    hr = pTex->LockRect(0, &rc, nullptr, 0);
    if (FAILED(hr)) { pTex->Release(); stbi_image_free(pixels); return hr; }

    /* stb_image: RGBA byte order.  D3DFMT_A8R8G8B8 memory layout: B G R A. */
    const UINT copyW = (Width  < static_cast<UINT>(imgW)) ? Width  : static_cast<UINT>(imgW);
    const UINT copyH = (Height < static_cast<UINT>(imgH)) ? Height : static_cast<UINT>(imgH);
    auto *dst = static_cast<BYTE *>(rc.pBits);
    for (UINT y = 0; y < copyH; ++y)
    {
        BYTE       *row = dst + y * static_cast<UINT>(rc.Pitch);
        const BYTE *src = pixels + y * static_cast<UINT>(imgW) * 4u;
        for (UINT x = 0; x < copyW; ++x, src += 4, row += 4)
        {
            row[0] = src[2]; /* B */
            row[1] = src[1]; /* G */
            row[2] = src[0]; /* R */
            row[3] = src[3]; /* A */
        }
    }
    pTex->UnlockRect(0);
    stbi_image_free(pixels);

    if (pSrcInfo)
    {
        pSrcInfo->Width  = static_cast<UINT>(imgW);
        pSrcInfo->Height = static_cast<UINT>(imgH);
    }
    *ppTexture = pTex;
    return S_OK;
}

#endif /* __MINGW32__ */
#endif /* D3DX9_MINGW_H */
