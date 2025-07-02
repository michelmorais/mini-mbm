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

#include <shape-mesh.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <cfloat>
#include <util-interface.h>
#include <util.h>

namespace mbm 
{
    SHAPE_MESH::SHAPE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_SHAPE_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->onRenderDynamicBuffer = nullptr;
        this->device->addRenderizable(this);
    }
    
    SHAPE_MESH::~SHAPE_MESH()
    {
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void SHAPE_MESH::release()
    {
        this->releaseAnimation();
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->onRenderDynamicBuffer = nullptr;
        dynamicVertex.clear();
        dynamicNormal.clear();
        dynamicUV.clear();
        dynamicIndex.clear();
    }
    
    bool SHAPE_MESH::loadIndexedDynamic(const char *nickName, std::vector<float> &&_dynamicVertex,
                                   std::vector<float> &&_dynamicNormal, std::vector<float> &&_dynamicUV,
                                   std::unique_ptr<unsigned short int[],DeleteArrayUnShortInt> &&indexArray,
                                   const unsigned int _sizeVertexArray, const unsigned int _sizeIndexArray,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        if (_dynamicVertex.size() == 0 || _sizeVertexArray == 0 || _sizeIndexArray == 0 || indexArray == nullptr)
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        unsigned int  sizeVertexBuffer = 0;
        if (this->is3D)
            sizeVertexBuffer = _sizeVertexArray;
        else
            sizeVertexBuffer = (_sizeVertexArray / 2) * 3;
        VEC3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        VEC3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        dynamicVertex.clear();
        dynamicNormal.clear();
        dynamicUV.clear();
        dynamicIndex.clear();
        const bool hasNormal = _dynamicNormal.size() ? true : false;
        const bool hasUV     = _dynamicUV.size()     ? true : false;
        if (sizeVertexBuffer == _sizeVertexArray)
        {
            this->dynamicVertex = std::move(_dynamicVertex);
        }
        else
        {
            this->dynamicVertex.resize(sizeVertexBuffer);
            auto vertex     = dynamicVertex.data();
            auto vertexFrom = _dynamicVertex.data();
            for (unsigned int p = 0, i = 0; p < sizeVertexBuffer; p += 3, i += 2)
            {
                vertex[p]     = vertexFrom[i];
                vertex[p + 1] = vertexFrom[i + 1];
                vertex[p + 2] = 0.0f;
            }
        }
        if (hasNormal)
            this->dynamicNormal = std::move(_dynamicNormal);
        else
            this->dynamicNormal.resize(sizeVertexBuffer);
        if (hasUV)
        {
            this->dynamicUV = std::move(_dynamicUV);
        }
        else
        {
            const unsigned sizeUv = (sizeVertexBuffer / 3) * 2;
            this->dynamicUV.resize(sizeUv);
        }

        if (this->is3D)
        {
            auto vertex = dynamicVertex.data();
            for (unsigned int p = 0; p < sizeVertexBuffer; p += 3)
            {
                if (vertex[p] > vMax.x)
                    vMax.x = vertex[p];
                if (vertex[p + 1] > vMax.y)
                    vMax.y = vertex[p + 1];
                if (vertex[p + 2] > vMax.z)
                    vMax.z = vertex[p + 2];

                if (vertex[p] < vMin.x)
                    vMin.x = vertex[p];
                if (vertex[p + 1] < vMin.y)
                    vMin.y = vertex[p + 1];
                if (vertex[p + 2] < vMin.z)
                    vMin.z = vertex[p + 2];
            }
            if (hasNormal == false)
            {
                auto normal = dynamicNormal.data();
                auto index  = indexArray.get();
                // recalcula as normais
                for (unsigned int i = 0; (i + 8) < sizeVertexBuffer; i += 9)
                {
                    const unsigned int index0 = i;
                    const unsigned int index1 = i + 3;
                    const unsigned int index2 = i + 6;
                    VEC3   a, b, n;
                    a.x = vertex[index[index1]] - vertex[index[index0]];
                    a.y = vertex[index[index1 + 1]] - vertex[index[index0 + 1]];
                    a.z = vertex[index[index1 + 2]] - vertex[index[index0 + 2]];

                    b.x = vertex[index[index2]] - vertex[index[index0]];
                    b.y = vertex[index[index2 + 1]] - vertex[index[index0 + 1]];
                    b.z = vertex[index[index2 + 2]] - vertex[index[index0 + 2]];

                    vec3Cross(&n, &a, &b);
                    vec3Normalize(&n, &n);

                    normal[index[index0]]     = n.x;
                    normal[index[index0 + 1]] = n.y;
                    normal[index[index0 + 2]] = n.z;

                    normal[index[index1]]     = n.x;
                    normal[index[index1 + 1]] = n.y;
                    normal[index[index1 + 2]] = n.z;

                    normal[index[index2]]     = n.x;
                    normal[index[index2 + 1]] = n.y;
                    normal[index[index2 + 2]] = n.z;
                }
            }
        }
        else
        {
            vMax.z      = 0;
            vMin.z      = 0;
            auto vertex = dynamicVertex.data();
            for (unsigned int p = 0; p < sizeVertexBuffer; p += 3)
            {
                vertex[p + 2] = 0;
                if (vertex[p] > vMax.x)
                    vMax.x = vertex[p];
                if (vertex[p + 1] > vMax.y)
                    vMax.y = vertex[p + 1];

                if (vertex[p] < vMin.x)
                    vMin.x = vertex[p];
                if (vertex[p + 1] < vMin.y)
                    vMin.y = vertex[p + 1];
            }
            if (hasNormal == false)
            {
                auto normal = dynamicNormal.data();
                for (unsigned int p = 0; p < sizeVertexBuffer; p += 3)
                {
                    normal[p]     = 0;
                    normal[p + 1] = 0;
                    normal[p + 2] = -1;
                }
            }
        }
        if (hasUV == false)
        {
            auto        vertex = dynamicVertex.data();
            auto        duv    = dynamicUV.data();
            const float width  = vMax.x - vMin.x;
            const float height = vMax.y - vMin.y;
            for (unsigned int uv = 0, p = 0; p < sizeVertexBuffer; p += 3, uv += 2)
            {
                duv[uv]     = (vertex[p] - vMin.x) / width;
                duv[uv + 1] = 1.0f - ((vertex[p + 1] - vMin.y) / height);
            }
        }
        std::string fileNameOldTexture;
        bool        hasOldAlpha = false;
        if (mesh && mesh->isLoaded())
        {
            mbm::TEXTURE *tex = mesh->getTexture(0, 0);
            if (tex)
            {
                fileNameOldTexture = tex->getFileNameTexture();
                hasOldAlpha        = tex->useAlphaChannel;
            }
        }
        dynamicIndex.resize(_sizeIndexArray);
        for (unsigned int i = 0; i < _sizeIndexArray; ++i)
        {
            dynamicIndex[i] = indexArray[i];
        }
        util::DYNAMIC_SHAPE dynamic_shape_info(dynamicVertex.data(),dynamicNormal.data(),dynamicUV.data(),dynamicVertex.size(),dynamicNormal.size(),dynamicUV.size());
        mesh = mehManager->loadDynamicIndex(nickName, sizeVertexBuffer, indexArray.get(), _sizeIndexArray,info_draw_mode,dynamic_shape_info);
        if (mesh)
        {
            if (fileNameOldTexture.size())
                mesh->setTexture(0, 0, fileNameOldTexture.c_str(), hasOldAlpha);
            mbm::CUBE *cube = nullptr;
            if (this->mesh->infoPhysics.lsCube.size())
                cube = this->mesh->infoPhysics.lsCube[0];
            else
            {
                cube = new mbm::CUBE();
                this->mesh->infoPhysics.lsCube.push_back(cube);
            }
            cube->halfDim.x   = (vMax.x - vMin.x) * 0.5f;
            cube->halfDim.y   = (vMax.y - vMin.y) * 0.5f;
            cube->halfDim.z   = (vMax.z - vMin.z) * 0.5f;
            const float xAbs1 = fabsf(vMax.x);
            const float xAbs2 = fabsf(vMin.x);
            const float yAbs1 = fabsf(vMax.y);
            const float yAbs2 = fabsf(vMin.y);
            const float zAbs1 = fabsf(vMax.z);
            const float zAbs2 = fabsf(vMin.z);
            if (xAbs1 > xAbs2)
            {
                const float diff = xAbs1 - xAbs2;
                cube->halfDim.x += diff * 0.5f;
            }
            else
            {
                const float diff = xAbs2 - xAbs1;
                cube->halfDim.x += diff * 0.5f;
            }
            if (yAbs1 > yAbs2)
            {
                const float diff = yAbs1 - yAbs2;
                cube->halfDim.y += diff * 0.5f;
            }
            else
            {
                const float diff = yAbs2 - yAbs1;
                cube->halfDim.y += diff * 0.5f;
            }
            if (zAbs1 > zAbs2)
            {
                const float diff = zAbs1 - zAbs2;
                cube->halfDim.z += diff * 0.5f;
            }
            else
            {
                const float diff = zAbs2 - zAbs1;
                cube->halfDim.z += diff * 0.5f;
            }
            if (this->lsAnimation.size() == 0)
            {
                auto anim = new ANIMATION();
                this->lsAnimation.push_back(anim);
                strncpy(anim->nameAnimation, "unic-anim",sizeof(anim->nameAnimation)-1);
                if (!anim->fx.shader.compileShader(nullptr, nullptr))
                    return false;
            }
            this->fileName = nickName;
            this->updateAABB();
            return true;
        }
        return false;
    }

    bool SHAPE_MESH::loadCircle(const char *nickName,float width, float height, bool dynamicBuffer, const int numTriangles)
    {
        if (this->mesh)
            return true;
        if (nickName == nullptr || (width <= 0.0 && height <=0.0) || numTriangles < 4)
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        mesh = mehManager->getIfExists(nickName);
		if(mesh == nullptr)
		{
			/*
			T for triangle
			p for points (index buffer)
                        1p
                8p       |         2p
                 \   8T  |  1T   /
                   \     |     /
                7T   \   |   /     2T
                       \ | /      
          7p -----------0p------------ 3p
                       / | \
                6T   /   |   \      3T
                   /     |     \
                 /   5T  |  4T   \
                6p       |        4p
                        5p

			*/
			//numTriangles = 8;
			const unsigned int numVertex     = numTriangles + 1;
			const unsigned int numIndex      = numTriangles * 3;
			const   float degree    = util::degreeToRadian(360) / numTriangles;
			std::vector<float> vertex(numVertex * 3);
			std::vector<float> normal(numVertex * 3);
			std::vector<float> uv(numVertex * 2);
			std::vector<unsigned short int> lsIndex(numIndex);

			float *pVertex = vertex.data();
			float *pNormal = normal.data();
			float *pUv     = uv.data();
			unsigned short int *pIndex = lsIndex.data();

			if(height <= 0.0f )
				height = width;
			const float ray_width  = width * 0.5f;
			const float ray_height = height * 0.5f;

			pVertex[0] = 0.0f;
			pVertex[1] = 0.0f;
			pVertex[2] = 0.0f;

			pNormal[0] = 0.0f;
			pNormal[1] = 0.0f;
			pNormal[2] = 1.0f;

			for (int i = 0, index = 3; i < numTriangles; ++i, index +=3) 
			{
				pVertex[index]   = std::sin(degree * i) * ray_width;
				pVertex[index+1] = std::cos(degree * i) * ray_height;
				pVertex[index+2] = 0.0f;

				pNormal[index]     = 0.0f;
				pNormal[index + 1] = 0.0f;
				pNormal[index + 2] = 1.0f;
			}

			for (int i = 0, index = 0, index_uv = 0; i <= numTriangles; ++i, index +=3, index_uv += 2) 
			{
				pUv[index_uv]   = (pVertex[index]   + ray_width)  / width;
				pUv[index_uv+1] = 1.0f - ((pVertex[index+1] + ray_height) / height);
			}

			pIndex[0] =0;
			pIndex[1] = numTriangles;//8
			pIndex[2] = 1;//1
            
			//create the index
			for (int i = 1, index = 3; i < numTriangles; ++i, index += 3)
			{
				pIndex[index]   = 0;//0
				pIndex[index+1] = i;//1
				pIndex[index+2] = i+1;//2
			}
			if(dynamicBuffer)
			{
				dynamicVertex = std::move(vertex);
				dynamicNormal = std::move(normal);
				dynamicUV     = std::move(uv);
                dynamicIndex  = lsIndex;
                util::DYNAMIC_SHAPE dynamic_shape_info(dynamicVertex.data(),dynamicNormal.data(),dynamicUV.data(),dynamicVertex.size(),dynamicNormal.size(),dynamicUV.size());
				mesh = mehManager->loadDynamicIndex(nickName,vertex.size(), pIndex, lsIndex.size(),nullptr,dynamic_shape_info);
			}
			else
			{
				mesh = mehManager->loadIndex(nickName, pVertex, pNormal, pUv, vertex.size(), pIndex,lsIndex.size(),nullptr);
			}
		}
        if (mesh)
        {
            mesh->infoPhysics.release();
			mbm::SPHERE* sphere = new mbm::SPHERE();
			sphere->ray = width > height ? width * 0.5f : height * 0.5f;
            mesh->infoPhysics.lsSphere.push_back(sphere);
            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            strncpy(anim->nameAnimation, "circle",sizeof(anim->nameAnimation)-1);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
			setColor(1.0f,0.0f,1.0f,0.7f);//magent
        }
		return mesh != nullptr;
    }

	bool SHAPE_MESH::loadRectangle(const char *nickName,float width, float height,bool dynamicBuffer, int numTriangles)
	{
        if (this->mesh)
            return true;
        if (nickName == nullptr || (width <= 0.0 && height <=0.0) || numTriangles < 2)
            return false;
        if(numTriangles % 2 == 1)
            numTriangles = numTriangles + 1;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
		mesh = mehManager->getIfExists(nickName);
		if(mesh == nullptr)
		{
			/*
			T for triangle
			p for points (index buffer)

            1p              3p              5P              7P              9P
            +---------------+---------------+---------------+---------------+
            |\              |\              |\              |\              |
            |  \      2T    |  \      4T    |  \      6T    |  \      8T    |
            |    \          |    \          |    \          |    \          |
            |      \        |      \        |      \        |      \        |
            |        \      |        \      |        \      |        \      |
            |   1T     \    |   3T     \    |   5T     \    |   7T     \    |
            |            \  |            \  |            \  |            \  |
            |              \|              \|              \|              \|
            +---------------+---------------+---------------+---------------+
            0p              2p               4P              6P             8P

            index = {0,1,2, 2,1,3, 2,3,4, 4,3,5, 4,5,6, 6,5,7}
			*/
            const  int total_vertex = numTriangles + 2;
            std::vector<float> vertex(3 * total_vertex);
            std::vector<float> normal(3 * total_vertex);
            std::vector<float> uv    (2 * total_vertex);

            float * pVertex = vertex.data();
			float * pNormal = normal.data();
			float * pUv = uv.data();
			
			float step = 0.0f;
            if( numTriangles % 2 == 0)
            {
				const float step_div = width / (numTriangles / 2.0f);
                for (int i = 0; i < total_vertex; i+=2, step += step_div)
                {
                    const int index = i * 3;
					const int index_uv = i * 2;
                    pVertex[index    ] = step;
                    pVertex[index + 1] = 0.0f;
                    pVertex[index + 2] = 0.0f;

					pVertex[index + 3] = step;
                    pVertex[index + 4] = height;
                    pVertex[index + 5] = 0.0f;

					pNormal[index    ] = 0.0f;
					pNormal[index + 1] = 0.0f;
					pNormal[index + 2] = 1.0f;
					pNormal[index + 3] = 0.0f;
					pNormal[index + 4] = 0.0f;
					pNormal[index + 5] = 1.0f;

					const float w_u = step > 0.0f ? step / width : 0.0f;
					pUv[index_uv ]    = w_u;// 0
					pUv[index_uv + 1] = 1.0f;// 1
					pUv[index_uv + 2] = w_u;// 0
					pUv[index_uv + 3] = 0.0f;// 0

                }

				pVertex[vertex.size() - 3] = width;
				pVertex[vertex.size() - 6] = width;
            }
			else
			{
				const float step_div = width / ((numTriangles + 1) / 2.0f);
                for (int i = 0; i < (total_vertex - 2); i+=2, step += step_div)
                {
                    const int index = i * 3;
					const int index_uv = i * 2;
                    pVertex[index    ] = step;
                    pVertex[index + 1] = 0.0f;
                    pVertex[index + 2] = 0.0f;

					pVertex[index + 3] = step;
                    pVertex[index + 4] = height;
                    pVertex[index + 5] = 0.0f;

					pNormal[index    ] = 0.0f;
					pNormal[index + 1] = 0.0f;
					pNormal[index + 2] = 1.0f;
					pNormal[index + 3] = 0.0f;
					pNormal[index + 4] = 0.0f;
					pNormal[index + 5] = 1.0f;

					const float w_u = step > 0.0f ? step / width : 0.0f;
					pUv[index_uv ]    = w_u;// 0
					pUv[index_uv + 1] = 1.0f;// 1

					pUv[index_uv + 2] = w_u;// 0
					pUv[index_uv + 3] = 0.0f;// 0

                }

				pVertex[vertex.size() - 1] = 0.0f;
				pVertex[vertex.size() - 2] = 0.0f;
				pVertex[vertex.size() - 3] = width;

				pUv[uv.size() - 1] = 1.0f;
				pUv[uv.size() - 2] = 1.0f;

            }

			//put it in the center
			const float half_width  = width * 0.5f;
			const float half_height = height * 0.5f;
			for (int i = 0; i < total_vertex; i+=2)
			{
				const int index = i * 3;
				pVertex[index    ] -= half_width;
                pVertex[index + 1] -= half_height;
				pVertex[index + 3] -= half_width;
                pVertex[index + 4] -= half_height;
			}
            
			const unsigned int size_index = numTriangles * 3;
			std::vector<unsigned short int> lsIndex(size_index); //{0,1,2, 2,1,3, 2,3,4, 4,3,5, 4,5,6, 6,5,7}
			unsigned short int* pIndex = lsIndex.data();
			bool even = true;
			unsigned short int value_indexed = 0;
			for(unsigned int i=0; i < size_index; i+=3, value_indexed +=1)
			{
				if(even)
				{
					pIndex[i    ] = value_indexed;
					pIndex[i + 1] = value_indexed + 1;
					pIndex[i + 2] = value_indexed + 2;
				}
				else //odd
				{
					pIndex[i    ] = value_indexed + 1;
					pIndex[i + 1] = value_indexed;
					pIndex[i + 2] = value_indexed + 2;
				}
				even = !even;
			}
			if(dynamicBuffer)
			{
				dynamicVertex = std::move(vertex);
				dynamicNormal = std::move(normal);
				dynamicUV     = std::move(uv);
                dynamicIndex  = lsIndex;
                util::DYNAMIC_SHAPE dynamic_shape_info(dynamicVertex.data(),dynamicNormal.data(),dynamicUV.data(),dynamicVertex.size(),dynamicNormal.size(),dynamicUV.size());
				mesh = mehManager->loadDynamicIndex(nickName,3 * total_vertex, pIndex, size_index,nullptr,dynamic_shape_info);

			}
			else
			{
				mesh = mehManager->loadIndex(nickName, pVertex, pNormal, pUv, 3 * total_vertex, pIndex, size_index,nullptr);
			}
		}
        if (mesh)
        {
            mesh->infoPhysics.release();
			mbm::CUBE* cube = new mbm::CUBE(width,height,0.0f);
            mesh->infoPhysics.lsCube.push_back(cube);
            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            strncpy(anim->nameAnimation, "rectangle",sizeof(anim->nameAnimation)-1);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
			setColor(1.0f,0.0f,1.0f,0.7f);//magent
        }
		return mesh != nullptr;
    }

    bool SHAPE_MESH::loadTriangle(const char *nickName,float points[6], bool dynamicBuffer)
    {
        if (this->mesh)
            return true;
        if (nickName == nullptr)
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        mesh = mehManager->getIfExists(nickName);
		if(mesh == nullptr)
		{
			constexpr int numTriangles = 1;
			constexpr int numVertex   = 2 + numTriangles;
			constexpr int size_vertex = numVertex * 3;
			constexpr int size_index  = 3 + ((numTriangles -1) * 3);
			constexpr int size_uv = numVertex * 2;

			std::vector<float> vertex(size_vertex);
			std::vector<float> normal(size_vertex);
			std::vector<float> uv(size_uv);
			std::vector<unsigned short int> lsIndex(size_index);


			float * pVertex = vertex.data();
			float * pNormal = normal.data();
			float * pUv = uv.data();
			unsigned short int* pIndex = lsIndex.data();

			pVertex[0] = points[0];
			pVertex[1] = points[1];
			pVertex[2] = 0;

			pVertex[3] = points[2];
			pVertex[4] = points[3];
			pVertex[5] = 0;

			pVertex[size_vertex - 3] = points[4];
			pVertex[size_vertex - 2] = points[5];
			pVertex[size_vertex - 1] = 0;

			pIndex[0] = 0;
			pIndex[1] = 1;
			pIndex[2] = 2;
			

			for(int index_uv = 0; index_uv < size_uv; index_uv+=2)
			{
				pUv[index_uv    ] = 1;
				pUv[index_uv + 1] = 0;
			}
			
			for (int i = 0; i < size_vertex; i+=3)
			{
				pNormal[i    ] = 0.0f;
				pNormal[i + 1] = 0.0f;
				pNormal[i + 2] = 1.0f;
			}

			if(dynamicBuffer)
			{
				dynamicVertex = std::move(vertex);
				dynamicNormal = std::move(normal);
				dynamicUV     = std::move(uv);
                dynamicIndex  = lsIndex;
                util::DYNAMIC_SHAPE dynamic_shape_info(dynamicVertex.data(),dynamicNormal.data(),dynamicUV.data(),dynamicVertex.size(),dynamicNormal.size(),dynamicUV.size());
				mesh = mehManager->loadDynamicIndex(nickName,size_vertex, pIndex, size_index,nullptr,dynamic_shape_info);
			}
			else
			{
				mesh = mehManager->loadIndex(nickName, pVertex, pNormal, pUv, size_vertex, pIndex, size_index,nullptr);
			}
		}
        if (mesh)
        {
            mesh->infoPhysics.release();
			mbm::TRIANGLE* triangle = new mbm::TRIANGLE();

            triangle->point[0].x = points[0];
			triangle->point[0].y = points[1];
			triangle->point[0].z = 0;

			triangle->point[1].x = points[2];
			triangle->point[1].y = points[3];
			triangle->point[1].z = 0;

			triangle->point[2].x = points[4];
			triangle->point[2].y = points[5];
			triangle->point[2].z = 0;
            
            mesh->infoPhysics.lsTriangle.push_back(triangle);
            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            strncpy(anim->nameAnimation, "triangle",sizeof(anim->nameAnimation)-1);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
			setColor(1.0f,0.0f,1.0f,0.7f);//magent
        }
		return mesh != nullptr;
    }

	bool SHAPE_MESH::loadTriangle(const char *nickName,float width, float height,bool dynamicBuffer, const int numTriangles)
	{
        if (this->mesh)
            return true;
        if (nickName == nullptr || (width <= 0.0 && height <=0.0) || numTriangles < 1 )
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        mesh = mehManager->getIfExists(nickName);
		if(mesh == nullptr)
		{
			/*
			T for triangle
			p for points (index buffer)


                    1p
                    /\
                   /  \
                  /    \
                 /      \
                /        \
               /    1T    \
              /            \
             /              \
            /                \
           /+----------------+\
            0p              2p
			*/
			const int numVertex   = 2 + numTriangles;
			const int size_vertex = numVertex * 3;
			const int size_index  = 3 + ((numTriangles -1) * 3);
			const int size_uv = numVertex * 2;

			std::vector<float> vertex(size_vertex);
			std::vector<float> normal(size_vertex);
			std::vector<float> uv(size_uv);
			std::vector<unsigned short int> lsIndex(size_index);


			if(width <= 0.0f )
				width = 1;
			if(height <= 0.0f )
				height = width;
			const float x  = width * 0.5f;
			const float y  = height * 0.5f;

			float * pVertex = vertex.data();
			float * pNormal = normal.data();
			float * pUv = uv.data();
			unsigned short int* pIndex = lsIndex.data();

			pVertex[0] = -x;
			pVertex[1] = -y;
			pVertex[2] = 0;

			pVertex[3] = 0;
			pVertex[4] = y;
			pVertex[5] = 0;

			pVertex[size_vertex - 3] = x;
			pVertex[size_vertex - 2] = -y;
			pVertex[size_vertex - 1] = 0;

			pIndex[0] = 0;
			pIndex[1] = 1;
			pIndex[2] = 2;
			

			if(numTriangles > 1)
			{
				VEC2 p0(0,y);
				VEC2 direction(x,-y);

				direction -= p0;
				const float dist = direction.length() / (numTriangles);
				vec2Normalize(&direction,&direction);
				
				for(int i=1; i <numTriangles; i++)
				{
					const int index_vertex = 6 + ((i-1) * 3);
					
					p0.x += (direction.x * dist);
					p0.y += (direction.y * dist);

					pVertex[index_vertex    ] = p0.x;
					pVertex[index_vertex + 1] = p0.y;
				}

				pIndex [2] = 3;

				for(int index = 3,based_index = 3; index < size_index; index += 3, based_index++)
				{
					pIndex [index    ] = 0;
					pIndex [index + 1] = based_index;
					pIndex [index + 2] = based_index + 1;
				}
				pIndex [size_index - 1] = 2;
			}

			const float half_width  = width / 2.0f;
			const float half_height = height / 2.0f;

			for(int index_uv = 0, index_vertex = 0; index_uv < size_uv; index_uv+=2, index_vertex+=3)
			{
				const float px = pVertex[index_vertex    ] + half_width;
				const float py = pVertex[index_vertex + 1] + half_height;
				pUv[index_uv    ] = px / width;
				pUv[index_uv + 1] = 1.0f - (py / height);
			}
			
			for (int i = 0; i < size_vertex; i+=3)
			{
				pNormal[i    ] = 0.0f;
				pNormal[i + 1] = 0.0f;
				pNormal[i + 2] = 1.0f;
			}

			if(dynamicBuffer)
			{
				dynamicVertex = std::move(vertex);
				dynamicNormal = std::move(normal);
				dynamicUV     = std::move(uv);
                dynamicIndex  = lsIndex;
                util::DYNAMIC_SHAPE dynamic_shape_info(dynamicVertex.data(),dynamicNormal.data(),dynamicUV.data(),dynamicVertex.size(),dynamicNormal.size(),dynamicUV.size());
				mesh = mehManager->loadDynamicIndex(nickName,size_vertex, pIndex, size_index,nullptr,dynamic_shape_info);
			}
			else
			{
				mesh = mehManager->loadIndex(nickName, pVertex, pNormal, pUv, size_vertex, pIndex, size_index,nullptr);
			}
		}
        if (mesh)
        {
            mesh->infoPhysics.release();
			mbm::TRIANGLE* triangle = new mbm::TRIANGLE();

            if(width <= 0.0f )
				width = 1;
			if(height <= 0.0f )
				height = width;

            const float x  = width * 0.5f;
			const float y  = height * 0.5f;

			triangle->point[0].x = -x;
			triangle->point[0].y = -y;
			triangle->point[0].z = 0;

			triangle->point[1].x = 0;
			triangle->point[1].y = y;
			triangle->point[1].z = 0;

			triangle->point[2].x = x;
			triangle->point[2].y = -y;
			triangle->point[2].z = 0;
            
            mesh->infoPhysics.lsTriangle.push_back(triangle);
            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            strncpy(anim->nameAnimation, "triangle",sizeof(anim->nameAnimation)-1);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
			setColor(1.0f,0.0f,1.0f,0.7f);//magent
        }
		return mesh != nullptr;
    }
    
    bool SHAPE_MESH::load(const char *nickName, mbm::AUTO_VERTEX *autoVertex,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        if (this->mesh)
            return true;
        if (autoVertex == nullptr || autoVertex->ls_xyz == nullptr || autoVertex->sizeArray == 0)
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        unsigned int  sizeVertexBuffer = 0;
        if (this->is3D)
            sizeVertexBuffer = autoVertex->sizeArray;
        else
            sizeVertexBuffer = (autoVertex->sizeArray / 2) * 3;
        VEC3        vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        VEC3        vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        AUTO_VERTEX vertex;
        if (this->is3D)
        {
            const unsigned sizeUv = (sizeVertexBuffer / 3) * 2;
            vertex.ls_xyz         = new float[sizeVertexBuffer];
            vertex.ls_normal      = new float[sizeVertexBuffer];
            vertex.ls_uv          = new float[sizeUv];
            for (unsigned int p = 0, uv = 0; p < sizeVertexBuffer; p += 3, uv += 2)
            {
                vertex.ls_xyz[p]     = autoVertex->ls_xyz[p];
                vertex.ls_xyz[p + 1] = autoVertex->ls_xyz[p + 1];
                vertex.ls_xyz[p + 2] = autoVertex->ls_xyz[p + 2];
                if (vertex.ls_xyz[p] > vMax.x)
                    vMax.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] > vMax.y)
                    vMax.y = vertex.ls_xyz[p + 1];
                if (vertex.ls_xyz[p + 2] > vMax.z)
                    vMax.z = vertex.ls_xyz[p + 2];

                if (vertex.ls_xyz[p] < vMin.x)
                    vMin.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] < vMin.y)
                    vMin.y = vertex.ls_xyz[p + 1];
                if (vertex.ls_xyz[p + 2] < vMin.z)
                    vMin.z = vertex.ls_xyz[p + 2];
                if (autoVertex->ls_uv)
                {
                    vertex.ls_uv[uv]     = autoVertex->ls_uv[uv];
                    vertex.ls_uv[uv + 1] = autoVertex->ls_uv[uv + 1];
                }
                if (autoVertex->ls_normal)
                {
                    vertex.ls_normal[p]     = autoVertex->ls_normal[p];
                    vertex.ls_normal[p + 1] = autoVertex->ls_normal[p + 1];
                    vertex.ls_normal[p + 2] = autoVertex->ls_normal[p + 2];
                }
            }
            if (autoVertex->ls_normal == nullptr)
            {
                // recalcula as normais
                for (unsigned int i = 0; (i + 8) < sizeVertexBuffer; i += 9)
                {
                    const unsigned int index0 = i;
                    const unsigned int index1 = i + 3;
                    const unsigned int index2 = i + 6;
                    VEC3   a, b, n;
                    a.x = vertex.ls_xyz[index1] - vertex.ls_xyz[index0];
                    a.y = vertex.ls_xyz[index1 + 1] - vertex.ls_xyz[index0 + 1];
                    a.z = vertex.ls_xyz[index1 + 2] - vertex.ls_xyz[index0 + 2];

                    b.x = vertex.ls_xyz[index2] - vertex.ls_xyz[index0];
                    b.y = vertex.ls_xyz[index2 + 1] - vertex.ls_xyz[index0 + 1];
                    b.z = vertex.ls_xyz[index2 + 2] - vertex.ls_xyz[index0 + 2];

                    vec3Cross(&n, &a, &b);
                    vec3Normalize(&n, &n);

                    vertex.ls_normal[index0]     = n.x;
                    vertex.ls_normal[index0 + 1] = n.y;
                    vertex.ls_normal[index0 + 2] = n.z;

                    vertex.ls_normal[index1]     = n.x;
                    vertex.ls_normal[index1 + 1] = n.y;
                    vertex.ls_normal[index1 + 2] = n.z;

                    vertex.ls_normal[index2]     = n.x;
                    vertex.ls_normal[index2 + 1] = n.y;
                    vertex.ls_normal[index2 + 2] = n.z;
                }
            }
        }
        else
        {
            vMax.z                = 0;
            vMin.z                = 0;
            const unsigned sizeUv = (sizeVertexBuffer / 3) * 2;
            vertex.ls_xyz         = new float[sizeVertexBuffer];
            vertex.ls_normal      = new float[sizeVertexBuffer];
            vertex.ls_uv          = new float[sizeUv];
            for (unsigned int p = 0, j = 0, uv = 0, s = 0; s < sizeVertexBuffer; p += 3, j += 2, uv += 2, s += 3)
            {
                vertex.ls_xyz[p]     = autoVertex->ls_xyz[j];
                vertex.ls_xyz[p + 1] = autoVertex->ls_xyz[j + 1];
                vertex.ls_xyz[p + 2] = 0;
                if (vertex.ls_xyz[p] > vMax.x)
                    vMax.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] > vMax.y)
                    vMax.y = vertex.ls_xyz[p + 1];

                if (vertex.ls_xyz[p] < vMin.x)
                    vMin.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] < vMin.y)
                    vMin.y = vertex.ls_xyz[p + 1];
                if (autoVertex->ls_uv)
                {
                    vertex.ls_uv[uv]     = autoVertex->ls_uv[uv];
                    vertex.ls_uv[uv + 1] = autoVertex->ls_uv[uv + 1];
                }
                vertex.ls_normal[p]     = 0;
                vertex.ls_normal[p + 1] = 0;
                vertex.ls_normal[p + 2] = -1;
            }
        }
        if (autoVertex->ls_uv == nullptr)
        {
            const float width  = vMax.x - vMin.x;
            const float height = vMax.y - vMin.y;
            for (unsigned int uv = 0, p = 0; p < sizeVertexBuffer; p += 3, uv += 2)
            {
                vertex.ls_uv[uv]     = (vertex.ls_xyz[p] - vMin.x) / width;
                vertex.ls_uv[uv + 1] = 1.0f - ((vertex.ls_xyz[p + 1] - vMin.y) / height);
            }
        }
        mesh = mehManager->load(nickName, vertex.ls_xyz, vertex.ls_normal, vertex.ls_uv, sizeVertexBuffer,info_draw_mode);
        if (mesh)
        {
            this->position += mesh->positionOffset;
            this->angle = mesh->angleDefault;
            this->mesh->infoPhysics.release();
			auto cube   = new mbm::CUBE();
            cube->halfDim.x   = (vMax.x - vMin.x) * 0.5f;
            cube->halfDim.y   = (vMax.y - vMin.y) * 0.5f;
            cube->halfDim.z   = (vMax.z - vMin.z) * 0.5f;
            const float xAbs1 = fabsf(vMax.x);
            const float xAbs2 = fabsf(vMin.x);
            const float yAbs1 = fabsf(vMax.y);
            const float yAbs2 = fabsf(vMin.y);
            const float zAbs1 = fabsf(vMax.z);
            const float zAbs2 = fabsf(vMin.z);
            if (xAbs1 > xAbs2)
            {
                const float diff = xAbs1 - xAbs2;
                cube->halfDim.x += diff * 0.5f;
            }
            else
            {
                const float diff = xAbs2 - xAbs1;
                cube->halfDim.x += diff * 0.5f;
            }
            if (yAbs1 > yAbs2)
            {
                const float diff = yAbs1 - yAbs2;
                cube->halfDim.y += diff * 0.5f;
            }
            else
            {
                const float diff = yAbs2 - yAbs1;
                cube->halfDim.y += diff * 0.5f;
            }
            if (zAbs1 > zAbs2)
            {
                const float diff = zAbs1 - zAbs2;
                cube->halfDim.z += diff * 0.5f;
            }
            else
            {
                const float diff = zAbs2 - zAbs1;
                cube->halfDim.z += diff * 0.5f;
            }

            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            this->mesh->infoPhysics.lsCube.push_back(cube);
            strncpy(anim->nameAnimation, "unic-anim",sizeof(anim->nameAnimation)-1);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
            return true;
        }
        return false;
    }
    
    bool SHAPE_MESH::loadIndexed(const char *nickName, mbm::AUTO_VERTEX *autoVertex,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        if (this->mesh)
            return true;
        if (autoVertex->ls_xyz == nullptr || autoVertex->sizeArray == 0 || autoVertex->sizeIndex == 0 ||
            autoVertex->ls_index == nullptr)
            return false;
        MESH_MANAGER *mehManager       = MESH_MANAGER::getInstance();
        unsigned int  sizeVertexBuffer = 0;
        if (this->is3D)
            sizeVertexBuffer = autoVertex->sizeArray;
        else
            sizeVertexBuffer = (autoVertex->sizeArray / 2) * 3;
        VEC3                vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        VEC3                vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        AUTO_VERTEX         vertex;
        unsigned short int *index = autoVertex->ls_index;
        if (this->is3D)
        {
            const unsigned sizeUv = (sizeVertexBuffer / 3) * 2;
            vertex.ls_xyz         = new float[sizeVertexBuffer];
            vertex.ls_normal      = new float[sizeVertexBuffer];
            vertex.ls_uv          = new float[sizeUv];
            for (unsigned int p = 0, uv = 0; p < sizeVertexBuffer; p += 3, uv += 2)
            {
                vertex.ls_xyz[p]     = autoVertex->ls_xyz[p];
                vertex.ls_xyz[p + 1] = autoVertex->ls_xyz[p + 1];
                vertex.ls_xyz[p + 2] = autoVertex->ls_xyz[p + 2];
                if (vertex.ls_xyz[p] > vMax.x)
                    vMax.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] > vMax.y)
                    vMax.y = vertex.ls_xyz[p + 1];
                if (vertex.ls_xyz[p + 2] > vMax.z)
                    vMax.z = vertex.ls_xyz[p + 2];

                if (vertex.ls_xyz[p] < vMin.x)
                    vMin.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] < vMin.y)
                    vMin.y = vertex.ls_xyz[p + 1];
                if (vertex.ls_xyz[p + 2] < vMin.z)
                    vMin.z = vertex.ls_xyz[p + 2];
                if (autoVertex->ls_uv)
                {
                    vertex.ls_uv[uv]     = autoVertex->ls_uv[uv];
                    vertex.ls_uv[uv + 1] = autoVertex->ls_uv[uv + 1];
                }
                if (autoVertex->ls_normal)
                {
                    vertex.ls_normal[p]     = autoVertex->ls_normal[p];
                    vertex.ls_normal[p + 1] = autoVertex->ls_normal[p + 1];
                    vertex.ls_normal[p + 2] = autoVertex->ls_normal[p + 2];
                }
            }
            if (autoVertex->ls_normal == nullptr)
            {
                // recalcula as normais
                for (unsigned int i = 0; (i + 8) < sizeVertexBuffer; i += 9)
                {
                    const unsigned int index0 = i;
                    const unsigned int index1 = i + 3;
                    const unsigned int index2 = i + 6;
                    VEC3   a, b, n;
                    a.x = vertex.ls_xyz[index[index1]] - vertex.ls_xyz[index[index0]];
                    a.y = vertex.ls_xyz[index[index1 + 1]] - vertex.ls_xyz[index[index0 + 1]];
                    a.z = vertex.ls_xyz[index[index1 + 2]] - vertex.ls_xyz[index[index0 + 2]];

                    b.x = vertex.ls_xyz[index[index2]] - vertex.ls_xyz[index[index0]];
                    b.y = vertex.ls_xyz[index[index2 + 1]] - vertex.ls_xyz[index[index0 + 1]];
                    b.z = vertex.ls_xyz[index[index2 + 2]] - vertex.ls_xyz[index[index0 + 2]];

                    vec3Cross(&n, &a, &b);
                    vec3Normalize(&n, &n);

                    vertex.ls_normal[index[index0]]     = n.x;
                    vertex.ls_normal[index[index0 + 1]] = n.y;
                    vertex.ls_normal[index[index0 + 2]] = n.z;

                    vertex.ls_normal[index[index1]]     = n.x;
                    vertex.ls_normal[index[index1 + 1]] = n.y;
                    vertex.ls_normal[index[index1 + 2]] = n.z;

                    vertex.ls_normal[index[index2]]     = n.x;
                    vertex.ls_normal[index[index2 + 1]] = n.y;
                    vertex.ls_normal[index[index2 + 2]] = n.z;
                }
            }
        }
        else
        {
            vMax.z                = 0;
            vMin.z                = 0;
            const unsigned sizeUv = (sizeVertexBuffer / 3) * 2;
            vertex.ls_xyz         = new float[sizeVertexBuffer];
            vertex.ls_normal      = new float[sizeVertexBuffer];
            vertex.ls_uv          = new float[sizeUv];
            for (unsigned int p = 0, j = 0, uv = 0, s = 0; s < sizeVertexBuffer; p += 3, j += 2, uv += 2, s += 3)
            {
                vertex.ls_xyz[p]     = autoVertex->ls_xyz[j];
                vertex.ls_xyz[p + 1] = autoVertex->ls_xyz[j + 1];
                vertex.ls_xyz[p + 2] = 0;
                if (vertex.ls_xyz[p] > vMax.x)
                    vMax.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] > vMax.y)
                    vMax.y = vertex.ls_xyz[p + 1];

                if (vertex.ls_xyz[p] < vMin.x)
                    vMin.x = vertex.ls_xyz[p];
                if (vertex.ls_xyz[p + 1] < vMin.y)
                    vMin.y = vertex.ls_xyz[p + 1];
                if (autoVertex->ls_uv)
                {
                    vertex.ls_uv[uv]     = autoVertex->ls_uv[uv];
                    vertex.ls_uv[uv + 1] = autoVertex->ls_uv[uv + 1];
                }
                vertex.ls_normal[p]     = 0;
                vertex.ls_normal[p + 1] = 0;
                vertex.ls_normal[p + 2] = -1;
            }
        }
        if (autoVertex->ls_uv == nullptr)
        {
            const float width  = vMax.x - vMin.x;
            const float height = vMax.y - vMin.y;
            for (unsigned int uv = 0, p = 0; p < sizeVertexBuffer; p += 3, uv += 2)
            {
                vertex.ls_uv[uv]     = (vertex.ls_xyz[p] - vMin.x) / width;
                vertex.ls_uv[uv + 1] = 1.0f - ((vertex.ls_xyz[p + 1] - vMin.y) / height);
            }
        }
        mesh = mehManager->loadIndex(nickName, vertex.ls_xyz, vertex.ls_normal, vertex.ls_uv, sizeVertexBuffer, index,autoVertex->sizeIndex,info_draw_mode);
        if (mesh)
        {
            this->position += mesh->positionOffset;
            this->angle = mesh->angleDefault;
            this->mesh->infoPhysics.release();
			auto cube   = new mbm::CUBE();
            cube->halfDim.x   = (vMax.x - vMin.x) * 0.5f;
            cube->halfDim.y   = (vMax.y - vMin.y) * 0.5f;
            cube->halfDim.z   = (vMax.z - vMin.z) * 0.5f;
            const float xAbs1 = fabsf(vMax.x);
            const float xAbs2 = fabsf(vMin.x);
            const float yAbs1 = fabsf(vMax.y);
            const float yAbs2 = fabsf(vMin.y);
            const float zAbs1 = fabsf(vMax.z);
            const float zAbs2 = fabsf(vMin.z);
            if (xAbs1 > xAbs2)
            {
                const float diff = xAbs1 - xAbs2;
                cube->halfDim.x += diff * 0.5f;
            }
            else
            {
                const float diff = xAbs2 - xAbs1;
                cube->halfDim.x += diff * 0.5f;
            }
            if (yAbs1 > yAbs2)
            {
                const float diff = yAbs1 - yAbs2;
                cube->halfDim.y += diff * 0.5f;
            }
            else
            {
                const float diff = yAbs2 - yAbs1;
                cube->halfDim.y += diff * 0.5f;
            }
            if (zAbs1 > zAbs2)
            {
                const float diff = zAbs1 - zAbs2;
                cube->halfDim.z += diff * 0.5f;
            }
            else
            {
                const float diff = zAbs2 - zAbs1;
                cube->halfDim.z += diff * 0.5f;
            }
            auto anim = new ANIMATION();
            this->lsAnimation.push_back(anim);
            strncpy(anim->nameAnimation, "unic-anim",sizeof(anim->nameAnimation)-1);
            this->mesh->infoPhysics.lsCube.push_back(cube);
            if (!anim->fx.shader.compileShader(nullptr, nullptr))
                return false;
            this->fileName = nickName;
            this->updateAABB();
            return true;
        }
        return false;
    }

	bool SHAPE_MESH::setColor(const float r,const float g, const float b, const float a)
	{
		char color_hex[32] = {};
		const mbm::COLOR c(r,g,b,a);
		mbm::COLOR::getStringHexColorFromColor(c,color_hex,sizeof(color_hex));
		return this->setTexture(mesh, color_hex, 0, true);
	}
    
    const char * SHAPE_MESH::getFileName() const
    {
        if (this->mesh)
            return this->mesh->getFilenameMesh();
        return nullptr;
    }
    
    bool SHAPE_MESH::isOnFrustum()
    {
        if (this->mesh && this->mesh->isLoaded())
        {
            IS_ON_FRUSTUM verify(this);
            const bool is_on_frustum = verify.isOnFrustum(this->is3D, this->is2dS);
            if(is_on_frustum)
            {
                if (this->dynamicVertex.size() > 0)
                {
                    if(onRenderDynamicBuffer)
                    {
                        const auto last_size_vertex = dynamicVertex.size();
                        const auto last_size_normal = dynamicNormal.size();
                        const auto last_size_uv     = dynamicUV.size();
                        onRenderDynamicBuffer(this, dynamicVertex,dynamicNormal,dynamicUV,dynamicIndex);
                        if(dynamicVertex.size() != last_size_vertex)
                        {
                            ERROR_LOG("Size of dynamic vertex changed detected. Aborting...");
                            ERROR_LOG("Resizing the vertex is not allowed.");
                            mesh = nullptr;
                            return false;
                        }
                        if(dynamicNormal.size() != last_size_normal)
                        {
                            ERROR_LOG("Size of dynamic normal changed detected. Aborting...");
                            ERROR_LOG("Resizing the normal is not allowed.");
                            mesh = nullptr;
                            return false;
                        }
                        if(dynamicUV.size() != last_size_uv)
                        {
                            ERROR_LOG("Size of dynamic uv changed detected. Aborting...");
                            ERROR_LOG("Resizing the uv is not allowed.");
                            mesh = nullptr;
                            return false;
                        }
                    }
                }
            }
            return is_on_frustum;
        }
        return false;
    }
    
    bool SHAPE_MESH::render()
    {
        if (this->mesh && this->indexCurrentAnimation < this->lsAnimation.size())
        {
            if(this->alwaysRenderize && this->dynamicVertex.size() > 0 && onRenderDynamicBuffer)
            {
                const auto last_size_vertex = dynamicVertex.size();
                const auto last_size_normal = dynamicNormal.size();
                const auto last_size_uv     = dynamicUV.size();
                onRenderDynamicBuffer(this, dynamicVertex,dynamicNormal,dynamicUV,dynamicIndex);
                if(dynamicVertex.size() != last_size_vertex)
                {
                    ERROR_LOG("Size of dynamic vertex changed detected. Aborting...");
                    ERROR_LOG("Resizing the vertex is not allowed.");
                    mesh = nullptr;
                    return false;
                }
                if(dynamicNormal.size() != last_size_normal)
                {
                    ERROR_LOG("Size of dynamic normal changed detected. Aborting...");
                    ERROR_LOG("Resizing the normal is not allowed.");
                    mesh = nullptr;
                    return false;
                }
                if(dynamicUV.size() != last_size_uv)
                {
                    ERROR_LOG("Size of dynamic uv changed detected. Aborting...");
                    ERROR_LOG("Resizing the uv is not allowed.");
                    mesh = nullptr;
                    return false;
                }
            }
            ANIMATION *animation = this->lsAnimation[this->indexCurrentAnimation];
            animation->updateAnimation(this->device->delta, this, this->onEndAnimation,this->onEndFx);
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * this->device->camera.scaleScreen2d.x,
                                    this->position.y * this->device->camera.scaleScreen2d.y, this->position.z);
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            this->blend.set(animation->blendState);
            if (this->dynamicVertex.size() > 0)
            {
                animation->fx.shader.update();
                animation->fx.setBlendOp();
                if (animation->fx.textureOverrideStage2)
                {
                    if (!mesh->renderDynamic(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,
                                                reinterpret_cast<VEC3 *>(this->dynamicVertex.data()), 
                                                reinterpret_cast<VEC3 *>(this->dynamicNormal.data()),
                                                reinterpret_cast<VEC2 *>(this->dynamicUV.data()),
                                                animation->fx.textureOverrideStage2->idTexture))
                        return false;
                }
                else
                {
                    if (!mesh->renderDynamic(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,
                                                reinterpret_cast<VEC3 *>(this->dynamicVertex.data()), 
                                                reinterpret_cast<VEC3 *>(this->dynamicNormal.data()),
                                                reinterpret_cast<VEC2 *>(this->dynamicUV.data()),0))
                        return false;
                }
            }
            else
            {
                animation->fx.shader.update();
                animation->fx.setBlendOp();
                if (animation->fx.textureOverrideStage2)
                {
                    if (!mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,animation->fx.textureOverrideStage2->idTexture))
                        return false;
                }
                else
                {
                    if (!mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,0))
                        return false;
                }
            }
            return true;
        }
        return false;
    }

	void SHAPE_MESH::onStop()
    {
        this->releaseAnimation();
        this->mesh = nullptr;
    }
    
    bool SHAPE_MESH::onRestoreDevice() noexcept
    {
        return false;
    }
    
    const mbm::INFO_PHYSICS * SHAPE_MESH::getInfoPhysics() const  noexcept
    {
        if (this->mesh)
            return &this->mesh->infoPhysics;
        return nullptr;
    }
    
    const MESH_MBM * SHAPE_MESH::getMesh() const  noexcept
    {
        return this->mesh;
    }

	FX*  SHAPE_MESH::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  SHAPE_MESH::getAnimationManager() noexcept
	{
		return this;
	}
    
    bool SHAPE_MESH::isLoaded() const noexcept
    {
        return this->mesh != nullptr;
    }
	
}
