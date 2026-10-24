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

#include "facial-animation-tests.h"
#include <private/facial-animation.h>
#include <private/facial-render-capability.h>
#include <private/skeletal-animation-foundation.h>
#include <mesh-v11-io.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/draw-compatibility.h>
#include <core_mbm/device.h>
#include <render/mesh.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>

namespace
{
    using namespace mbm;
    int failures=0;
    void expect(bool result, const char *what)
    {
        if (!result) { ++failures; std::fprintf(stderr,"FACIAL FAIL: %s\n",what); }
    }
    const auto directory=std::filesystem::temp_directory_path()/"mini-mbm-facial-tests";
    struct SECTION { util::SECTION_HEADER_V11 header; std::vector<uint8_t> bytes; };
    struct FILE_DATA { util::FILE_HEADER_V11 header; std::vector<SECTION> sections; };
    bool read(const std::filesystem::path &path, FILE_DATA &data)
    {
        FILE *f=std::fopen(path.string().c_str(),"rb");
        if (!f) return false;
        bool ok=util::readFileHeaderV11(f,data.header);
        data.sections.clear();
        for (uint32_t i=0;ok && i<data.header.sectionCount;++i)
        {
            SECTION section;
            ok=util::readSectionV11(f,section.header,section.bytes);
            if (ok) data.sections.push_back(std::move(section));
        }
        std::fclose(f); return ok;
    }
    bool write(const std::filesystem::path &path, FILE_DATA data)
    {
        FILE *f=std::fopen(path.string().c_str(),"wb");
        if (!f) return false;
        data.header.sectionCount=static_cast<uint32_t>(data.sections.size());
        bool ok=util::writeFileHeaderV11(f,data.header);
        for (auto &section : data.sections)
            ok=ok && util::writeSectionV11(f,section.header,section.bytes.data(),static_cast<uint32_t>(section.bytes.size()));
        std::fclose(f); return ok;
    }
    bool section(const facial::ASSET &asset, uint16_t type, SECTION &out)
    {
        FILE *f=std::tmpfile();
        if (!f) return false;
        bool ok=type==util::SECTION_FACIAL_TARGETS ? facial::writeTargets(f,asset) : facial::writeClips(f,asset);
        const long length=std::ftell(f);
        if (length<0) ok=false;
        if (ok)
        {
            out.bytes.resize(static_cast<size_t>(length));
            std::rewind(f);
            ok=std::fread(out.bytes.data(),1,out.bytes.size(),f)==out.bytes.size();
        }
        std::fclose(f);
        out.header.type=type; out.header.sectionVersion=1;
        return ok;
    }
    bool save(MESH_MBM_DEBUG &mesh,const char *file,bool compressed=false)
    {
        char error[512]={};
        const bool ok=mesh.saveV11((directory/file).string().c_str(),false,false,compressed,error,sizeof(error));
        if (!ok) std::fprintf(stderr,"facial save: %s\n",error);
        return ok;
    }
    facial::ASSET assetFor(MESH_MBM_DEBUG &mesh)
    {
        auto *frame=mesh.getFrameBuffer(0);
        facial::ASSET a; a.meshId=123; a.vertexCount=3;
        a.geometrySignature=facial::geometrySignature(reinterpret_cast<VEC3 *>(frame->position),
            reinterpret_cast<VEC3 *>(frame->normal),3,frame->indexBuffer,frame->headerFrame.sizeIndexBuffer);
        for (uint32_t i=0;i<2;++i)
        {
            facial::TARGET t; t.id=100+i; t.name=i?"smile":"jawOpen";
            t.convention="test"; t.positions.resize(3); t.normals.resize(3);
            t.positions[2]=i?VEC3(80,0,0):VEC3(0,-60,0);
            a.targets.push_back(t);
        }
        facial::CLIP c; c.id=90; c.name="expression"; c.duration=1;
        c.keys={{0,{0,0}},{1,{1,0.5f}}}; a.clips.push_back(c);
        return a;
    }
    bool makeFixture(bool indexed, bool skeletal, const char *file)
    {
        std::filesystem::create_directories(directory);
        MESH_MBM_DEBUG mesh;
        mesh.addBuffer(); mesh.addSubset(0);
        auto *frame=mesh.getFrameBuffer(0);
        frame->position=reinterpret_cast<float *>(new VEC3[3]{VEC3(-50,-50,0),VEC3(0,50,0),VEC3(50,-50,0)});
        frame->normal=reinterpret_cast<float *>(new VEC3[3]{VEC3(0,0,-1),VEC3(0,0,-1),VEC3(0,0,-1)});
        frame->uv=reinterpret_cast<float *>(new VEC2[3]{VEC2(0,0),VEC2(.5f,1),VEC2(1,0)});
        frame->headerFrame.sizeVertexBuffer=3; frame->headerFrame.totalSubset=1;
        auto *subset=mesh.getSubset(0,0); subset->vertexCount=3; subset->texture="#FFFFFFFF";
        if (indexed)
        {
            frame->indexBuffer=new uint16_t[3]{0,1,2}; frame->headerFrame.sizeIndexBuffer=3;
            std::strcpy(frame->headerFrame.typeBuffer,"IB"); subset->indexCount=3;
        }
        mesh.setHasNormal(HAS_NOR_IN_FILE); mesh.setHasTexture(HAS_TEX_EACH_FRAME);
        mesh.setMeshType(util::TYPE_MESH_3D); mesh.setModeDraw(util::MODE_DRAW_TRIANGLES);
        char error[512]={};
        if (skeletal)
        {
            uint32_t index=0;
            if (!mesh.initializeSkeletalSkeleton("head",VEC3(),.1f,1,true,error,sizeof(error)) ||
                !mesh.initializeSkeletalVertexWeights(0,&index,error,sizeof(error)) ||
                !mesh.addSkeletalClip("pose",1,false,&index,error,sizeof(error)) ||
                !mesh.addSkeletalTrack(0,0,skeletal::SKELETAL_CHANNEL_TRANSLATION,&index,error,sizeof(error)) ||
                !mesh.addSkeletalKey(0,0,1,&index,error,sizeof(error)) ||
                !mesh.updateSkeletalKey(0,0,index,1,VEC3(100,0,0),0,0,0,1,VEC3(1,1,1),0,0,0,1,1,error,sizeof(error)))
            { std::fprintf(stderr,"fixture skeleton: %s\n",error); return false; }
        }
        const auto a=assetFor(mesh);
        if (!save(mesh,"base.msh")) return false;
        FILE_DATA data;
        if (!read(directory/"base.msh",data)) return false;
        SECTION targets,clips;
        if (!section(a,util::SECTION_FACIAL_TARGETS,targets) || !section(a,util::SECTION_FACIAL_CLIPS,clips)) return false;
        // Clips before targets and geometry test order-independent resolution.
        data.sections.insert(data.sections.begin(),targets);
        data.sections.insert(data.sections.begin(),clips);
        return write(directory/file,data);
    }
    void put32(std::vector<uint8_t> &bytes,size_t offset,uint32_t v)
    { for (unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<uint8_t>(v>>(8*i)); }
    void foundations()
    {
        const auto morphCapability = facial::calculateGpuMorphCapability(8, 4096, true);
        expect(morphCapability.measured && morphCapability.supportsIndexedDeltas &&
            morphCapability.maxVertexTextureUnits==8 && morphCapability.maxTextureSize==4096,
            "GPU morph capability prerequisites are measured");
        const auto unavailableMorph = facial::calculateGpuMorphCapability(0, 4096, true);
        expect(!unavailableMorph.measured && !unavailableMorph.supportsIndexedDeltas,
            "GPU morph capability rejects missing vertex texture units");
        const auto insufficientUnits = facial::calculateGpuMorphCapability(6, 4096, true);
        expect(insufficientUnits.measured && !insufficientUnits.supportsIndexedDeltas,
            "GPU morph capability reserves texture unit 6 for indexed deltas");
        facial::ASSET layoutAsset;
        layoutAsset.vertexCount = 34164;
        layoutAsset.targets.resize(6);
        facial::GPU_MORPH_LAYOUT layout;
        std::string layoutError;
        expect(facial::buildGpuMorphLayout(layoutAsset, morphCapability, true, layout, layoutError) &&
            layout.valid && layout.textureWidth==4096 && layout.rowsPerVertexBlock==9 &&
            layout.rowsPerTarget==18 && layout.textureHeight==108,
            "GPU morph layout tiles Megan-scale vertices and normals");
        expect(makeFixture(true,true,"indexed.msh"),"create indexed skinned facial fixture");
        expect(makeFixture(false,false,"vertices.msh"),"create VB facial fixture");
        FILE_DATA original;
        if (!read(directory/"indexed.msh",original)) { expect(false,"read fixture"); return; }
        const auto targetSection=std::find_if(original.sections.begin(),original.sections.end(),
            [](const SECTION &section){ return section.header.type==util::SECTION_FACIAL_TARGETS; });
        facial::ASSET decoded;
        std::string decodeError;
        if (targetSection!=original.sections.end())
        {
            util::MEM_CURSOR_V11 targetCursor{targetSection->bytes.data(),targetSection->bytes.size(),0};
            expect(facial::readTargets(targetCursor,1,decoded,decodeError) && decoded.targets.size()==2 &&
                decoded.targets[0].sparseReady && decoded.targets[0].activeVertices.size()==1 &&
                decoded.targets[0].activeVertices[0]==2,
                "decoded facial targets build sparse vertex index");
        }
        else expect(false,"facial target section is present for sparse decode");
        MESH_MBM_DEBUG mesh;
        expect(mesh.loadV11((directory/"indexed.msh").string().c_str()),"load facial sections before geometry");
        expect(mesh.getFacialMeshId()==123 && mesh.getFacialVertexCount()==3 &&
            mesh.getTotalFacialTargets()==2 && mesh.getTotalFacialClips()==1,
            "debug facial export metadata");
        SKELETON_BIND_SUMMARY skeletonSummary;
        SKELETAL_CLIP_INFO skeletalClip;
        expect(mesh.getSkeletonBindSummary(skeletonSummary) && skeletonSummary.valid &&
            skeletonSummary.canonical && skeletonSummary.boneCount==1,
            "debug skeletal bind metadata survives facial load");
        expect(mesh.getTotalSkeletalClips()==1 &&
            std::strcmp(mesh.getSkeletalClipName(0),"pose")==0 &&
            mesh.getSkeletalClip(0,skeletalClip) && skeletalClip.duration==1.0f &&
            skeletalClip.trackCount==1,
            "debug skeletal clip metadata survives facial load");
        expect(std::strcmp(mesh.getFacialTargetName(0),"jawOpen")==0 &&
            std::strcmp(mesh.getFacialTargetConvention(0),"test")==0 &&
            mesh.getFacialTargetId(0)==100,
            "debug facial target identity");
        VEC3 targetDelta, targetNormal;
        expect(mesh.getFacialTargetDelta(0,2,&targetDelta,&targetNormal) &&
            targetDelta.y==-60.0f && targetNormal.z==0.0f,
            "debug facial target deltas");
        MESH_MBM_DEBUG scaledMesh;
        expect(scaledMesh.loadV11((directory/"indexed.msh").string().c_str()),
            "load facial asset for uniform scaling");
        char scaleError[512]={};
        expect(scaledMesh.scaleSkeletalAsset(2.0f,scaleError,sizeof(scaleError)),
            "scale skeletal and facial asset together");
        auto *scaledFrame=scaledMesh.getFrameBuffer(0);
        expect(scaledFrame && reinterpret_cast<VEC3 *>(scaledFrame->position)[0].x==-100.0f,
            "uniform scaling updates facial base geometry");
        expect(scaledMesh.getFacialTargetDelta(0,2,&targetDelta,&targetNormal) &&
            targetDelta.y==-120.0f && targetNormal.z==0.0f,
            "uniform scaling updates facial position deltas only");
        expect(scaledMesh.getFacialGeometrySignature()==facial::geometrySignature(
            reinterpret_cast<VEC3 *>(scaledFrame->position),reinterpret_cast<VEC3 *>(scaledFrame->normal),
            3,scaledFrame->indexBuffer,scaledFrame->headerFrame.sizeIndexBuffer),
            "uniform scaling refreshes facial geometry signature");
        SKELETAL_KEY_INFO scaledKey;
        expect(scaledMesh.getSkeletalKey(0,0,1,scaledKey) &&
            scaledKey.localTranslation.x==200.0f,
            "uniform scaling updates skeletal translation keys");
        expect(save(scaledMesh,"scaled.msh"),"save uniformly scaled facial asset");
        MESH_MBM_DEBUG scaledReload;
        expect(scaledReload.loadV11((directory/"scaled.msh").string().c_str()) &&
            scaledReload.getFacialGeometrySignature()==scaledMesh.getFacialGeometrySignature(),
            "reload uniformly scaled facial asset");
        MESH_MBM_DEBUG facialOnly;
        expect(facialOnly.loadV11((directory/"vertices.msh").string().c_str()),
            "load facial-only asset for uniform scaling");
        expect(facialOnly.scaleSkeletalAsset(2.0f,scaleError,sizeof(scaleError)),
            "scale facial-only asset without a skeleton");
        float clipTime=0.0f, clipWeights[2]={};
        expect(mesh.getFacialClipId(0)==90 && mesh.getFacialClipInfo(0,nullptr,nullptr,nullptr,nullptr) &&
            mesh.getFacialClipKey(0,1,&clipTime,clipWeights,2) && clipTime==1.0f &&
            clipWeights[0]==1.0f && clipWeights[1]==0.5f,
            "debug facial clip keys");
        for (bool compressed : {false,true})
        {
            expect(save(mesh,"roundtrip.msh",compressed),"roundtrip save");
            FILE_DATA out; expect(read(directory/"roundtrip.msh",out),"roundtrip read");
            for (size_t i=0;i<2;++i)
            {
                auto found=std::find_if(out.sections.begin(),out.sections.end(),[&](const SECTION &s){return s.header.type==original.sections[i].header.type;});
                expect(found!=out.sections.end() && found->bytes==original.sections[i].bytes,"facial payload preserved byte-for-byte");
            }
            constexpr uint16_t skeletalSections[] = {
                util::SECTION_SKELETAL_SKELETON,
                util::SECTION_SKELETAL_WEIGHTS,
                util::SECTION_SKELETAL_ANIMATION,
            };
            for (const uint16_t type : skeletalSections)
            {
                const auto originalSection=std::find_if(original.sections.begin(),original.sections.end(),
                    [type](const SECTION &s){ return s.header.type==type; });
                const auto roundtripSection=std::find_if(out.sections.begin(),out.sections.end(),
                    [type](const SECTION &s){ return s.header.type==type; });
                expect(originalSection!=original.sections.end() && roundtripSection!=out.sections.end() &&
                    roundtripSection->bytes==originalSection->bytes,
                    "skeletal payload preserved alongside facial payload");
            }
        }
        const auto reject=[&](FILE_DATA bad,const char *what) {
            expect(write(directory/"bad.msh",bad),"write invalid fixture with valid CRC");
            expect(!mesh.loadV11((directory/"bad.msh").string().c_str()),what);
            expect(mesh.getTotalFrames()==1 && mesh.getTotalSkeletalClips()==1,"failed load retains original mesh");
        };
        auto bad=original; bad.sections[1].header.sectionVersion=2; reject(bad,"unsupported version");
        bad=original; bad.sections.push_back(bad.sections[1]); reject(bad,"duplicate targets");
        bad=original; bad.sections.erase(bad.sections.begin()+1); reject(bad,"orphan clips");
        bad=original; bad.sections[0].bytes[0]^=1; reject(bad,"mismatched clip mesh ID");
        bad=original; bad.sections[1].bytes[8]^=1; reject(bad,"geometry signature mismatch");
        bad=original; put32(bad.sections[1].bytes,16,4); reject(bad,"vertex count mismatch");
        bad=original; put32(bad.sections[1].bytes,20,0xffffffff); reject(bad,"target count budget");
        bad=original; bad.sections[1].bytes.pop_back(); reject(bad,"truncated target payload");
        bad=original; bad.sections[1].bytes.push_back(0); reject(bad,"trailing target bytes");
        bad=original; put32(bad.sections[0].bytes,bad.sections[0].bytes.size()-4,0x7fc00000); reject(bad,"NaN weight");
        expect(!mesh.mergeSubsets(0,{0}) && mesh.getTotalFrames()==1,"unsupported topology edit rejected");
        const float before=mesh.getFrameBuffer(0)->position[0];
        mesh.translateFrame(0,-1,1,0,0);
        expect(mesh.getFrameBuffer(0)->position[0]==before,"unsupported transform retains source");
        mesh.getFrameBuffer(0)->position[0]+=1;
        expect(!save(mesh,"invalid-save.msh"),"legacy raw-pointer edit detected on save");
        mesh.getFrameBuffer(0)->position[0]=before;
        auto a=assetFor(mesh); std::string error;
        auto invalid=a; invalid.targets[1].id=invalid.targets[0].id;
        expect(!facial::validate(invalid,error),"duplicate IDs rejected");
        invalid=a; invalid.targets[1].name=invalid.targets[0].name;
        expect(!facial::validate(invalid,error),"duplicate names rejected");
        invalid=a; invalid.targets[0].positions[0].x=std::numeric_limits<float>::infinity();
        expect(!facial::validate(invalid,error),"non-finite delta rejected");
        invalid=a; invalid.clips[0].keys[1].time=0;
        expect(!facial::validate(invalid,error),"unordered keys rejected");
        invalid=a; invalid.clips[0].duration=-1;
        expect(!facial::validate(invalid,error),"invalid duration rejected");
        invalid=a; invalid.targets[0].normals.clear();
        expect(facial::validate(invalid,error),"optional normal data is valid for storage");
        std::vector<VEC3> base={{-50,-50,0},{0,50,0},{50,-50,0}}, normals(3,VEC3(0,0,-1)),p,n;
        auto denseDecoded=decoded;
        for (facial::TARGET &target : denseDecoded.targets)
        {
            target.activeVertices.clear();
            target.sparseReady=false;
        }
        std::vector<VEC3> sparsePositions, sparseNormals, densePositions, denseNormals;
        expect(facial::deform(decoded,base,normals,{1,0},sparsePositions,sparseNormals,error) &&
            facial::deform(denseDecoded,base,normals,{1,0},densePositions,denseNormals,error) &&
            sparsePositions.size()==densePositions.size() && sparseNormals.size()==denseNormals.size() &&
            std::memcmp(sparsePositions.data(),densePositions.data(),sparsePositions.size()*sizeof(VEC3))==0 &&
            std::memcmp(sparseNormals.data(),denseNormals.data(),sparseNormals.size()*sizeof(VEC3))==0,
            "sparse facial deformation matches dense output");
        expect(facial::deform(a,base,normals,{0,0},p,n,error) && std::memcmp(p.data(),base.data(),sizeof(VEC3)*3)==0,"zero weights reproduce base");
        expect(facial::deform(a,base,normals,{.5f,.25f},p,n,error) && p[2].x==70 && p[2].y==-80,"simultaneous targets add from immutable base");
        expect(!facial::deform(invalid,base,normals,{0,0},p,n,error),"lit profile rejects missing normals even at zero");
        invalid=a; invalid.targets[0].normals[0]=VEC3(0,0,1);
        expect(!facial::deform(invalid,base,normals,{1,0},p,n,error),"degenerate normal rejected");
        std::vector<float> sampled;
        expect(facial::sampleClip(a,0,.5f,sampled,error) && sampled.size()==2 &&
            std::fabs(sampled[0]-.5f)<1e-6f && std::fabs(sampled[1]-.25f)<1e-6f,
            "linear facial clip sampling");
        expect(facial::sampleClip(a,0,4.0f,sampled,error) && sampled[0]==1.0f && sampled[1]==.5f,
            "facial clip sampling clamps to duration");
        // Representative allocation/workload size, synthetic and reproducible; no GPU capacity claim.
        facial::ASSET benchmark; benchmark.meshId=1; benchmark.geometrySignature=1; benchmark.vertexCount=20000;
        benchmark.targets.resize(52);
        for (uint32_t i=0;i<52;++i)
        {
            auto &t=benchmark.targets[i]; t.id=i+1; t.name=std::to_string(i);
            t.positions.assign(20000,VEC3(.001f,0,0)); t.normals.resize(20000);
        }
        base.assign(20000,VEC3()); normals.assign(20000,VEC3(0,0,1));
        std::vector<float> weights(52,0); std::fill_n(weights.begin(),8,.5f);
        const auto start=std::chrono::steady_clock::now();
        for (unsigned i=0;i<20;++i) expect(facial::deform(benchmark,base,normals,weights,p,n,error),"reference benchmark");
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
        std::printf("FACIAL BENCHMARK vertices=20000 targets=52 active=8 deltaBytes=24960000 ms=%.3f (build-dependent CPU reference)\n",ms);
    }
    std::unique_ptr<MESH> first,second,asyncMesh;
    bool asyncDone=false, asyncOk=false;
    unsigned tick=0;
    FACIAL_ANIMATION_REPORT baseline,changed;
}

int runFacialFoundationTests()
{
    failures=0; foundations();
    std::printf("FACIAL FOUNDATION %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}

int runFacialPerformanceBenchmark()
{
    struct PROFILE
    {
        const char *name;
        uint32_t vertices;
        uint32_t targets;
        uint32_t active;
    };
    constexpr PROFILE profiles[] = {
        {"small", 5000, 16, 4},
        {"medium", 20000, 52, 8},
        {"large", 40000, 52, 8},
    };
    bool passed = true;
    for (const PROFILE &profile : profiles)
    {
        facial::ASSET sparseAsset;
        sparseAsset.meshId = 1;
        sparseAsset.geometrySignature = 1;
        sparseAsset.vertexCount = profile.vertices;
        sparseAsset.targets.resize(profile.targets);
        for (uint32_t targetIndex = 0; targetIndex < profile.targets; ++targetIndex)
        {
            facial::TARGET &target = sparseAsset.targets[targetIndex];
            target.id = targetIndex + 1;
            target.name = std::to_string(targetIndex);
            target.positions.assign(profile.vertices, VEC3());
            target.normals.assign(profile.vertices, VEC3());
            for (uint32_t vertex = targetIndex % 8; vertex < profile.vertices; vertex += 8)
            {
                target.positions[vertex] = VEC3(0.001f, 0.0f, 0.0f);
                target.activeVertices.push_back(vertex);
            }
            target.sparseReady = true;
        }
        std::vector<VEC3> base(profile.vertices, VEC3());
        std::vector<VEC3> normals(profile.vertices, VEC3(0.0f, 0.0f, 1.0f));
        std::vector<float> weights(profile.targets, 0.0f);
        std::fill_n(weights.begin(), profile.active, 0.5f);
        std::vector<VEC3> positions, outputNormals;
        std::string error;
        facial::ASSET denseAsset = sparseAsset;
        for (facial::TARGET &target : denseAsset.targets)
        {
            target.activeVertices.clear();
            target.sparseReady = false;
        }
        const auto measure = [&](const facial::ASSET &asset, double &median, double &p95) {
            for (unsigned warmup = 0; warmup < 3; ++warmup)
            {
                if (!facial::deform(asset, base, normals, weights, positions, outputNormals, error)) return false;
            }
            std::vector<double> samples;
            samples.reserve(12);
            for (unsigned sample = 0; sample < 12; ++sample)
            {
                const auto start = std::chrono::steady_clock::now();
                if (!facial::deform(asset, base, normals, weights, positions, outputNormals, error)) return false;
                samples.push_back(std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count());
            }
            std::sort(samples.begin(), samples.end());
            median = samples[samples.size() / 2];
            p95 = samples[std::min(samples.size() - 1, (samples.size() * 95) / 100)];
            return true;
        };
        double denseMedian = 0, denseP95 = 0, sparseMedian = 0, sparseP95 = 0;
        if (!measure(denseAsset, denseMedian, denseP95) || !measure(sparseAsset, sparseMedian, sparseP95))
        {
            std::fprintf(stderr, "FACIAL PERF FAIL profile=%s error=%s\n", profile.name, error.c_str());
            return 1;
        }
        const uint64_t deltaBytes = static_cast<uint64_t>(profile.vertices) * profile.targets *
            static_cast<uint64_t>(sizeof(VEC3)) * 2u;
        std::printf("FACIAL PERF profile=%s vertices=%u targets=%u active=%u denseMedianMs=%.3f "
                    "sparseMedianMs=%.3f denseP95Ms=%.3f sparseP95Ms=%.3f deltaBytes=%llu reference=cpu\n",
                    profile.name, profile.vertices, profile.targets, profile.active, denseMedian,
                    sparseMedian, denseP95, sparseP95,
                    static_cast<unsigned long long>(deltaBytes));
        passed = passed && !positions.empty() && positions.size() == profile.vertices;
    }
    std::printf("FACIAL PERFORMANCE %s (reference CPU; no hard hardware threshold)\n",
                passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}

bool beginFacialRuntimeTests(const mbm::SCENE *scene)
{
    failures=0; tick=0; asyncDone=false; asyncOk=false;
    if (!makeFixture(true,true,"runtime.msh") || !makeFixture(false,false,"async.msh")) return false;
    first=std::make_unique<mbm::MESH>(scene,true,false);
    second=std::make_unique<mbm::MESH>(scene,true,false);
    asyncMesh=std::make_unique<mbm::MESH>(scene,true,false);
    const auto path=(directory/"runtime.msh").string();
    if (!first->load(path.c_str()) || !second->load(path.c_str())) return false;
    first->setPosition(VEC3(-120,0,0)); second->setPosition(VEC3(120,0,0));
    expect(first->getTotalFacialTargets()==2 && first->getTotalFacialClips()==1,
        "facial target and clip enumeration");
    expect(first->playFacialAnimation("expression"),"start facial clip");
    expect(first->playSkeletalAnimation("pose") && first->pauseSkeletalAnimation(),"start and pause skin pose");
    expect(!first->setFacialWeight(99,1) && !first->setFacialWeight(0,std::numeric_limits<float>::quiet_NaN()),"invalid runtime weight rejected");
    asyncMesh->loadAsync((directory/"async.msh").string().c_str(),[](bool ok){asyncDone=true;asyncOk=ok;});
    auto &camera=DEVICE::getInstance()->getCamera();
    camera.position=VEC3(0,0,-700); camera.focus=VEC3(0,0,0);
    return true;
}

int stepFacialRuntimeTests()
{
    ++tick;
    FACIAL_ANIMATION_REPORT a,b;
    first->getFacialAnimationReport(a); second->getFacialAnimationReport(b);
    if (tick==10)
    {
        expect(a.ready && b.ready && a.targetCount==2 && a.clipCount==1 && a.uploadCount>0 &&
            a.playing && a.clipName && std::strcmp(a.clipName,"expression")==0,
            "runtime drew both instances and reports facial clip");
        baseline=a;
    }
    if (tick==20)
    {
        expect(a.deformationCount==baseline.deformationCount && a.uploadCount==baseline.uploadCount,"idle pose does not deform/upload");
        expect(first->setFacialWeightByName("jawOpen",.5f) && first->setFacialWeight(1,.25f),"set named and indexed weights");
    }
    if (tick==30)
    {
        expect(a.ready && a.deformationCount==baseline.deformationCount+1 && a.uploadCount==baseline.uploadCount+1,"weight change evaluates/uploads once");
        float weight=-1; expect(second->getFacialWeight(0,&weight) && weight==0,"shared asset does not leak weights");
        changed=a;
        expect(first->setFacialWeight(0,.5f),"assign same weight");
    }
    if (tick==40)
    {
        expect(a.deformationCount==changed.deformationCount && a.uploadCount==changed.uploadCount,"same weight stays idle");
        expect(first->clearFacialWeightByName("jawOpen") && first->seekFacialAnimation(.5f) &&
            first->seekSkeletalAnimation(.5f),"seek facial and skin poses with morphs active");
    }
    if (tick==50)
    {
        expect(a.ready && a.deformationCount==changed.deformationCount+1,"pose change recomposes after morphing");
        changed=a;
        first->setPosition(VEC3(100000,0,0));
        first->setFacialWeight(0,.8f);
    }
    if (tick==60)
    {
        expect(a.deformationCount==changed.deformationCount+1,"culled mesh still prepares current deformed bounds");
        first->setPosition(VEC3(-120,0,0));
    }
    if (tick==70)
    {
        first->release();
        expect(first->load((directory/"runtime.msh").string().c_str()),"reload instance");
        float weight=-1; expect(first->getFacialWeight(0,&weight) && weight==0,"reload restores default weights");
        MESH_MBM_DEBUG copy;
        auto *asset=MESH_MANAGER::getInstance()->load((directory/"runtime.msh").string().c_str());
        expect(copy.loadDebugFromMemory(asset) && save(copy,"runtime-extracted.msh",true),"runtime extraction preserves facial geometry and skeleton");
    }
    if (tick>=90 && asyncDone)
    {
        FACIAL_ANIMATION_REPORT asyncReport; asyncMesh->getFacialAnimationReport(asyncReport);
        expect(asyncOk && asyncReport.ready && asyncReport.uploadCount>0,"async VB-only facial load renders");
        std::printf("FACIAL RUNTIME %s status=%s targets=%u clips=%u evaluations=%llu uploads=%llu\n",
            failures?"FAIL":"PASS",a.status,a.targetCount,a.clipCount,
            static_cast<unsigned long long>(a.deformationCount),static_cast<unsigned long long>(a.uploadCount));
        return failures?-1:1;
    }
    if (tick>1000) { expect(false,"async runtime timeout"); return -1; }
    return 0;
}

void endFacialRuntimeTests()
{
    asyncMesh.reset(); second.reset(); first.reset();
}
