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

#include <private/normal-map-asset.h>
#include <mesh-v11-io.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/physics.h>
#include <core_mbm/shapes.h>
#include <core_mbm/draw-compatibility.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <vector>
#include <limits>

namespace
{
    using namespace mbm;
    int failures = 0;
    void expect(bool condition, const char *message)
    {
        if (!condition) { std::fprintf(stderr, "[normal-map-persistence] FAIL: %s\n", message); ++failures; }
    }
    struct SECTION { util::SECTION_HEADER_V11 header; std::vector<uint8_t> bytes; };
    struct FILE_DATA { util::FILE_HEADER_V11 header; std::vector<SECTION> sections; };

    bool readFile(const std::filesystem::path &path, FILE_DATA &data)
    {
        FILE *file = std::fopen(path.string().c_str(), "rb");
        if (!file) return false;
        bool ok = util::readFileHeaderV11(file, data.header);
        data.sections.clear();
        for (uint32_t i = 0; ok && i < data.header.sectionCount; ++i)
        {
            SECTION s;
            ok = util::readSectionV11(file, s.header, s.bytes);
            if (ok) data.sections.push_back(std::move(s));
        }
        std::fclose(file);
        return ok;
    }

    bool writeFile(const std::filesystem::path &path, FILE_DATA data)
    {
        FILE *file = std::fopen(path.string().c_str(), "wb");
        if (!file) return false;
        data.header.sectionCount = static_cast<uint32_t>(data.sections.size());
        bool ok = util::writeFileHeaderV11(file, data.header);
        for (auto &s : data.sections)
            ok = ok && util::writeSectionV11(file, s.header, s.bytes.data(), static_cast<uint32_t>(s.bytes.size()));
        std::fclose(file);
        return ok;
    }

    size_t tangentSection(const FILE_DATA &file)
    {
        for (size_t i = 0; i < file.sections.size(); ++i)
            if (file.sections[i].header.type == util::SECTION_NORMAL_MAP_TANGENTS) return i;
        return file.sections.size();
    }

    void setU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
    {
        for (size_t i = 0; i < 4; ++i) bytes[offset+i] = static_cast<uint8_t>(value >> (i*8));
    }
    void setFloat(std::vector<uint8_t> &bytes, size_t offset, float value)
    {
        uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); setU32(bytes, offset, bits);
    }

    void addTriangle(MESH_MBM_DEBUG &mesh, bool withMap)
    {
        const uint32_t f = mesh.getTotalFrames();
        mesh.addBuffer();
        mesh.addSubset(f);
        auto *frame = mesh.getFrameBuffer(f);
        frame->position = reinterpret_cast<float *>(new VEC3[3]{VEC3(0,0,0), VEC3(1,0,0), VEC3(0,1,0)});
        frame->normal = reinterpret_cast<float *>(new VEC3[3]{VEC3(0,0,1), VEC3(0,0,1), VEC3(0,0,1)});
        frame->uv = reinterpret_cast<float *>(new VEC2[3]{VEC2(0,0), VEC2(1,0), VEC2(0,1)});
        frame->headerFrame.sizeVertexBuffer = 3;
        frame->headerFrame.totalSubset = 1;
        auto *subset = mesh.getSubset(f, 0);
        subset->vertexCount = 3;
        subset->vertexStart = 0;
        subset->indexCount = 0;
        subset->indexStart = 0;
        subset->texture = "#FFFFFFFF";
        if (withMap)
        {
            util::MATERIAL_TEXTURE_SLOT_DEBUG slot;
            slot.type = util::MATERIAL_TEXTURE_SLOT_NORMAL;
            slot.texture = "#8080FFFF";
            subset->materialTextureSlots.push_back(slot);
        }
        mesh.setHasNormal(HAS_NOR_IN_FILE);
        mesh.setHasTexture(HAS_TEX_EACH_FRAME);
        mesh.setMeshType(util::TYPE_MESH_3D);
        mesh.setModeDraw(util::MODE_DRAW_TRIANGLES);
    }

    bool save(MESH_MBM_DEBUG &mesh, const std::filesystem::path &path, bool compress)
    {
        char error[512] = {};
        bool ok = mesh.saveV11(path.string().c_str(), false, false, compress, error, sizeof(error));
        if (!ok) std::fprintf(stderr, "save failed: %s\n", error);
        return ok;
    }
    void testAuthoringIdentity(const std::filesystem::path &dir)
    {
        MESH_MBM_DEBUG mesh;
        addTriangle(mesh, false);
        auto *frame = mesh.getFrameBuffer(0);
        delete[] frame->position; delete[] frame->normal; delete[] frame->uv;
        frame->position = new float[12]{0,0,0, 1,0,0, 0,1,0, 1,1,0};
        frame->normal = new float[12]{0,0,1, 0,0,1, 0,0,1, 0,0,1};
        frame->uv = new float[8]{0,0, 1,0, 0,1, 0,0};
        frame->indexBuffer = new uint16_t[6]{0,1,2,2,1,3};
        frame->headerFrame.sizeVertexBuffer = 4;
        frame->headerFrame.sizeIndexBuffer = 6;
        std::strcpy(frame->headerFrame.typeBuffer, "IB");
        mesh.getSubset(0,0)->vertexCount = 4;
        mesh.getSubset(0,0)->indexCount = 6;
        auto *triangle = new TRIANGLE();
        for (uint32_t i = 0; i < 3; ++i) triangle->point[i] = mesh.getPositionArray(0)[i];
        mesh.getPhysicsInfo().lsTriangle.push_back(triangle);
        FILE_DATA before, after;
        if (!save(mesh,dir/"seam-source.msh",false) || !readFile(dir/"seam-source.msh",before))
        { expect(false,"save seam source"); return; }
        auto *positions = mesh.getPositionArray(0);
        auto *indices = mesh.getIndexArray(0);
        NORMAL_MAP_REPORT report;
        char error[512] = {};
        expect(mesh.prepareNormalMap(0,0,NORMAL_MAP_POLICY::GENERATE,report,error,sizeof(error)) && report.vertices == 6,
               "mirrored source quad prepares six render vertices");
        expect(mesh.getPositionArray(0) == positions && mesh.getIndexArray(0) == indices &&
               mesh.getSubset(0,0)->vertexCount == 4 && mesh.getSubset(0,0)->indexCount == 6,
               "preparation leaves editor selection arrays and source ranges intact");
        expect(mesh.getPhysicsInfo().lsTriangle[0] == triangle && triangle->point[1].x == 1,
               "preparation leaves physics geometry intact");
        if (!save(mesh,dir/"mirrored-seam.msh",true) || !readFile(dir/"mirrored-seam.msh",after))
        { expect(false,"save prepared seam"); return; }
        for (uint16_t type : {util::SECTION_FRAME_STATIC,util::SECTION_DETAIL_PHYSICS})
        {
            const auto find = [type](const FILE_DATA &data) -> const SECTION *
            { for (const auto &s : data.sections) if (s.header.type == type) return &s; return nullptr; };
            const auto *a = find(before), *b = find(after);
            expect(a && b && a->bytes == b->bytes, "prepared export preserves geometry and physics payload bytes");
        }
        expect(tangentSection(before) == before.sections.size() && tangentSection(after) < after.sections.size(),
               "only prepared export gains optional tangent section");
        MESH_MBM_DEBUG loaded;
        expect(loaded.loadV11((dir/"mirrored-seam.msh").string().c_str()), "load seam export");
        expect(loaded.getSubset(0,0) && loaded.getSubset(0,0)->vertexCount == 4 &&
               loaded.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,report,error,sizeof(error)) &&
               report.reused && report.vertices == 6, "reload retains separate source and prepared vertex counts");
    }
}

int runNormalMapPersistenceTests()
{
    failures = 0;
    const char *keep = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    const auto dir = keep ? std::filesystem::path(keep) : std::filesystem::temp_directory_path() /
        ("mini-mbm-normal-map-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    testAuthoringIdentity(dir);
    MESH_MBM_DEBUG mesh;
    addTriangle(mesh, false);
    FILE_DATA noMap;
    expect(save(mesh, dir/"no-map.msh", false) && readFile(dir/"no-map.msh", noMap), "save mesh without normal map");
    expect(tangentSection(noMap) == noMap.sections.size(), "no normal map emits no tangent section");
    util::MATERIAL_TEXTURE_SLOT_DEBUG slot;
    slot.type = util::MATERIAL_TEXTURE_SLOT_NORMAL; slot.texture = "#8080FFFF";
    mesh.getSubset(0,0)->materialTextureSlots.push_back(slot);
    FILE_DATA original;
    if (!save(mesh, dir/"prepared.msh", false) || !readFile(dir/"prepared.msh", original) ||
        tangentSection(original) == original.sections.size())
    {
        expect(false, "normal map emits optional tangent section");
        return 1;
    }
    const size_t section = tangentSection(original);
    const auto originalBytes = original.sections[section].bytes;
    expect(original.sections[section].header.sectionVersion == 1, "section version is one");
    normal_map::ASSET_FRAME prepared;
    uint32_t frameIndex = 99;
    std::string error;
    util::MEM_CURSOR_V11 cursor{originalBytes.data(), originalBytes.size(), 0};
    expect(normal_map::readPayload(cursor, 1, frameIndex, prepared, error) && frameIndex == 0 &&
        prepared.prepared.batches.size() == 1, "decode persisted prepared frame");
    for (size_t length = 0; length < originalBytes.size(); ++length)
    {
        normal_map::ASSET_FRAME untouched;
        untouched.sourceSignature = 123;
        uint32_t untouchedIndex = 99;
        util::MEM_CURSOR_V11 truncated{originalBytes.data(), length, 0};
        expect(!normal_map::readPayload(truncated, 1, untouchedIndex, untouched, error) &&
               untouchedIndex == 99 && untouched.sourceSignature == 123,
               "all truncated payload prefixes fail without publishing partial data");
    }
    for (bool compressed : {false, true})
    {
        MESH_MBM_DEBUG loaded;
        expect(loaded.loadV11((dir/"prepared.msh").string().c_str()), "authoring reader loads prepared mesh");
        expect(save(loaded, dir/"roundtrip.msh", compressed), "save loaded prepared mesh");
        FILE_DATA roundtrip;
        expect(readFile(dir/"roundtrip.msh", roundtrip), "read roundtrip sections");
        size_t found = tangentSection(roundtrip);
        expect(found < roundtrip.sections.size() && roundtrip.sections[found].bytes == originalBytes,
               "tangent payload survives compressed and uncompressed roundtrip");
    }
    // A valid imported basis different from Mikk's +X proves save/load do not regenerate it.
    FILE_DATA imported = original;
    for (size_t i = 0; i < 3; ++i)
    {
        setFloat(imported.sections[section].bytes, 40+i*20, 0);
        setFloat(imported.sections[section].bytes, 44+i*20, 1);
    }
    expect(writeFile(dir/"imported.msh", imported), "write valid imported tangent fixture");
    MESH_MBM_DEBUG importedMesh;
    expect(importedMesh.loadV11((dir/"imported.msh").string().c_str()) &&
           save(importedMesh, dir/"imported-roundtrip.msh", true), "roundtrip imported basis");
    FILE_DATA importedRoundtrip;
    expect(readFile(dir/"imported-roundtrip.msh", importedRoundtrip), "read imported roundtrip");
    size_t found = tangentSection(importedRoundtrip);
    expect(found < importedRoundtrip.sections.size() && importedRoundtrip.sections[found].bytes == imported.sections[section].bytes,
           "imported bases are preserved rather than regenerated");
    importedMesh.getPositionArray(0)[1].x = 2;
    expect(save(importedMesh, dir/"edited.msh", false), "save edited geometry");
    FILE_DATA edited;
    expect(readFile(dir/"edited.msh", edited), "read edited sections");
    found = tangentSection(edited);
    expect(found < edited.sections.size() && edited.sections[found].bytes != imported.sections[section].bytes,
           "source change invalidates prepared cache");
    MESH_MBM_DEBUG editedReload;
    expect(editedReload.loadV11((dir/"edited.msh").string().c_str()), "edited prepared mesh reloads");

    FILE_DATA absent = original;
    absent.sections.erase(absent.sections.begin()+static_cast<std::ptrdiff_t>(section));
    expect(writeFile(dir/"unprepared.msh", absent), "write normal-map mesh without tangent section");
    MESH_MBM_DEBUG unprepared;
    expect(unprepared.loadV11((dir/"unprepared.msh").string().c_str()), "missing section is valid");
    expect(save(unprepared, dir/"regenerated.msh", false), "save generates missing basis");

    MESH_MBM_DEBUG removable;
    addTriangle(removable, true);
    addTriangle(removable, true);
    util::MATERIAL_TEXTURE_SLOT_DEBUG otherSlot;
    otherSlot.type = util::MATERIAL_TEXTURE_SLOT_SPECULAR;
    otherSlot.texture = "#FFFFFFFF";
    removable.getSubset(0, 0)->materialTextureSlots.push_back(otherSlot);
    expect(removable.setNormalMapSettings(0, 0, -1, 2.5f), "set removal fixture material");
    expect(save(removable, dir/"remove-before.msh", false), "save prepared removal fixture");
    expect(removable.hasNormalMapTangents(), "removal fixture has tangents");
    expect(removable.removeNormalMap(), "remove complete normal mapping");
    expect(!removable.hasNormalMapTangents(), "all tangent frames removed");
    expect(!removable.removeNormalMap(), "repeated removal is unchanged");
    for (const bool compressed : {false, true})
    {
        const auto removedPath = dir/(compressed ? "removed-compressed.msh" : "removed.msh");
        expect(save(removable, removedPath, compressed), "save removed normal mapping");
        FILE_DATA removalAfter;
        expect(readFile(removedPath, removalAfter), "read removed normal mapping");
        for (const auto &part : removalAfter.sections)
            expect(part.header.type != util::SECTION_NORMAL_MAP_TANGENTS &&
                   part.header.type != util::SECTION_NORMAL_MAP_MATERIALS, "no normal-map sections remain");
        MESH_MBM_DEBUG reopened, originalRemoval;
        expect(reopened.loadV11(removedPath.string().c_str()) && !reopened.hasNormalMapTangents(),
               "reopen retains no tangents");
        expect(originalRemoval.loadV11((dir/"remove-before.msh").string().c_str()), "load undo snapshot");
        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            int sign = 0; float strength = 0;
            expect(reopened.getNormalMapSettings(frame, 0, sign, strength) && sign == 1 && strength == 1,
                   "normal-map settings return to defaults");
            const auto *subset = reopened.getSubset(frame, 0);
            expect(subset->texture == "#FFFFFFFF", "diffuse texture retained");
            expect(subset->materialTextureSlots.size() == (frame == 0 ? 1u : 0u), "only normal texture references removed");
            for (const auto &slot : subset->materialTextureSlots)
                expect(slot.type == util::MATERIAL_TEXTURE_SLOT_SPECULAR && slot.texture == "#FFFFFFFF",
                       "other texture roles retained");
            const auto *before = originalRemoval.getFrameBuffer(frame);
            const auto *after = reopened.getFrameBuffer(frame);
            expect(std::memcmp(before->position, after->position, 9*sizeof(float)) == 0 &&
                   std::memcmp(before->normal, after->normal, 9*sizeof(float)) == 0 &&
                   std::memcmp(before->uv, after->uv, 6*sizeof(float)) == 0,
                   "source positions, normals and UVs retained");
        }
        expect(save(reopened, dir/"removed-resaved.msh", compressed), "resave stripped mesh");
        expect(readFile(dir/"removed-resaved.msh", removalAfter) &&
               tangentSection(removalAfter) == removalAfter.sections.size(), "resave does not regenerate tangents");
        expect(originalRemoval.hasNormalMapTangents(), "undo snapshot retains tangents");
        int sign = 0; float strength = 0;
        expect(originalRemoval.getNormalMapSettings(0, 0, sign, strength) && sign == -1 && strength == 2.5f,
               "undo snapshot retains normal-map properties");
        NORMAL_MAP_REPORT restoredReport; char restorationError[512] = {};
        expect(reopened.prepareNormalMap(0, 0, NORMAL_MAP_POLICY::PRESERVE, restoredReport,
                                         restorationError, sizeof(restorationError)), "explicit preparation after removal");
    }
    FILE_DATA reordered = original;
    std::rotate(reordered.sections.begin(), reordered.sections.begin()+static_cast<std::ptrdiff_t>(section), reordered.sections.end());
    expect(writeFile(dir/"reordered.msh", reordered), "write tangent section before source frame");
    MESH_MBM_DEBUG reorderMesh;
    expect(reorderMesh.loadV11((dir/"reordered.msh").string().c_str()), "section order does not affect validation");

    const auto reject = [&](FILE_DATA bad, const char *name)
    {
        expect(writeFile(dir/name, bad), "write corruption fixture with valid section CRC");
        MESH_MBM_DEBUG rejected;
        expect(!rejected.loadV11((dir/name).string().c_str()), name);
    };
    FILE_DATA bad = original; bad.sections.push_back(bad.sections[section]); reject(bad, "duplicate.msh");
    bad = original; setU32(bad.sections[section].bytes, 0, 99); reject(bad, "bad-frame.msh");
    bad = original; bad.sections[section].header.sectionVersion = 2; reject(bad, "bad-version.msh");
    bad = original; bad.sections[section].bytes[8] ^= 1; reject(bad, "stale-source.msh");
    bad = original; setU32(bad.sections[section].bytes, 20, UINT32_MAX); reject(bad, "bad-count.msh");
    bad = original; setU32(bad.sections[section].bytes, 36, 99); reject(bad, "bad-source-index.msh");
    bad = original; setU32(bad.sections[section].bytes, 40, 0x7fc00000u); reject(bad, "nan-tangent.msh");
    bad = original; setFloat(bad.sections[section].bytes, 52, 0); reject(bad, "bad-sign.msh");
    bad = original; bad.sections[section].bytes.pop_back(); reject(bad, "truncated.msh");
    bad = original; bad.sections[section].bytes.push_back(0); reject(bad, "trailing.msh");
    bad = original; bad.sections[section].bytes[96] = 99; reject(bad, "bad-local-index.msh");
    bad = original; bad.sections[section].bytes[96] = 1; reject(bad, "wrong-triangle.msh");

    // Shared UVs must hash the actual serialized source (frame zero), not stale local UVs.
    MESH_MBM_DEBUG shared;
    addTriangle(shared, true); addTriangle(shared, true);
    shared.setHasTexture(HAS_TEX_FIRST_FRAME);
    shared.getUvArray(1)[1].x = 8;
    expect(save(shared, dir/"shared-uv.msh", true), "save prepared frames sharing UVs");
    MESH_MBM_DEBUG sharedReload;
    expect(sharedReload.loadV11((dir/"shared-uv.msh").string().c_str()), "shared UV signatures match serialized data");
    MESH_MBM_DEBUG copied;
    expect(copied.loadV11((dir/"imported.msh").string().c_str()), "load basis for frame cloning");
    expect(copied.copyBufferFrom(copied, 0) == 2, "copy prepared frame");
    copied.removeBuffer(0);
    expect(save(copied, dir/"copied.msh", false), "save copied frame after index shift");
    FILE_DATA copiedData;
    expect(readFile(dir/"copied.msh", copiedData), "read copied frame");
    found = tangentSection(copiedData);
    expect(found < copiedData.sections.size() && copiedData.sections[found].bytes == imported.sections[section].bytes,
           "copy/remove frame preserves imported basis and source association");
    MESH_MBM_DEBUG indexed;
    addTriangle(indexed, true);
    auto *indexedFrame = indexed.getFrameBuffer(0);
    indexedFrame->indexBuffer = new uint16_t[3]{0,1,2};
    indexedFrame->headerFrame.sizeIndexBuffer = 3;
    indexed.getSubset(0,0)->indexCount = 3;
    expect(save(indexed, dir/"indexed.msh", true), "save indexed prepared mesh");
    MESH_MBM_DEBUG indexedReload;
    expect(indexedReload.loadV11((dir/"indexed.msh").string().c_str()), "reload indexed prepared mesh");
    FILE_DATA indexedData;
    expect(readFile(dir/"indexed.msh", indexedData), "read indexed basis for import fixture");
    const size_t indexedSection = tangentSection(indexedData);
    if (indexedSection < indexedData.sections.size())
    {
        for (size_t i = 0; i < 3; ++i)
        {
            setFloat(indexedData.sections[indexedSection].bytes, 40+i*20, 0);
            setFloat(indexedData.sections[indexedSection].bytes, 44+i*20, 1);
        }
        expect(writeFile(dir/"indexed-imported.msh", indexedData), "write indexed imported tangent fixture");
    }
    MESH_MBM_DEBUG mixed;
    addTriangle(mixed, false); addTriangle(mixed, true);
    expect(save(mixed, dir/"mixed-uv-source.msh", false), "save mixed material frames");
    FILE_DATA mixedData;
    expect(readFile(dir/"mixed-uv-source.msh", mixedData), "read mixed source frames");
    for (auto &part : mixedData.sections)
    {
        if (part.header.type != util::SECTION_FRAME_STATIC) continue;
        part.bytes[10] = 0; // first frame has no UV, second frame retains its own UVs
        part.bytes.erase(part.bytes.begin()+88, part.bytes.begin()+112);
        break;
    }
    expect(writeFile(dir/"mixed-uv.msh", mixedData), "write optional UV arrays per frame");
    MESH_MBM_DEBUG mixedReload;
    expect(mixedReload.loadV11((dir/"mixed-uv.msh").string().c_str()),
           "validate tangents against each frame UV flag rather than first-frame summary");
    // Material settings are independent from textures and from the tangent source signature.
    const auto materialSection = [](const FILE_DATA &data)
    {
        for (size_t i = 0; i < data.sections.size(); ++i)
            if (data.sections[i].header.type == util::SECTION_NORMAL_MAP_MATERIALS) return i;
        return data.sections.size();
    };
    const auto settingsAre = [](const MESH_MBM_DEBUG &asset, uint32_t frame, uint32_t subset, int sign, float strength)
    {
        int actualSign = 0;
        float actualStrength = -1;
        return asset.getNormalMapSettings(frame, subset, actualSign, actualStrength) &&
               actualSign == sign && actualStrength == strength;
    };
    MESH_MBM_DEBUG material;
    addTriangle(material, false);
    expect(settingsAre(material, 0, 0, 1, 1), "absent settings default to +Y and strength one");
    expect(materialSection(noMap) == noMap.sections.size(), "default settings omit optional material section");
    expect(!material.setNormalMapSettings(0,0,0,1) && !material.setNormalMapSettings(0,0,1,-1) &&
           !material.setNormalMapSettings(0,0,1,std::numeric_limits<float>::infinity()) &&
           !material.setNormalMapSettings(0,0,1,std::numeric_limits<float>::quiet_NaN()) &&
           !material.setNormalMapSettings(1,0,1,2) && !material.setNormalMapSettings(0,1,1,2) &&
           settingsAre(material,0,0,1,1), "invalid settings do not mutate state");
    int untouchedSign = 7; float untouchedStrength = 8;
    expect(!material.getNormalMapSettings(4,0,untouchedSign,untouchedStrength) &&
           untouchedSign == 7 && untouchedStrength == 8, "invalid get does not change outputs");
    expect(material.setNormalMapSettings(0,0,-1,2.5f), "set negative Y source and strength");
    FILE_DATA materialData;
    expect(save(material, dir/"material-no-map.msh", false) && readFile(dir/"material-no-map.msh", materialData),
           "save settings without assigning texture");
    const size_t materialIndex = materialSection(materialData);
    expect(materialIndex < materialData.sections.size() && tangentSection(materialData) == materialData.sections.size() &&
           material.getSubset(0,0)->materialTextureSlots.empty(), "settings alone require neither normal texture nor tangents");
    MESH_MBM_DEBUG materialReload;
    expect(materialReload.loadV11((dir/"material-no-map.msh").string().c_str()) && settingsAre(materialReload,0,0,-1,2.5f),
           "material round-trip without tangent section");
    FILE_DATA compressedMaterial;
    expect(save(materialReload,dir/"material-compressed.msh",true) && readFile(dir/"material-compressed.msh", compressedMaterial),
           "compress material settings");
    expect(materialSection(compressedMaterial) < compressedMaterial.sections.size() && materialIndex < materialData.sections.size() &&
           compressedMaterial.sections[materialSection(compressedMaterial)].bytes == materialData.sections[materialIndex].bytes,
           "compressed material payload round-trip");
    if (materialIndex < materialData.sections.size())
    {
        const auto bytes = materialData.sections[materialIndex].bytes;
        expect(bytes.size() == 20, "one material entry uses 20 bytes including count");
        for (size_t length = 0; length < bytes.size(); ++length)
        {
            normal_map::MATERIAL_SETTINGS_MAP untouched{{{8,9},{-1,3}}};
            util::MEM_CURSOR_V11 shortCursor{bytes.data(),length,0};
            expect(!normal_map::readMaterialPayload(shortCursor,1,untouched,error) &&
                   untouched.size()==1 && untouched.begin()->first.first==8, "truncated materials never publish partial data");
        }
        bad = materialData; bad.sections.push_back(bad.sections[materialIndex]); reject(bad,"material-duplicate-section.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,4,99); reject(bad,"material-bad-frame.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,8,99); reject(bad,"material-bad-subset.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,12,2); reject(bad,"material-bad-convention.msh");
        bad = materialData; setFloat(bad.sections[materialIndex].bytes,16,-1); reject(bad,"material-negative-strength.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,16,0x7fc00000u); reject(bad,"material-nan.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,16,0x7f800000u); reject(bad,"material-infinity.msh");
        bad = materialData; setU32(bad.sections[materialIndex].bytes,0,UINT32_MAX); reject(bad,"material-count.msh");
        bad = materialData; bad.sections[materialIndex].header.sectionVersion=2; reject(bad,"material-version.msh");
        bad = materialData; bad.sections[materialIndex].bytes.pop_back(); reject(bad,"material-truncated.msh");
        bad = materialData; bad.sections[materialIndex].bytes.push_back(0); reject(bad,"material-trailing.msh");
        bad = materialData;
        bad.sections[materialIndex].bytes.insert(bad.sections[materialIndex].bytes.end(), bytes.begin()+4, bytes.end());
        setU32(bad.sections[materialIndex].bytes,0,2); reject(bad,"material-duplicate-entry.msh");
        auto materialFirst = materialData;
        std::rotate(materialFirst.sections.begin(),materialFirst.sections.begin()+static_cast<std::ptrdiff_t>(materialIndex),materialFirst.sections.end());
        expect(writeFile(dir/"material-first.msh",materialFirst), "write materials before source frames");
        MESH_MBM_DEBUG first;
        expect(first.loadV11((dir/"material-first.msh").string().c_str()) && settingsAre(first,0,0,-1,2.5f), "material section order independent");
    }
    expect(material.setNormalMapSettings(0,0,1,0) && settingsAre(material,0,0,1,0), "zero strength valid");
    expect(material.setNormalMapSettings(0,0,1,1), "reset defaults");
    FILE_DATA reset;
    expect(save(material,dir/"material-reset.msh",false) && readFile(dir/"material-reset.msh",reset) &&
           materialSection(reset)==reset.sections.size(), "reset defaults removes section");
    MESH_MBM_DEBUG propsImported;
    expect(propsImported.loadV11((dir/"indexed-imported.msh").string().c_str()) && propsImported.setNormalMapSettings(0,0,-1,3),
           "set material on imported tangent basis");
    FILE_DATA propsImportedData;
    expect(save(propsImported,dir/"material-imported.msh",true) && readFile(dir/"material-imported.msh",propsImportedData), "save properties with imported basis");
    expect(tangentSection(propsImportedData)<propsImportedData.sections.size() &&
           propsImportedData.sections[tangentSection(propsImportedData)].bytes == indexedData.sections[indexedSection].bytes,
           "material settings never change imported tangents or their source signature");
    expect(save(propsImported,dir/"material-async.msh",true), "separate uncached async fixture");
    expect(propsImported.copyBufferFrom(propsImported,0)==2 && settingsAre(propsImported,1,0,-1,3), "frame clone copies settings");
    propsImported.removeBuffer(0);
    expect(settingsAre(propsImported,0,0,-1,3), "frame removal reindexes settings");
    expect(propsImported.copySubsetFrom(0,propsImported,0,0)==2 && settingsAre(propsImported,0,1,-1,3), "subset clone copies settings");
    expect(propsImported.setNormalMapSettings(0,0,1,0) && propsImported.moveSubsetUp(0,1) &&
           settingsAre(propsImported,0,0,-1,3) && settingsAre(propsImported,0,1,1,0), "subset reorder preserves each material");
    expect(!propsImported.mergeSubsets(0,{0,1}) && propsImported.getTotalSubsets(0)==2,
           "merging different normal-map settings fails without mutation");
    expect(propsImported.setNormalMapSettings(0,1,-1,3) && propsImported.mergeSubsets(0,{0,1}) &&
           settingsAre(propsImported,0,0,-1,3), "merge matching settings");
    expect(propsImported.copySubsetFrom(0,propsImported,0,0)==2 && propsImported.setNormalMapSettings(0,1,1,0), "prepare removal");
    propsImported.removeSubset(0,0);
    expect(settingsAre(propsImported,0,0,1,0), "subset removal preserves next material");
    expect(save(propsImported,dir/"material-edited.msh",true), "save after material reindexing");
    expect(propsImported.loadV11((dir/"no-map.msh").string().c_str()) && settingsAre(propsImported,0,0,1,1), "reload clears stale settings");
    MESH_MBM_DEBUG authored;
    addTriangle(authored,false);
    NORMAL_MAP_REPORT preparationReport;
    char preparationError[512] = {};
    const NORMAL_MAP_CORNER alternate[3] = {{0,1,0,1},{0,1,0,1},{0,1,0,1}};
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::IMPORT,preparationReport,preparationError,sizeof(preparationError),alternate,3),
           "explicitly import per-corner tangents without a normal texture");
    expect(!preparationReport.reused && preparationReport.batches==1 && preparationReport.vertices==3,
           "import report describes target subset");
    FILE_DATA authoredData;
    expect(save(authored,dir/"author-import.msh",false) && readFile(dir/"author-import.msh",authoredData), "persist explicitly prepared asset without map");
    expect(tangentSection(authoredData)<authoredData.sections.size() && authored.getSubset(0,0)->materialTextureSlots.empty(),
           "explicit preparation adds no normal texture");
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,preparationReport,preparationError,sizeof(preparationError)) && preparationReport.reused,
           "preserve reuses imported tangent basis");
    FILE_DATA preservedData;
    expect(save(authored,dir/"author-preserve.msh",true) && readFile(dir/"author-preserve.msh",preservedData) &&
           preservedData.sections[tangentSection(preservedData)].bytes==authoredData.sections[tangentSection(authoredData)].bytes,
           "preserve is byte-stable across compressed save");
    NORMAL_MAP_REPORT unchangedReport; unchangedReport.vertices=99;
    const NORMAL_MAP_CORNER badCorners[3] = {{1,0,0,0},{1,0,0,1},{1,0,0,1}};
    expect(!authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::IMPORT,unchangedReport,preparationError,sizeof(preparationError),badCorners,3) &&
           unchangedReport.vertices==99 && preparationError[0], "invalid import is transactional and diagnostic");
    expect(!authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::IMPORT,unchangedReport,preparationError,sizeof(preparationError),alternate,2), "per-vertex or wrong corner count rejected");
    expect(!authored.prepareNormalMap(1,0,NORMAL_MAP_POLICY::GENERATE,unchangedReport,preparationError,sizeof(preparationError)), "invalid frame rejected");
    expect(!authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::GENERATE,unchangedReport,preparationError,sizeof(preparationError),alternate,3), "generate cannot silently ignore imported input");
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,preparationReport,preparationError,sizeof(preparationError)) && preparationReport.reused,
           "failed imports keep old preparation");
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::GENERATE,preparationReport,preparationError,sizeof(preparationError)) && !preparationReport.reused,
           "explicit recalculation replaces imported basis");
    FILE_DATA generatedData;
    expect(save(authored,dir/"author-generate.msh",false) && readFile(dir/"author-generate.msh",generatedData) &&
           generatedData.sections[tangentSection(generatedData)].bytes!=authoredData.sections[tangentSection(authoredData)].bytes,
           "recalculated MikkTSpace basis differs from alternate import");
    authored.getFrameBuffer(0)->position[3]=2;
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,preparationReport,preparationError,sizeof(preparationError)) && !preparationReport.reused,
           "stale geometry regenerates on explicit preserve");
    expect(authored.copySubsetFrom(0,authored,0,0)==2, "create two authoring subsets");
    expect(authored.prepareNormalMap(0,0,NORMAL_MAP_POLICY::IMPORT,preparationReport,preparationError,sizeof(preparationError),alternate,3), "import first subset");
    expect(authored.prepareNormalMap(0,1,NORMAL_MAP_POLICY::GENERATE,preparationReport,preparationError,sizeof(preparationError)), "prepare second subset without replacing first");
    FILE_DATA twoPrepared;
    expect(save(authored,dir/"author-subsets.msh",false) && readFile(dir/"author-subsets.msh",twoPrepared), "save per-subset preparation");
    const auto &twoBytes=twoPrepared.sections[tangentSection(twoPrepared)].bytes;
    util::MEM_CURSOR_V11 twoCursor{twoBytes.data(),twoBytes.size(),0};
    normal_map::ASSET_FRAME twoBases;
    expect(normal_map::readPayload(twoCursor,1,frameIndex,twoBases,error) && twoBases.prepared.batches.size()==2 &&
           twoBases.prepared.batches[0].tangents[0].y==1 && twoBases.prepared.batches[1].tangents[0].x==1,
           "adding a requested subset preserves existing imported basis");
    MESH_MBM_DEBUG noUv;
    addTriangle(noUv,false); noUv.setHasTexture(HAS_TEX_NO);
    expect(!noUv.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,preparationReport,preparationError,sizeof(preparationError)), "explicit request diagnoses missing UVs");
    noUv.setHasTexture(HAS_TEX_EACH_FRAME); noUv.setModeDraw(util::MODE_DRAW_POINTS);
    expect(!noUv.prepareNormalMap(0,0,NORMAL_MAP_POLICY::PRESERVE,preparationReport,preparationError,sizeof(preparationError)), "explicit request diagnoses unsupported topology");
    expect(shared.prepareNormalMap(1,0,NORMAL_MAP_POLICY::GENERATE,preparationReport,preparationError,sizeof(preparationError)) &&
           save(shared,dir/"author-shared-uv.msh",true), "explicit preparation resolves shared UVs like save");
    if (!keep) std::filesystem::remove_all(dir);
    std::printf("[normal-map-persistence] %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
