/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef MESH_MANAGER_GLES_H
#define MESH_MANAGER_GLES_H

#include "core-exports.h"
#include "primitives.h"
#include "header-mesh.h"
#include "physics.h"
#include "texture-role.h"
#include "animation.h"
#include <map>
#include <memory>
#include <functional>

namespace util
{
    struct SUBSET;
}


namespace mbm
{
    class MESH_MBM_DEBUG;
    namespace skeletal
    {
        struct CANONICAL_PARITY_ASSET;
        API_IMPL bool copyCanonicalParityAsset(const MESH_MBM_DEBUG &mesh,
                                               CANONICAL_PARITY_ASSET &out) noexcept;
    }
    class BUFFER_GL;
    class RENDERIZABLE;
    class RENDERIZABLE_TO_TARGET;
    class SHADER;
    class MESH_MBM;
    class ARTICULATED_ANIMATION_PLAYER;
    class SKELETAL_ANIMATION_PLAYER;
    struct IMAGE_RESOURCE;
    // Defined in mesh-manager.cpp only - forward-declared here so MESH_MBM::finishLoadFromIntermediate
    // can be declared without exposing the type's layout in the public header, same PIMPL-style
    // pattern as MESH_MBM::Impl.
    struct MESH_LOAD_INTERMEDIATE_V11;

    struct SKELETAL_VERTEX_WEIGHT_EDIT
    {
        uint32_t vertexIndex;
        const char *boneNames[4];
        float weights[4];
    };

    struct MESH_SIMPLIFY_REPORT
    {
        uint32_t sourceVertexCount = 0;
        uint32_t resultVertexCount = 0;
        uint32_t sourceTriangleCount = 0;
        uint32_t resultTriangleCount = 0;
        float maximumGeometricError = 0.0f;
        bool skinWeightAware = false;
        bool poseSampledError = false;
        uint32_t sampledPoseCount = 0;
        uint32_t sampledClipCount = 0;
        float maximumPoseError = 0.0f;
        bool geometryFrameAware = false;
        uint32_t geometryFrameCount = 0;
        float maximumFrameError = 0.0f;
        float maximumRelativeError = 0.0f;
        uint32_t collapseCount = 0;
        uint32_t boundaryRejectedCollapseCount = 0;
        uint32_t topologyRejectedCollapseCount = 0;
        uint32_t orientationRejectedCollapseCount = 0;
        uint32_t invalidRejectedCollapseCount = 0;
        uint32_t degenerateTriangleCount = 0;
        uint32_t nonManifoldEdgeCount = 0;
        uint32_t connectedComponentCount = 0;
        uint32_t detailPenalizedCandidateCount = 0;
        uint32_t detailPenalizedCollapseCount = 0;
    };

    struct BUFFER_MESH
    {
        BUFFER_GL *pBufferGL;
        util::SUBSET *  subset;
        uint32_t    totalSubset;
        API_IMPL BUFFER_MESH() noexcept;
        API_IMPL virtual ~BUFFER_MESH();
        API_IMPL void release();
        API_IMPL BUFFER_GL *getRenderBuffer() const noexcept;
        API_IMPL bool hasLoadedRenderBuffer() const noexcept;
        API_IMPL uint32_t getTotalSubsets() const noexcept;
        API_IMPL util::SUBSET *getSubset(const uint32_t indexSubset) const noexcept;
    };

    class ARTICULATED_ANIMATION_PLAYER
    {
        friend class MESH_MBM;
      public:
        API_IMPL ARTICULATED_ANIMATION_PLAYER();
        API_IMPL ~ARTICULATED_ANIMATION_PLAYER();
        API_IMPL void reset() noexcept;
        ARTICULATED_ANIMATION_PLAYER(const ARTICULATED_ANIMATION_PLAYER &) = delete;
        ARTICULATED_ANIMATION_PLAYER &operator=(const ARTICULATED_ANIMATION_PLAYER &) = delete;
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    class SKELETAL_ANIMATION_PLAYER
    {
        friend class MESH_MBM;
      public:
        API_IMPL SKELETAL_ANIMATION_PLAYER();
        API_IMPL ~SKELETAL_ANIMATION_PLAYER();
        API_IMPL void reset() noexcept;
        API_IMPL void setSkinningMethod(SKELETAL_SHADER_METHOD method) noexcept;
        API_IMPL SKELETAL_SHADER_METHOD getSkinningMethod() const noexcept;
        API_IMPL SKELETAL_SHADER_METHOD getResolvedSkinningMethod() const noexcept;
        API_IMPL const char *getSkinningResolutionReason() const noexcept;
        SKELETAL_ANIMATION_PLAYER(const SKELETAL_ANIMATION_PLAYER &) = delete;
        SKELETAL_ANIMATION_PLAYER &operator=(const SKELETAL_ANIMATION_PLAYER &) = delete;
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    // Read-only, copy-out view of the canonical bind-pose compiler. The compiled vectors and
    // lookup tables remain private to MESH_MBM_DEBUG::Impl; these records expose only values that
    // editor diagnostics need and are never consulted by rendering.
    struct SKELETON_BIND_SUMMARY
    {
        uint32_t boneCount = 0;
        uint32_t diagnosticCount = 0;
        uint32_t animationClipCount = 0;
        float maximumReconstructionError = 0.0f;
        float maximumBindIdentityError = 0.0f;
        bool valid = false;
        bool canonical = false;
    };

    struct SKELETON_BIND_BONE_INFO
    {
        uint64_t boneId = 0;
        uint64_t parentBoneId = 0;
        int32_t parentIndex = -1;
        uint32_t sourceIndex = 0;
        VEC3 localTranslation;
        float localRotationX = 0.0f;
        float localRotationY = 0.0f;
        float localRotationZ = 0.0f;
        float localRotationW = 1.0f;
        VEC3 localScale = VEC3(1.0f, 1.0f, 1.0f);
        MATRIX localBindMatrix;
        MATRIX globalBindMatrix;
        MATRIX inverseGlobalBindMatrix;
        float radius = 0.0f;
        float length = 0.0f;
        VEC3 tailOffset;
        bool hasExplicitTail = false;
        bool connectedToParent = false;
        uint32_t childCount = 0;
        uint32_t weightedVertexCount = 0;
        uint32_t animationTrackCount = 0;
        bool weightPaletteReferenced = false;
        bool hasNegativeScale = false;
        bool hasShear = false;
    };

    struct SKELETON_BIND_DIAGNOSTIC_INFO
    {
        const char *code = nullptr;
        uint32_t sourceIndex = 0;
        float observedError = 0.0f;
        bool fatal = true;
    };

    struct SKELETAL_CLIP_INFO
    {
        uint64_t clipId = 0;
        float duration = 0.0f;
        uint32_t trackCount = 0;
        bool loop = false;
    };

    struct SKELETAL_TRACK_INFO
    {
        uint64_t boneId = 0;
        uint32_t boneIndex = 0;
        uint32_t keyCount = 0;
        uint8_t channelMask = 0;
    };

    struct SKELETAL_KEY_INFO
    {
        float time = 0.0f;
        VEC3 localTranslation;
        float localRotationX = 0.0f;
        float localRotationY = 0.0f;
        float localRotationZ = 0.0f;
        float localRotationW = 1.0f;
        VEC3 localScale = VEC3(1.0f, 1.0f, 1.0f);
        uint8_t easing = 0;
        float bezierX1 = 0.0f;
        float bezierY1 = 0.0f;
        float bezierX2 = 1.0f;
        float bezierY2 = 1.0f;
    };

    struct SKELETAL_POSE_BONE_INFO
    {
        uint64_t boneId = 0;
        VEC3 localTranslation;
        float localRotationX = 0.0f;
        float localRotationY = 0.0f;
        float localRotationZ = 0.0f;
        float localRotationW = 1.0f;
        VEC3 localScale = VEC3(1.0f, 1.0f, 1.0f);
        MATRIX globalMatrix;
    };

    struct SKELETAL_RUNTIME_POSE_BONE_INFO
    {
        uint64_t boneId = 0;
        int32_t parentIndex = -1;
        MATRIX globalMatrix;
    };

    struct SKELETAL_SHARING_COMPATIBILITY
    {
        bool compatible = false;
        const char *reason = "missing_skeleton";
        uint32_t boneCount = 0;
        uint32_t boneIndex = UINT32_MAX;
        const char *boneName = nullptr;
        uint64_t boneId = 0;
        uint64_t otherBoneId = 0;
        int32_t parentIndex = -1;
        int32_t otherParentIndex = -1;
        uint64_t parentBoneId = 0;
        uint64_t otherParentBoneId = 0;
        float observedError = 0.0f;
        float tolerance = 0.0f;
    };

    class MESH_MBM_DEBUG
    {
      public:
        API_IMPL MESH_MBM_DEBUG();

        API_IMPL virtual ~MESH_MBM_DEBUG();
        API_IMPL uint32_t addBuffer(const int stride = 3);
        API_IMPL uint32_t addSubset(uint32_t indexFrame);
        API_IMPL void     removeSubset(uint32_t indexFrame, uint32_t indexSubset);
        API_IMPL bool     moveSubsetUp(uint32_t indexFrame, uint32_t indexSubset);
        API_IMPL bool     mergeSubsets(uint32_t indexFrame, const std::vector<uint32_t> &subsetIndices);
        API_IMPL uint32_t copyBufferFrom(MESH_MBM_DEBUG &src, uint32_t srcFrameIdx);
        API_IMPL uint32_t copySubsetFrom(uint32_t targetFrame, MESH_MBM_DEBUG &src, uint32_t srcFrame, uint32_t srcSubsetIdx);
        API_IMPL bool getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut, INFO_BOUND_FONT **datailFontOut,
                     std::vector<util::STAGE_PARTICLE> &lsStageParticle);
        API_IMPL static bool getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                                  util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                                  std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut = nullptr,
                                  bool *hasSkeletonOut = nullptr, uint16_t *totalBonesOut = nullptr);
        API_IMPL static const char* getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown);
        API_IMPL static std::string getExtension(const char* fileName);
        API_IMPL util::TYPE_MESH getMeshType() const noexcept;
        API_IMPL void setMeshType(const util::TYPE_MESH type) noexcept;
        API_IMPL util::TYPE_MESH getType() noexcept;
        API_IMPL util::TYPE_MESH getType(const char *fileNamePath);
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL int getFileVersion() const noexcept;
        API_IMPL util::MATERIAL &getMaterial() noexcept;
        API_IMPL const util::MATERIAL &getMaterial() const noexcept;
        API_IMPL int16_t getHasNormal() const noexcept;
        API_IMPL void setHasNormal(const int16_t hasNormalMode) noexcept;
        API_IMPL int16_t getHasTexture() const noexcept;
        API_IMPL void setHasTexture(const int16_t hasTextureMode) noexcept;
        API_IMPL const char *getFilenameMesh() const noexcept;
        API_IMPL unsigned int getModeDraw() const noexcept;
        API_IMPL void setModeDraw(const unsigned int modeDraw) noexcept;
        API_IMPL unsigned int getModeCullFace() const noexcept;
        API_IMPL void setModeCullFace(const unsigned int modeCullFace) noexcept;
        API_IMPL unsigned int getModeFrontFaceDirection() const noexcept;
        API_IMPL void setModeFrontFaceDirection(const unsigned int modeFrontFaceDirection) noexcept;
        API_IMPL void * getDetailInfo() const noexcept;
        API_IMPL void replaceDetailInfo(void *detailInfo) noexcept;
        API_IMPL uint32_t getTotalAnimationHeaders() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL void appendAnimationHeader(util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead) noexcept;
        API_IMPL void clearBlendOperations() noexcept;
        API_IMPL void resizeBlendOperations(const uint32_t totalAnimations);
        API_IMPL void setBlendOperation(const uint32_t index, const int blendOperation);
        API_IMPL uint32_t getTotalFrames() const noexcept;
        API_IMPL util::BUFFER_MESH_DEBUG *getFrameBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL uint32_t getTotalSubsets(const uint32_t indexFrame) const noexcept;
        API_IMPL util::SUBSET_DEBUG *getSubset(const uint32_t indexFrame, const uint32_t indexSubset) const noexcept;
        API_IMPL bool hasIndexBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getPositionArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getNormalArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC2 *getUvArray(const uint32_t indexFrame) const noexcept;
        API_IMPL uint16_t *getIndexArray(const uint32_t indexFrame) const noexcept;
        API_IMPL void calculateNormals();
        API_IMPL void calculateUV();
        API_IMPL void removeNormals();
        API_IMPL void addNormals();
        // Simplifies one indexed or non-indexed triangle frame atomically. Non-indexed input is
        // internally indexed by exact position/normal/UV attributes and the committed result uses
        // the existing uint16 index contract. targetSubsetIndex=-1 processes the complete virtual
        // cross-subset topology; otherwise only that zero-based material subset is reduced and
        // every other subset is copied without changing its rendered vertex attributes.
        // targetFrameIndex selects the zero-based geometry frame; -1 applies one shared collapse
        // sequence to every compatible non-skeletal frame. Multi-frame skeletal and articulated
        // assets remain unsupported.
        API_IMPL bool simplify(const float targetTriangleRatio, MESH_SIMPLIFY_REPORT &report,
                               char *errorOut, const int errorOutLen,
                               const int targetSubsetIndex = -1,
                               const int targetFrameIndex = 0,
                               const bool preserveDetails = true);
        API_IMPL void removeBuffer(uint32_t indexFrame);
        API_IMPL void removeAnimation(uint32_t index);
        // Writes the v11 section/TLV format (docs/mesh-v11-format.md): material+transform, frames,
        // physics bounding volumes, extra paths, animation headers, and (for FONT/PARTICLE/TILE_MAP
        // meshes) their own detail section. Returns false with a message in errorOut (not a partial
        // file) on failure.
        // `compress` (opt-in, default-off at every call site): when true, requests DEFLATE for
        // SECTION_FRAME_STATIC (the vertex/index buffer - the one section large enough for
        // compression to be worth it); every other section always stays uncompressed.
        API_IMPL bool saveV11(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, const bool compress, char *errorOut,const int lenErrorOut);
        API_IMPL bool loadDebugFromMemory(const MESH_MBM* meshMemory);
        // Reads the v11 section/TLV format. This is the only mesh format core_mbm reads/writes -
        // v1-v10 support has been removed entirely.
        API_IMPL bool loadV11(const char *fileNamePath);
        API_IMPL bool check(char *error,const int lenError);
        // indexSubset selects the center anchor; the calculated translation applies to every
        // subset in each selected frame so their relative placement remains unchanged.
        API_IMPL void centralizeFrame(const int indexFrame, const int indexSubset);
        // Independently centralizes each selected subset. Unselected subsets are not moved.
        API_IMPL void centralizeFrameItself(const int indexFrame, const int indexSubset);
        API_IMPL void rotateFrame(const int indexFrame, const int indexSubset, const float angleX, const float angleY, const float angleZ);
        API_IMPL void scaleFrame(const int indexFrame, const int indexSubset, const float sx, const float sy, const float sz);
        API_IMPL bool scaleSkeletalAsset(const float scale, char *errorOut, const int errorOutLen);
        API_IMPL void translateFrame(const int indexFrame, const int indexSubset, const float dx, const float dy, const float dz);
        API_IMPL bool addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                            const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                            char *strErrorOut, const int strErrorOutLen);
        API_IMPL bool addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex);
        API_IMPL int addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut, const int errorOutLen);
        API_IMPL bool updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError);
        API_IMPL const util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnim(const uint32_t index)const;
        API_IMPL const char *getAnimationEffectTexture(const uint32_t index) const noexcept;
        API_IMPL bool setAnimationEffectTexture(const uint32_t index, const char *fileName) noexcept;
        API_IMPL bool getSkeletalSharingCompatibility(const MESH_MBM_DEBUG &other,
                                                       SKELETAL_SHARING_COMPATIBILITY &out) const noexcept;
        // Read-only views of the canonical skeleton compiled and validated during load.
        API_IMPL bool getSkeletonBindSummary(SKELETON_BIND_SUMMARY &out) const noexcept;
        API_IMPL bool getSkeletonBindBone(const uint32_t index, SKELETON_BIND_BONE_INFO &out,
                                          const bool includeDependencyImpact = true) const noexcept;
        API_IMPL const char *getSkeletonBindBoneName(const uint32_t index) const noexcept;
        API_IMPL bool getSkeletonBindDiagnostic(const uint32_t index,
                                                SKELETON_BIND_DIAGNOSTIC_INFO &out) const noexcept;
        API_IMPL uint32_t getTotalSkeletalClips() const noexcept;
        API_IMPL bool getSkeletalClip(const uint32_t clipIndex, SKELETAL_CLIP_INFO &out) const noexcept;
        API_IMPL const char *getSkeletalClipName(const uint32_t clipIndex) const noexcept;
        API_IMPL bool getSkeletalTrack(const uint32_t clipIndex, const uint32_t trackIndex,
                                       SKELETAL_TRACK_INFO &out) const noexcept;
        API_IMPL bool getSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                     const uint32_t keyIndex, SKELETAL_KEY_INFO &out) const noexcept;
        API_IMPL bool addSkeletalClip(const char *name, const float duration, const bool loop,
                                      uint32_t *newIndexOut, char *errorOut, const int errorOutLen);
        API_IMPL bool updateSkeletalClip(const uint32_t clipIndex, const char *name,
                                         const float duration, const bool loop,
                                         char *errorOut, const int errorOutLen);
        API_IMPL bool removeSkeletalClip(const uint32_t clipIndex,
                                         char *errorOut, const int errorOutLen);
        API_IMPL bool addSkeletalTrack(const uint32_t clipIndex, const uint32_t boneIndex,
                                       const uint8_t channelMask, uint32_t *newIndexOut,
                                       char *errorOut, const int errorOutLen);
        API_IMPL bool updateSkeletalTrackChannels(const uint32_t clipIndex, const uint32_t trackIndex,
                                                  const uint8_t channelMask,
                                                  char *errorOut, const int errorOutLen);
        API_IMPL bool removeSkeletalTrack(const uint32_t clipIndex, const uint32_t trackIndex,
                                          char *errorOut, const int errorOutLen);
        API_IMPL bool addSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                     const float time, uint32_t *newIndexOut,
                                     char *errorOut, const int errorOutLen);
        API_IMPL bool updateSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                        const uint32_t keyIndex, const float time,
                                        const VEC3 &translation, const float rotationX,
                                        const float rotationY, const float rotationZ,
                                        const float rotationW, const VEC3 &scale,
                                        const uint8_t easing, const float bezierX1,
                                        const float bezierY1, const float bezierX2,
                                        const float bezierY2, char *errorOut, const int errorOutLen);
        // Moves multiple existing keys by one time delta on a candidate copy. Track/key indices
        // refer to the pre-move ordering; validation and commit are atomic.
        API_IMPL bool moveSkeletalKeys(const uint32_t clipIndex, const uint32_t *trackIndices,
                                       const uint32_t *keyIndices, const uint32_t keyCount,
                                       const float timeDelta, char *errorOut, const int errorOutLen);
        API_IMPL bool duplicateSkeletalKeys(const uint32_t clipIndex,
                                            const uint32_t *trackIndices,
                                            const uint32_t *keyIndices, const uint32_t keyCount,
                                            const float timeDelta, char *errorOut,
                                            const int errorOutLen);
        // Pastes detached key payloads into one destination clip. Tracks are resolved by stable
        // bone ID, missing tracks are created, and the complete candidate validates atomically.
        API_IMPL bool pasteSkeletalKeys(const uint32_t clipIndex, const uint64_t *boneIds,
                                        const uint8_t *channelMasks,
                                        const SKELETAL_KEY_INFO *keys, const uint32_t keyCount,
                                        const float sourceMinimumTime, const float insertionTime,
                                        char *errorOut, const int errorOutLen);
        API_IMPL bool insertSkeletalKeysRipple(const uint32_t clipIndex,
                                               const uint32_t *trackIndices,
                                               const uint32_t *keyIndices,
                                               const uint32_t keyCount,
                                               const float insertionTime, char *errorOut,
                                               const int errorOutLen);
        API_IMPL bool insertSkeletalEmptyTime(const uint32_t clipIndex,
                                              const float insertionTime,
                                              const float duration, char *errorOut,
                                              const int errorOutLen);
        API_IMPL bool removeSkeletalTimeRange(const uint32_t clipIndex,
                                              const float startTime,
                                              const float duration,
                                              uint32_t *removedKeyCountOut,
                                              char *errorOut, const int errorOutLen);
        API_IMPL bool removeSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                        const uint32_t keyIndex, char *errorOut, const int errorOutLen);
        API_IMPL bool commitSkeletalAuthoringKey(const uint32_t clipIndex, const uint32_t boneIndex,
                                                 const float time, const uint8_t channelMask,
                                                 const SKELETAL_KEY_INFO &local,
                                                 bool *createdKeyOut, char *errorOut,
                                                 const int errorOutLen);
        API_IMPL bool commitSkeletalAuthoringPose(const uint32_t clipIndex, const float time,
                                                  const uint64_t *boneIds,
                                                  const SKELETAL_KEY_INFO *locals,
                                                  const uint32_t boneCount,
                                                  char *errorOut, const int errorOutLen);
        // Evaluates an editor-only pose from the unsaved canonical clip state. An optional local
        // override is applied after sampling and before hierarchy/global/palette reconstruction.
        API_IMPL bool evaluateSkeletalAuthoringPose(const uint32_t clipIndex, const float time,
                                                    const int32_t overrideBoneIndex,
                                                    const SKELETAL_KEY_INFO *overrideLocal,
                                                    const SKELETAL_SHADER_METHOD method,
                                                    char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getSkeletalAuthoringPoseBoneCount() const noexcept;
        API_IMPL bool getSkeletalAuthoringPoseBone(const uint32_t boneIndex,
                                                   SKELETAL_POSE_BONE_INFO &out) const noexcept;
        API_IMPL uint32_t getSkeletalAuthoringPaletteSize() const noexcept;
        API_IMPL bool copySkeletalAuthoringPalette(float *rows, const uint32_t rowCount) const noexcept;
        API_IMPL bool renameSkeletalBone(const uint32_t index, const char *name,
                                         char *errorOut, const int errorOutLen);
        // newParentIndex is -1 for a root or a zero-based compiled/source index otherwise.
        API_IMPL bool reparentSkeletalBone(const uint32_t index, const int32_t newParentIndex,
                                           const bool preserveGlobalBind,
                                           char *errorOut, const int errorOutLen);
        // Replaces one bone's parent-relative bind TRS and display metadata. Stable identity and
        // hierarchy are preserved; the edited local transform deliberately moves its subtree.
        API_IMPL bool setSkeletalBoneBind(const uint32_t index, const VEC3 &translation,
                                          const float rotationX, const float rotationY,
                                          const float rotationZ, const float rotationW,
                                          const VEC3 &scale, const float radius, const float length,
                                          char *errorOut, const int errorOutLen);
        // Replaces explicit Bone Editor geometry and keeps explicitly connected child heads on the
        // same parent-local point. preserveOtherJoints compensates every joint outside the edited
        // shared joint in global bind space. Runtime bind orientation is not inferred from the tail.
        API_IMPL bool setSkeletalBoneTail(const uint32_t index, const VEC3 &tailOffset,
                                          const bool hasExplicitTail, const bool preserveOtherJoints,
                                          char *errorOut, const int errorOutLen);
        // Moves one transform head in parent-local space while preserving its explicit tail in
        // global bind space. Connected children remain attached to that preserved tail;
        // preserveOtherJoints compensates the remaining hierarchy in global bind space.
        API_IMPL bool setSkeletalBoneHead(const uint32_t index, const VEC3 &translation,
                                          const bool preserveOtherJoints,
                                          char *errorOut, const int errorOutLen);
        // Translates one complete authored segment by moving its transform head while retaining its
        // bone-local tail. Connected child heads follow the tail; optional compensation preserves
        // every other joint in global bind space.
        API_IMPL bool translateSkeletalBoneSegment(const uint32_t index, const VEC3 &translation,
                                                    const bool preserveOtherJoints,
                                                    char *errorOut, const int errorOutLen);
        // Connects/disconnects a bone head to/from its current parent's explicit tail. Connecting
        // preserves this bone's global tail and can compensate all other global joint transforms.
        API_IMPL bool setSkeletalBoneConnectedToParent(const uint32_t index, const bool connected,
                                                        const bool preserveOtherJoints,
                                                        char *errorOut, const int errorOutLen);
        // Updates positive authoring/picking radius for one bone or its complete descendant subtree.
        API_IMPL bool setSkeletalBoneRadius(const uint32_t index, const float radius,
                                            const bool includeDescendants,
                                            char *errorOut, const int errorOutLen);
        // Adds a parent-first canonical transform with a new opaque stable ID. parentIndex is -1
        // for root; hasExplicitTail distinguishes a selectable bone segment from a joint only.
        API_IMPL bool addSkeletalBone(const int32_t parentIndex, const char *name,
                                      const VEC3 &translation, const float radius, const float length,
                                      const bool hasExplicitTail, const bool connectedToParent,
                                      uint32_t *newIndexOut, char *errorOut, const int errorOutLen);
        // Atomically appends count parent-linked bones named prefix1..prefixN.
        API_IMPL bool addSkeletalBoneChain(const int32_t parentIndex, const char *namePrefix,
                                           const uint32_t count, const VEC3 &stepTranslation,
                                           const float radius, const float length,
                                           uint32_t *lastIndexOut, char *errorOut, const int errorOutLen);
        // Atomically extends an explicit tail with connected segments that continue its direction.
        API_IMPL bool extendSkeletalBoneTail(const uint32_t index, const uint32_t count,
                                             const float radius, const float length,
                                             uint32_t *lastIndexOut,
                                             char *errorOut, const int errorOutLen);
        // Duplicates a subtree by reflecting global bind matrices across axis 0=X, 1=Y, 2=Z.
        API_IMPL bool mirrorSkeletalBoneSubtree(const uint32_t index, const uint32_t axis,
                                                const char *namePrefix, uint32_t *newRootIndexOut,
                                                char *errorOut, const int errorOutLen);
        // Creates section 41 with one root transform on a loaded static mesh without skeletal data.
        API_IMPL bool initializeSkeletalSkeleton(const char *rootName, const VEC3 &translation,
                                                 const float radius, const float length,
                                                 const bool hasExplicitTail,
                                                 char *errorOut, const int errorOutLen);
        // Strict removal: only an unreferenced leaf may be deleted. No implicit remapping occurs.
        API_IMPL bool removeSkeletalBone(const uint32_t index,
                                         char *errorOut, const int errorOutLen);
        // Removes an unbranched bone while explicitly transferring its palette entry and optionally
        // deleting its tracks. The replacement is an existing zero-based bone index.
        API_IMPL bool removeSkeletalBoneRemapped(const uint32_t index, const uint32_t replacementIndex,
                                                 const bool discardAnimationTracks,
                                                 const bool reparentChildrenPreserveGlobal,
                                                 char *errorOut, const int errorOutLen);
        // Canonical SECTION_SKELETAL_WEIGHTS editor surface. Names are UI lookup keys only: every
        // accepted name is resolved to the skeleton's stable boneId before type-42 storage changes.
        // An asset without an existing canonical skeleton/weight section is rejected rather than
        // promoted from, or mirrored into, the exploratory name-palette representation above.
        // Deprecated compatibility wrapper. New editor code must use the atomic batch operation.
        API_IMPL bool setSkeletalVertexWeight(const uint32_t vertexIndex,
                                               const char *boneName0, const float weight0,
                                               const char *boneName1, const float weight1,
                                               const char *boneName2, const float weight2,
                                               const char *boneName3, const float weight3,
                                               char *errorOut, const int errorOutLen);
        // Atomically validates and applies a detached set of unique vertex edits. The caller-owned
        // records are consumed only for the duration of the call; no pointer or container is retained.
        API_IMPL bool setSkeletalVertexWeightsBatch(const SKELETAL_VERTEX_WEIGHT_EDIT *edits,
                                                     const uint32_t editCount,
                                                     char *errorOut, const int errorOutLen);
        API_IMPL bool getSkeletalVertexWeight(const uint32_t vertexIndex,
                                               const char **boneName0, float *weight0,
                                               const char **boneName1, float *weight1,
                                               const char **boneName2, float *weight2,
                                               const char **boneName3, float *weight3) const noexcept;
        // Creates complete frame-zero type-42 weights rigidly bound to one existing bone.
        API_IMPL bool initializeSkeletalVertexWeights(const uint32_t boneIndex,
                                                       uint32_t *vertexCountOut,
                                                       char *errorOut, const int errorOutLen);
        // Removes only canonical type-42 weights, preserving the skeleton and animation clips.
        API_IMPL bool removeSkeletalVertexWeights(uint32_t *vertexCountOut,
                                                  char *errorOut, const int errorOutLen);
        // Atomically removes the complete canonical 41-43 skeletal asset. Counts report impact.
        API_IMPL bool removeAllSkeletalData(uint32_t *boneCountOut, uint32_t *vertexCountOut,
                                            uint32_t *clipCountOut,
                                            char *errorOut, const int errorOutLen);
        API_IMPL bool hasSkeletalVertexWeights() const noexcept;
        API_IMPL uint32_t getTotalSkeletalWeightBones() const noexcept;
        // Rigid/articulated animation authoring data. The storage remains PIMPL-owned; these
        // narrow operations are the editor-facing API and are also suitable for Lua bindings.
        API_IMPL uint32_t getTotalArticulatedParts() const noexcept;
        API_IMPL const util::ARTICULATED_PART_V11 *getArticulatedPart(const uint32_t index) const noexcept;
        API_IMPL uint32_t initializeArticulatedParts();
        // Removes all parts/pivots and tracks that reference those parts. Clips remain available.
        API_IMPL uint32_t removeArticulatedParts() noexcept;
        API_IMPL int addArticulatedPart(const uint64_t partId, const uint32_t frameIndex,
                                        const uint32_t subsetIndex, const char *name,
                                        const float pivotX, const float pivotY, const float pivotZ,
                                        const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                        const uint64_t parentPartId, char *errorOut, const int errorOutLen);
        API_IMPL bool updateArticulatedPart(const uint32_t index, const char *name,
                                            const float pivotX, const float pivotY, const float pivotZ,
                                            const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                            const uint64_t parentPartId, char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getTotalArticulatedAnimations() const noexcept;
        API_IMPL const char *getArticulatedAnimationName(const uint32_t index) const noexcept;
        API_IMPL bool getArticulatedAnimation(const uint32_t index, const char **name, float *duration,
                                              float *speed, int *priority, bool *loop,
                                              uint8_t *blendMode) const noexcept;
        API_IMPL bool updateArticulatedAnimation(const uint32_t index, const char *name, const float duration,
                                                 const float speed, const int priority, const bool loop,
                                                 const uint8_t blendMode,
                                                 char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getTotalArticulatedTracks(const uint32_t animationIndex) const noexcept;
        API_IMPL bool getArticulatedTrack(const uint32_t animationIndex, const uint32_t trackIndex,
                                          uint64_t *partId, uint8_t *channelMask, uint32_t *keyCount) const noexcept;
        API_IMPL bool getArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                        const uint32_t keyIndex, float *time,
                                        float *positionX, float *positionY, float *positionZ,
                                        float *rotationX, float *rotationY, float *rotationZ, float *rotationW,
                                        float *scaleX, float *scaleY, float *scaleZ,
                                        uint8_t *easing,
                                        float *bezierX1, float *bezierY1,
                                        float *bezierX2, float *bezierY2,
                                        float *rotationEulerX, float *rotationEulerY,
                                        float *rotationEulerZ, bool *hasRotationEuler) const noexcept;
        API_IMPL int addArticulatedAnimation(const char *name, const float duration, const float speed,
                                             const int priority, const bool loop, const uint8_t blendMode,
                                             char *errorOut, const int errorOutLen);
        API_IMPL bool removeArticulatedAnimation(const uint32_t animationIndex, char *errorOut, const int errorOutLen);
        API_IMPL int addArticulatedTrack(const uint32_t animationIndex, const uint64_t partId,
                                         const uint8_t channelMask, char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedTrackChannels(const uint32_t animationIndex,
                                                  const uint32_t trackIndex,
                                                  const uint8_t channelMask,
                                                  char *errorOut, const int errorOutLen);
        API_IMPL bool addArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                        const float time, const float positionX, const float positionY, const float positionZ,
                                        const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                        const float scaleX, const float scaleY, const float scaleZ,
                                        char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyEuler(const uint32_t animationIndex, const uint32_t trackIndex,
                                             const float time, const float rotationEulerX,
                                             const float rotationEulerY, const float rotationEulerZ,
                                             char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyEasing(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex, const uint8_t easing,
                                              char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyBezier(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex,
                                              const float x1, const float y1,
                                              const float x2, const float y2,
                                              char *errorOut, const int errorOutLen);
        API_IMPL bool updateArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const uint32_t keyIndex, const float time,
                                           const float positionX, const float positionY, const float positionZ,
                                           const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                           const float scaleX, const float scaleY, const float scaleZ,
                                           char *errorOut, const int errorOutLen);
        API_IMPL bool removeArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const uint32_t keyIndex, char *errorOut, const int errorOutLen);
        API_IMPL void fixDefaultBoud();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
      private:
        friend API_IMPL bool skeletal::copyCanonicalParityAsset(
            const MESH_MBM_DEBUG &mesh, skeletal::CANONICAL_PARITY_ASSET &out) noexcept;
        void fillAtLeastOneBound();
        bool fillInSubsetDebug(const MESH_MBM* meshMemory,
                               const int currentFrame,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
                               util::HEADER_FRAME* headerFrame,
                               util::BUFFER_MESH_DEBUG* pBuffer);//need to be implemented by specific backend engine
        bool readDebugTriangleDetailCompat(util::MEM_CURSOR_V11 &fp, const char *fileNamePath, const int totalBounding);
        bool readFrameStaticV11Payload(util::MEM_CURSOR_V11 &fp, const util::BUFFER_MESH_DEBUG *frame0, util::BUFFER_MESH_DEBUG *&out,
                                       util::FRAME_HEADER_V11 &outFrameHeader);
        std::vector<std::string> getKnowPathsToExtraHeader();

        struct Impl;
        std::unique_ptr<Impl> impl;
    };


    class MESH_MBM
    {
        friend class MESH_MANAGER;
        friend class ANIMATION_MANAGER;
        friend class MESH;
      public:
        API_IMPL BUFFER_MESH *getBuffer(const uint32_t index) const;
        API_IMPL TEXTURE *getTexture(const uint32_t indexFrame, const uint32_t indexSubset);
        API_IMPL bool setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                               const bool hasAlpha);
        API_IMPL TEXTURE *getMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role) const noexcept;
        API_IMPL bool setMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role,
                               const char *fileNameTexture, const bool hasAlpha) const;
        API_IMPL const char *getFilenameMesh() const;
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL void resetPhysicsInfo();
        API_IMPL void appendPhysicsCube(CUBE *cube) noexcept;
        API_IMPL void appendPhysicsSphere(SPHERE *sphere) noexcept;
        API_IMPL void appendPhysicsCubeComplex(CUBE_COMPLEX *cubeComplex) noexcept;
        API_IMPL void appendPhysicsTriangle(TRIANGLE *triangle) noexcept;
        API_IMPL util::INFO_ANIMATION &getAnimationInfo() noexcept;
        API_IMPL const util::INFO_ANIMATION &getAnimationInfo() const noexcept;
        API_IMPL uint32_t getTotalAnimations() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL virtual ~MESH_MBM();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
        API_IMPL bool isLoaded() const;
        API_IMPL bool render(const uint32_t indexFrame,const SHADER *pShader,
                             const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL bool hasArticulatedAnimationData() const noexcept;
        API_IMPL uint32_t getTotalArticulatedAnimations() const noexcept;
        API_IMPL const char *getArticulatedAnimationName(const uint32_t index) const noexcept;
        API_IMPL bool hasActiveArticulatedAnimations(const ARTICULATED_ANIMATION_PLAYER &player) const noexcept;
        API_IMPL bool playArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name, const int priority,
                                               const float blendDuration = 0.0f,
                                               const float weight = 1.0f) const;
        API_IMPL bool pauseArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                const char *name) const noexcept;
        API_IMPL bool resumeArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                 const char *name) const noexcept;
        API_IMPL bool disableArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                  const char *name) const noexcept;
        API_IMPL bool seekArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name, const float time) const noexcept;
        API_IMPL bool getArticulatedAnimationTime(const ARTICULATED_ANIMATION_PLAYER &player,
                                                  const char *name, float *time) const noexcept;
        API_IMPL void updateArticulatedAnimations(ARTICULATED_ANIMATION_PLAYER &player,
                                                  const float delta, RENDERIZABLE *owner,
                                                  OnEndAnimation onEndAnimation) const;
        API_IMPL bool getArticulatedTransform(const ARTICULATED_ANIMATION_PLAYER &player,
                                              const uint32_t frameIndex, const uint32_t subsetIndex,
                                              VEC3 *translation, float rotationQuaternion[4], VEC3 *scale,
                                              VEC3 *pivot, float pivotQuaternion[4]) const noexcept;
        API_IMPL bool renderArticulatedStatic(const ARTICULATED_ANIMATION_PLAYER &player,
                                              const uint32_t indexFrame, const SHADER *pShader,
                                              const MATRIX &viewMatrix, const MATRIX &perspectiveMatrix,
                                              const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL bool renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                        VEC2 *uv,
                                        const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL util::TYPE_MESH getTypeMesh() const;
        API_IMPL VEC2 getZoomEditorSprite() const;
        API_IMPL uint32_t getTotalFrame() const;
        API_IMPL uint32_t getTotalSubset(const uint32_t indexFrame) const;
        API_IMPL const INFO_BOUND_FONT* getInfoFont()const;
        const std::vector<util::STAGE_PARTICLE*>* getInfoParticle()const;
        API_IMPL const util::BTILE_INFO* getInfoTile()const;
        API_IMPL const util::DYNAMIC_SHAPE* getInfoShape()const;
        
      private:
        MESH_MBM();
        bool buildArticulatedTransformMatrix(const ARTICULATED_ANIMATION_PLAYER &player,
                                             const uint32_t frameIndex, const uint32_t subsetIndex,
                                             MATRIX *out) const noexcept;
        bool load(const char *fileNamePath);
        bool load(const char *fileNamePath, RENDERIZABLE *renderizable);
        // Reads the v11 section/TLV format. This is the only mesh format - it backs
        // load()/MESH_MANAGER::load() directly.
        bool loadV11(const char *fileNamePath);
        // Main-thread-only GPU-finish half of loadV11 (async loading) - see mesh-manager.cpp's
        // IntermediateMeshV11/finishLoadFromIntermediate comments. Takes the forward-declared struct
        // below by reference; defined in mesh-manager.cpp only, same forward-declare-in-header
        // pattern as Impl.
        bool finishLoadFromIntermediate(MESH_LOAD_INTERMEDIATE_V11 &in, const char *fileNamePath);
        uint32_t getPreparedSkeletalPaletteSize(SKELETAL_SHADER_METHOD method) const noexcept;
        bool supportsGpuSkeletalPath(SKELETAL_SHADER_METHOD method) const noexcept;
        void resolveSkeletalSkinningMethod(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        void getSkeletalSkinningReport(SKELETAL_SHADER_METHOD method, const char **status,
                                       uint32_t *requiredBoneCount,
                                       uint32_t *effectiveBoneCapacity) const noexcept;
        bool canUseCpuSkeletalPath(SKELETAL_SHADER_METHOD method,
                                   const SKELETAL_ANIMATION_PLAYER *player = nullptr,
                                   const char **reason = nullptr) const noexcept;
        bool renderCpuSkeletal(const SKELETAL_ANIMATION_PLAYER &player,
                               const uint32_t indexFrame, BUFFER_MESH &dynamicBuffer,
                               std::vector<VEC3> &positions, std::vector<VEC3> &normals,
                               std::vector<VEC2> &uvs, bool &initialized,
                               const SHADER *pShader,
                               const RENDERIZABLE *renderizableOwner = nullptr) const;
        uint32_t getTotalSkeletalAnimations() const noexcept;
        const char *getSkeletalAnimationName(uint32_t index) const noexcept;
        bool getSkeletalAnimationDuration(uint32_t index, float *duration) const noexcept;
        bool playSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, const char *name) const;
        bool crossFadeSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, const char *name,
                                        float duration) const;
        bool hasActiveSkeletalAnimation(const SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool pauseSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool resumeSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool stopSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool seekSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, float time) const;
        bool getSkeletalAnimationTime(const SKELETAL_ANIMATION_PLAYER &player, float *time) const noexcept;
        bool setSkeletalAnimationPlaybackSpeed(SKELETAL_ANIMATION_PLAYER &player,
                                               float speed) const noexcept;
        float getSkeletalAnimationPlaybackSpeed(
            const SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool playSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                const char *name, float weight) const;
        bool playSkeletalAnimationAdditiveLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                const char *name, float weight) const;
        bool pauseSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool resumeSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool isSkeletalAnimationLayerPaused(
            const SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool stopSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player) const;
        bool seekSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player, float time) const;
        bool setSkeletalAnimationAbsoluteLayerWeight(SKELETAL_ANIMATION_PLAYER &player,
                                                     float weight) const;
        bool fadeSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                float targetWeight, float duration) const;
        bool getSkeletalAnimationAbsoluteLayerWeight(const SKELETAL_ANIMATION_PLAYER &player,
                                                     float *weight) const noexcept;
        bool getSkeletalAnimationAbsoluteLayerTime(const SKELETAL_ANIMATION_PLAYER &player,
                                                   float *time) const noexcept;
        bool setSkeletalAnimationLayerBoneWeight(SKELETAL_ANIMATION_PLAYER &player,
                                                 uint64_t boneId, float weight) const;
        bool setSkeletalAnimationLayerBoneWeights(SKELETAL_ANIMATION_PLAYER &player,
                                                  const uint64_t *boneIds, const float *weights,
                                                  uint32_t count) const;
        bool getSkeletalAnimationLayerBoneWeight(const SKELETAL_ANIMATION_PLAYER &player,
                                                 uint64_t boneId, float *weight) const noexcept;
        bool clearSkeletalAnimationLayerMask(SKELETAL_ANIMATION_PLAYER &player) const;
        uint32_t getSkeletalAnimationPoseBoneCount(
            const SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool getSkeletalAnimationPoseBone(const SKELETAL_ANIMATION_PLAYER &player,
                                          uint32_t boneIndex,
                                          SKELETAL_RUNTIME_POSE_BONE_INFO &out) const noexcept;
        bool getSkeletalBoneTransform(const SKELETAL_ANIMATION_PLAYER &player,
                                      const char *boneName, const MATRIX *modelMatrix,
                                      uint64_t *boneId, MATRIX *matrix, VEC3 *position,
                                      float rotation[4], VEC3 *angle, VEC3 *scale) const noexcept;
        bool getSkeletalRootMotionDelta(const SKELETAL_ANIMATION_PLAYER &player,
                                        const char *boneName, const MATRIX *modelMatrix,
                                        uint64_t *boneId, VEC3 *translation) const noexcept;
        bool getSkeletalSharingCompatibility(const MESH_MBM &other,
                                             SKELETAL_SHARING_COMPATIBILITY &out) const noexcept;
        bool hasSkeletalRenderPalette(const SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool enableAutomaticSkeletalRootMotion(SKELETAL_ANIMATION_PLAYER &player,
                                               const char *boneName,
                                               bool applyRotation = false) const noexcept;
        bool disableAutomaticSkeletalRootMotion(SKELETAL_ANIMATION_PLAYER &player) const noexcept;
        bool getAutomaticSkeletalRootMotionBone(const SKELETAL_ANIMATION_PLAYER &player,
                                                const char **boneName,
                                                uint64_t *boneId,
                                                bool *applyRotation = nullptr) const noexcept;
        bool setSkeletalAuthoringPalette(SKELETAL_ANIMATION_PLAYER &player,
                                         SKELETAL_SHADER_METHOD method, const float *rows,
                                         uint32_t rowCount, const uint64_t *orderedBoneIds,
                                         uint32_t boneIdCount, float time, char *errorOut,
                                         int errorOutLen) const noexcept;
        bool updateSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, float delta,
                                     RENDERIZABLE *owner = nullptr,
                                     OnEndAnimation onEndAnimation = nullptr) const;
        bool playSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player, const char *name,
                                        float weight, bool additive) const;
        bool renderSkeletal(const SKELETAL_ANIMATION_PLAYER &player, uint32_t indexFrame,
                            const SHADER *shader, const RENDERIZABLE *owner);

        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    using MeshAsyncLoadCallback = std::function<void(MESH_MBM *mesh, bool success)>;

    class MESH_MANAGER
    {
      private:
        static MESH_MANAGER *instanceMeshManager;

      public:

        API_IMPL static MESH_MANAGER *getInstance();
        API_IMPL static void release();
        API_IMPL void fakeRelease(const char* fileName);
        API_IMPL MESH_MBM *load(const char *fileName);
        API_IMPL MESH_MBM *load(const char *fileName, RENDERIZABLE *renderizable);
        API_IMPL MESH_MBM *loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng,TEXTURE ** texture_loaded);
        API_IMPL MESH_MBM *load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode);
        API_IMPL MESH_MBM *loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer, uint16_t *index,const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode);
        API_IMPL MESH_MBM *loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode, const util::DYNAMIC_SHAPE & dynamic_shape_info);
        API_IMPL MESH_MBM *getIfExists(const char* fileName);
        API_IMPL static const char * typeClassName(const util::TYPE_MESH type) noexcept;
        // Milestone 6: async loading. loadAsync() does file I/O + v11 parsing on a worker thread
        // (lazily starts a small fixed pool on first use); onComplete always fires from
        // pumpAsyncLoads() on the main thread (never inline, even on a cache hit) - call
        // pumpAsyncLoads() once per frame (CORE_MANAGER::update() does this) to finish the load
        // (GPU buffer/texture creation, which must happen on the thread owning the GL context) and
        // dispatch completions. Not wired into any render call site yet - load()/loadIndex()/etc.
        // are unaffected.
        API_IMPL void loadAsync(const char *fileName, MeshAsyncLoadCallback onComplete);
        API_IMPL void pumpAsyncLoads();
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        MESH_MANAGER();
        virtual ~MESH_MANAGER();
    };
}

#endif
