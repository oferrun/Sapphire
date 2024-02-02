#include <stdint.h>
#include "Errors.hpp"
#include "FileWrapper.hpp"
#include "DataBlobImpl.hpp"
#include "GrimrockModelLoader.h"
#include "TextureUtilities.h"
#include "../../../DiligentCore/Common/interface/BasicMath.hpp"

using namespace Diligent;


namespace Grimrock
{
	// A String is a variable length 8-bit string
	struct String
	{
		int32_t		length;
		char		data[64];
	};

	// A FourCC is a four character code string used for headers
	struct FourCC
	{
		uint8_t    data[4];
	};

	struct Vec3
	{
		float x, y, z;
	};

	// A Mat4x3 is a transformation matrix split into 3x3 rotation and 3D rotation parts
	struct Mat4x3
	{
		Vec3    baseX;
		Vec3    baseY;
		Vec3    baseZ;
		Vec3    translation;
	};

	// A Quaternion represents a rotation in 3D space
	struct Quaternion
	{
		float x, y, z, w;
	};

	struct Bone
	{
		int32_t		boneNodeIndex;    // index of the node used to deform the object
		Mat4x3		invRestMatrix;   // transform from model space to bone space
	};

	/*
		VERTEX_ARRAY_POSITION = 0
		VERTEX_ARRAY_NORMAL = 1
		VERTEX_ARRAY_TANGENT = 2
		VERTEX_ARRAY_BITANGENT = 3    
		VERTEX_ARRAY_COLOR = 4
		VERTEX_ARRAY_TEXCOORD0 = 5
		VERTEX_ARRAY_TEXCOORD1 = 6
		VERTEX_ARRAY_TEXCOORD2 = 7
		VERTEX_ARRAY_TEXCOORD3 = 8
		VERTEX_ARRAY_TEXCOORD4 = 9
		VERTEX_ARRAY_TEXCOORD5 = 10
		VERTEX_ARRAY_TEXCOORD6 = 11
		VERTEX_ARRAY_TEXCOORD7 = 12
		VERTEX_ARRAY_BONE_INDEX = 13
		VERTEX_ARRAY_BONE_WEIGHT = 14
	*/

	struct VertexArray
	{
		int32_t dataType;   // 0 = byte, 1 = int16, 2 = int32, 3 = float32
		int32_t dim;        // dimensions of the data type (2-4)
		int32_t stride;     // byte offset from vertex to vertex
		//uint8_t  rawVertexData[numVertices * stride];
	};

	struct MeshSegment
	{
		String material;      // name of the material defined in Lua script
		int32_t  primitiveType; // always two
		int32_t  firstIndex;    // starting location in the index list
		int32_t  count;         // number of triangles
	};

	struct MeshData
	{
		FourCC			magic;          // "MESH"
		int32_t			version;        // must be two
		int32_t			numVertices;    // number of vertices following
		VertexArray		vertexArrays[15];
		int32_t			numIndices;     // number of triangle indices following
		//int32_t			indices[numIndices];
		int32_t			numSegments;    // number of mesh segments following
		//MeshSegment		segments[numSegments];
		Vec3			boundCenter;    // center of the bound sphere in model space
		float			boundRadius;    // radius of the bound sphere in model space
		Vec3			boundMin;       // minimum extents of the bound box in model space
		Vec3			boundMax;       // maximum extents of the bound box in model space
	};

	struct MeshEntity
	{
		MeshData	meshdata;
		int32_t		numBones;         // number of bones for skeletal animation
		//Bone		bones[numBones];
		Vec3		emissiveColor;    // deprecated, should be set to 0,0,0
		uint8_t     castShadow;       // 0 = shadow casting off, 1 = shadow casting on
	};

	struct Node
	{
		String		name;
		Mat4x3		localToParent;
		int32_t		parent;        // index of the parent node or -1 if no parent
		int32_t		type;          // -1 = no entity data, 0 = MeshEntity follows
		//(MeshEntity)           // not present if type is -1    
	};

	struct ModelFile
	{
		FourCC		magic;           // "MDL1"
		int32_t		version;         // always two
		int32_t		numNodes;        // number of nodes following
		//Node		nones[0];
	};

	FourCC readFourCC(const uint8_t*& ptr) {
		FourCC fourcc;
		std::memcpy(fourcc.data, ptr, 4);
		ptr += 4; // Move pointer forward

		return fourcc;
	}

	String readString(const uint8_t*& ptr) {
		uint32_t length = *(reinterpret_cast<const uint32_t*>(ptr));
		ptr += sizeof(uint32_t); // Move pointer forward

		String str;
		
		
		str.length = length;
		uint32_t maxlength = std::min(length, (uint32_t)sizeof(str.data) - 1);
		memcpy(str.data, ptr, maxlength);
		str.data[maxlength] = '\0';
		ptr += length; // Move pointer forward

		return str;
	}

	Vec3 readVec3(const uint8_t*& ptr) {
		Vec3 vec;
		vec.x = *(reinterpret_cast<const float*>(ptr));
		ptr += sizeof(float);
		vec.y = *(reinterpret_cast<const float*>(ptr));
		ptr += sizeof(float);
		vec.z = *(reinterpret_cast<const float*>(ptr));
		ptr += sizeof(float);

		return vec;
	}

	Mat4x3 readMatrix4x3(const uint8_t*& ptr) {
		Mat4x3 mat;

		mat.baseX = readVec3(ptr);
		mat.baseY = readVec3(ptr);
		mat.baseZ = readVec3(ptr);
		mat.translation = readVec3(ptr);
		return mat;
	}

	void readVertexArray(int32_t numVertices, int32_t index, Sapphire::SapphireMeshLoadData* meshLoadData , const uint8_t*& ptr)
	{
		// 0 = byte, 1 = int16, 2 = int32, 3 = float32
		//int32_t dataType = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		// dimensions of the data type (2-4)
		//int32_t dim = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		// byte offset from vertex to vertex
		int32_t stride = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		
		int32_t sizeVertexData = numVertices * stride;
		if (sizeVertexData > 0)
		{
			if (index == 0)
			{
				meshLoadData->m_verticesPositionPtr = ptr;
			}
			else if (index == 1)
			{
				meshLoadData->m_verticesNormalsPtr = ptr;
			}
			else if (index == 5)
			{
				meshLoadData->m_verticesUVPtr = ptr;
			}			
			
			ptr += sizeVertexData;
		}

		
	}


	void readMeshSegment(int32_t numSegment,  Sapphire::SapphireMeshLoadData* meshLoadData, const uint8_t*& ptr)
	{
		for (int32_t i = 0; i < numSegment; ++i)
		{
			String material = readString(ptr);      // name of the material defined in Lua script
			strcpy_s(meshLoadData->m_subMeshes[i].m_material.m_name, material.data);
			//int32_t  primitiveType = *(reinterpret_cast<const int32_t*>(ptr)); // always two
			ptr += sizeof(int32_t);
			int32_t  firstIndex = *(reinterpret_cast<const int32_t*>(ptr)); // starting location in the index list
			ptr += sizeof(int32_t);
			int32_t  count = *(reinterpret_cast<const int32_t*>(ptr)); // number of triangles
			ptr += sizeof(int32_t);

			meshLoadData->m_subMeshes[i].m_indicesStart = firstIndex;
			// count is triangles - each triangle has 3 indices so multiple by 3
			meshLoadData->m_subMeshes[i].m_indicesCount = count * 3;
		}
	}

	void readMeshData(Sapphire::SapphireMeshLoadData* meshLoadData, const uint8_t*& ptr)
	{
		FourCC			magic = readFourCC(ptr);
		//int32_t version = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		int32_t numVertices = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		meshLoadData->m_numVertices = numVertices;
		for (uint32_t i = 0; i < 15; ++i)
		{
			readVertexArray(numVertices, i, meshLoadData, ptr);
		}

		int32_t numIndices = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		meshLoadData->m_numIndices = numIndices;
		meshLoadData->m_indicesPtr = ptr;
		
		int32_t indicesSize = numIndices * sizeof(int32_t);
		ptr += indicesSize;

		int32_t numSegments = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		meshLoadData->m_numSubmeshes = numSegments;
		readMeshSegment(numSegments, meshLoadData, ptr);
				
		Vec3			boundCenter = readVec3(ptr);    // center of the bound sphere in model space

		float			boundRadius = *(reinterpret_cast<const float*>(ptr));   // radius of the bound sphere in model space
		ptr += sizeof(float);

		Vec3			boundMin = readVec3(ptr);       // minimum extents of the bound box in model space
		Vec3			boundMax = readVec3(ptr);       // maximum extents of the bound box in model space

		meshLoadData->m_boundingBoxMin.x = boundMin.x;
		meshLoadData->m_boundingBoxMin.y = boundMin.y;
		meshLoadData->m_boundingBoxMin.z = boundMin.z;
		meshLoadData->m_boundingBoxMax.x = boundMax.x;
		meshLoadData->m_boundingBoxMax.y = boundMax.y;
		meshLoadData->m_boundingBoxMax.z = boundMax.z;
		meshLoadData->m_boundingSphereCenter.x = boundCenter.x;
		meshLoadData->m_boundingSphereCenter.y = boundCenter.y;
		meshLoadData->m_boundingSphereCenter.z = boundCenter.z;
		meshLoadData->m_boundingSphereRadius = boundRadius;
	}

	void readBones(int32_t numBones, const uint8_t*& ptr)
	{
		for (int32_t i = 0; i < numBones; ++i)
		{
			int32_t boneNodeIndex = *(reinterpret_cast<const int32_t*>(ptr));
			ptr += sizeof(int32_t);
			
			Mat4x3		invRestMatrix = readMatrix4x3(ptr);   // transform from model space to bone space

			if (boneNodeIndex)
			{
				continue;
			}
		}
	}

	void readMeshEntity(Sapphire::SapphireMeshLoadData* meshLoadData, const uint8_t*& ptr)
	{
		readMeshData(meshLoadData, ptr);
		int32_t numBones = *(reinterpret_cast<const int32_t*>(ptr));
		ptr += sizeof(int32_t);
		readBones(numBones, ptr);
		readVec3(ptr); // ignored
		//uint8_t castShadow = *ptr;
		ptr += sizeof(uint8_t);
	}

	void readNodes(uint32_t numNodes, Sapphire::SapphireMeshLoadData* meshLoadData, const uint8_t*& ptr)
	{
		for (uint32_t i = 0; i < numNodes; ++i)
		{
			// read node
			String name = readString(ptr);
			Mat4x3 localToParent = readMatrix4x3(ptr);

			int32_t parent = *(reinterpret_cast<const int32_t*>(ptr));
			ptr += sizeof(int32_t);

			int32_t nodeType = *(reinterpret_cast<const int32_t*>(ptr));
			ptr += sizeof(int32_t);
			if (nodeType == 0)
			{
				readMeshEntity(meshLoadData, ptr);
			}

			if (parent || nodeType)
			{
				continue;
			}
		}
	}

	void buildModelFromStream(const uint8_t* streamPtr, Sapphire::SapphireMeshLoadData * meshLoadData)
	{
		const uint8_t* currPtr = streamPtr;
		FourCC fourCC = readFourCC(currPtr);

		int32_t version = *(reinterpret_cast<const int32_t*>(currPtr));
		currPtr += sizeof(int32_t);

		int32_t numNodes = *(reinterpret_cast<const int32_t*>(currPtr));
		currPtr += sizeof(int32_t);

		readNodes(numNodes, meshLoadData, currPtr);

		if (version)
		{
			return;
		}
	}

}




namespace Sapphire
{


	static void LoadTexture(IRenderDevice* pDevice,
		const Char* ResourceDirectory,
		const Char* Name,
		bool                              IsSRGB,
		ITexture** ppTexture,
		ITextureView** ppSRV,
		std::vector<StateTransitionDesc>& Barriers)
	{
		std::string FullPath = ResourceDirectory;
		if (!FullPath.empty() && !FileSystem::IsSlash(FullPath.back()))
			FullPath.push_back(FileSystem::SlashSymbol);
		FullPath.append(Name);
		if (FileSystem::FileExists(FullPath.c_str()))
		{
			TextureLoadInfo LoadInfo;
			LoadInfo.IsSRGB = IsSRGB;
			CreateTextureFromFile(FullPath.c_str(), LoadInfo, pDevice, ppTexture);
			if (*ppTexture != nullptr)
			{
				*ppSRV = (*ppTexture)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
				(*ppSRV)->AddRef();
			}
			else
			{
				LOG_ERROR("Failed to load texture ", Name);
			}
			Barriers.emplace_back(*ppTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE);
		}
	}

	void WorldGPUResourcesManager::init()
	{
		m_numVBs = 0;
		m_numIBs = 0;
		m_numMaterials = 0;
		m_numMeshes = 0;
		m_vertexBuffers.resize(1024);
		m_indexBuffers.resize(1024);
		m_materials.resize(1024);
		m_meshes.resize(1024);

		//m_materialNameToIndex.init();
		
	}

	void WorldGPUResourcesManager::createVertexBuffer(IRenderDevice* pDevice, std::vector<StateTransitionDesc>& barriers, const uint8_t* vertices, VertexBufferData& vertexBufferData, uint32_t* outVertexBufferId)
	{
		uint32_t vbId = m_numVBs;
		++m_numVBs;

		std::stringstream ss;
		ss << "DXSDK Mesh vertex buffer #" << vbId;
		std::string VBName = ss.str();
		BufferDesc  VBDesc;
		VBDesc.Name = VBName.c_str();
		VBDesc.Usage = USAGE_IMMUTABLE;
		VBDesc.Size = vertexBufferData.m_numVertices * vertexBufferData.m_strideSize;
		VBDesc.BindFlags = BIND_VERTEX_BUFFER;

		BufferData InitData{ vertices, vertexBufferData.m_dataSize };
		pDevice->CreateBuffer(VBDesc, &InitData, &m_vertexBuffers[vbId]);

		barriers.emplace_back(m_vertexBuffers[vbId], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);
		*outVertexBufferId = vbId;
	}

	void WorldGPUResourcesManager::createIndexBuffer(IRenderDevice* pDevice, std::vector<StateTransitionDesc>& barriers, const uint8_t* indices, IndexBufferData& indexBufferData, uint32_t* outIndexBufferId)
	{
		uint32_t ibId = m_numIBs;
		++m_numIBs;

		std::stringstream ss;
		ss << "DXSDK Mesh index buffer #" << ibId;
		std::string IBName = ss.str();

		BufferDesc IBDesc;
		IBDesc.Name = IBName.c_str();
		IBDesc.Usage = USAGE_IMMUTABLE;
		IBDesc.Size = indexBufferData.m_numIndices * 4;
		IBDesc.BindFlags = BIND_INDEX_BUFFER;

		BufferData InitData{ indices, static_cast<Uint32>(indexBufferData.m_dataSize) };
		pDevice->CreateBuffer(IBDesc, &InitData, &m_indexBuffers[ibId]);

		barriers.emplace_back(m_indexBuffers[ibId], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);
		*outIndexBufferId = ibId;
	}

	void WorldGPUResourcesManager::loadGPUResources(IRenderDevice* pDevice, IDeviceContext* pDeviceCtx, const char* resourceFolder)
	{
		std::vector<StateTransitionDesc> barriers;
		for (size_t i = 0; i < m_materialDefs.size(); ++i)
		{
			const SapphireMaterialLoadingData& materialLoadData = m_materialDefs[i];
			LoadTexture(pDevice, resourceFolder, materialLoadData.m_diffuseTexture, true, &m_materials[i].pDiffuseTexture, &m_materials[i].pDiffuseRV, barriers);
			LoadTexture(pDevice, resourceFolder, materialLoadData.m_normalsTexture, true, &m_materials[i].pNormalTexture, &m_materials[i].pNormalRV, barriers);
			LoadTexture(pDevice, resourceFolder, materialLoadData.m_normalsTexture, true, &m_materials[i].pARMTexture, &m_materials[i].pARMRV, barriers);
		}

		for (size_t i = 0; i < m_meshDefs.size(); ++i)
		{
			const SapphireMeshLoadData meshLoadData = m_meshDefs[i];
			loadMeshResouces(pDevice, pDeviceCtx, resourceFolder, meshLoadData, barriers);
		}

		pDeviceCtx->TransitionResourceStates(static_cast<Uint32>(barriers.size()), barriers.data());
	}

	void WorldGPUResourcesManager::addMaterial(const SapphireMaterialLoadingData& materialLoadData)
	{
		m_materialDefs.push_back(materialLoadData);
	}

	void WorldGPUResourcesManager::addMesh(const SapphireMeshLoadData& meshLoadData)
	{
		m_meshDefs.push_back(meshLoadData);
	}
	

	

	void WorldGPUResourcesManager::loadMeshResouces(IRenderDevice* pDevice, IDeviceContext* pDeviceCtx, const char* resourceFolder, const SapphireMeshLoadData& meshLoadData, std::vector<StateTransitionDesc>& barriers)
	{

		const uint32_t vertexStrideSize = sizeof(float) * (3 + 3 + 2);
		auto vericesDataBlob = DataBlobImpl::Create(vertexStrideSize * meshLoadData.m_numVertices);
		uint8_t* pVerticesBuffer = reinterpret_cast<Uint8*>(vericesDataBlob->GetDataPtr());
		
		const uint8_t* pos_ptr = meshLoadData.m_verticesPositionPtr;
		const uint8_t* norm_ptr = meshLoadData.m_verticesNormalsPtr;
		const uint8_t* uv_ptr = meshLoadData.m_verticesUVPtr;
		for (uint32_t i = 0; i < meshLoadData.m_numVertices; ++i)
		{
			
			memcpy(pVerticesBuffer, pos_ptr, sizeof(float) * 3);
			pVerticesBuffer += sizeof(float) * 3;
			pos_ptr += sizeof(float) * 3;
			memcpy(pVerticesBuffer, norm_ptr, sizeof(float) * 3);
			pVerticesBuffer += sizeof(float) * 3;
			norm_ptr += sizeof(float) * 3;
			memcpy(pVerticesBuffer, uv_ptr, sizeof(float) * 2);
			
			

			pVerticesBuffer += sizeof(float) * 2;
			uv_ptr += sizeof(float) * 2;
		}
		
		uint32_t vbId;

		VertexBufferData vbData;
		vbData.m_numVertices = meshLoadData.m_numVertices;
		vbData.m_strideSize = vertexStrideSize;
		vbData.m_dataSize = vertexStrideSize * meshLoadData.m_numVertices;
		const uint8_t* finalBuff = reinterpret_cast<const Uint8*>(vericesDataBlob->GetConstDataPtr());
		createVertexBuffer(pDevice, barriers, finalBuff, vbData, &vbId);

		IndexBufferData ibData;
		ibData.m_numIndices = meshLoadData.m_numIndices;
		ibData.m_dataSize = meshLoadData.m_numIndices * sizeof(uint32_t);

		uint32_t ibId;
		createIndexBuffer(pDevice, barriers, meshLoadData.m_indicesPtr, ibData, &ibId);

		uint32_t meshId = m_numMeshes;
		++m_numMeshes;

		SapphireMesh& mesh = m_meshes[meshId];
		mesh.m_ib = ibId;
		mesh.m_vb = vbId;
		mesh.m_boundingBoxMax = meshLoadData.m_boundingBoxMax;
		mesh.m_boundingBoxMin = meshLoadData.m_boundingBoxMin;
		mesh.m_boundingSphereCenter = meshLoadData.m_boundingSphereCenter;
		mesh.m_boundingSphereRadius = meshLoadData.m_boundingSphereRadius;
		mesh.m_numSubsets = meshLoadData.m_numSubmeshes;

		uint32_t matId;
		for (uint32_t i = 0; i < meshLoadData.m_numSubmeshes; ++i)
		{			
			meshLoadData.m_subMeshes[i].m_material.m_name;
			matId = i;
			mesh.m_subMeshes[i].m_materialId = matId;
			mesh.m_subMeshes[i].m_indicesCount = meshLoadData.m_subMeshes[i].m_indicesCount;
			mesh.m_subMeshes[i].m_indicesStart = meshLoadData.m_subMeshes[i].m_indicesStart;
		}
		
		
	}

	void loadModelFromFile(const char* file_path, SapphireMeshLoadData* meshLoadData)
	{
		try
		{
			FileWrapper File{ file_path, EFileAccessMode::Read };
			if (!File)
				LOG_ERROR_AND_THROW("Failed to open file '", file_path, "'.");

			meshLoadData->m_blob = DataBlobImpl::Create();
			File->Read(meshLoadData->m_blob);

			File.Close();

			
			const uint8_t* stream = reinterpret_cast<const Uint8*>(meshLoadData->m_blob->GetConstDataPtr());
			Grimrock::buildModelFromStream(stream, meshLoadData);
			return;
			//RefCntAutoPtr<ITextureLoader> pTexLoader{
			//	MakeNewRCObj<TextureLoaderImpl>()(TexLoadInfo, reinterpret_cast<const Uint8*>(pFileData->GetConstDataPtr()), pFileData->GetSize(), std::move(pFileData)) //
			//};
			//if (pTexLoader)
			//	pTexLoader->QueryInterface(IID_TextureLoader, reinterpret_cast<IObject**>(ppLoader));
		}
		catch (std::runtime_error& err)
		{
			LOG_ERROR("Failed to create texture loader from file: ", err.what());
		}
	}
}