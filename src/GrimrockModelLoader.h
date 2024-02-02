#pragma once

#include "../../../DiligentCore/Common/interface/BasicMath.hpp"
#include "DataBlobImpl.hpp"
#include "StringToIndexHashTable.h"

using namespace Diligent;

namespace Sapphire
{
	struct SapphireMeshInstance
	{
		uint32_t m_meshId;
		float4x4 m_transform;

	};
	

	struct SapphireSubmesh
	{
		uint32_t m_materialId;
		uint32_t m_indicesCount;
		uint32_t m_indicesStart;
	};



	struct SapphireMesh
	{
		static constexpr size_t MAX_SUB_MESHES = 4;
		uint32_t m_vb;
		uint32_t m_ib;
		uint32_t m_numSubsets;
		SapphireSubmesh m_subMeshes[MAX_SUB_MESHES];
		// in local space
		float3 m_boundingBoxMin;
		float3 m_boundingBoxMax;
		float3 m_boundingSphereCenter;
		float m_boundingSphereRadius;
	};

	struct SapphireMaterialLoadingData
	{
		static constexpr size_t MaxMaterialName = 64;
		static constexpr size_t MaxTextureFilePath = 128;
		char                    m_name[MaxMaterialName];
		char                    m_diffuseTexture[MaxTextureFilePath];
		char                    m_normalsTexture[MaxTextureFilePath];
		char                    m_armTexture[MaxTextureFilePath];
	};

	struct SapphireMaterial
	{
		union
		{
			Uint64    Force64_1; //Force the union to 64bits
			ITexture* pDiffuseTexture = nullptr;
		};
		union
		{
			Uint64    Force64_2; //Force the union to 64bits
			ITexture* pNormalTexture = nullptr;
		};
		union
		{
			Uint64    Force64_3; //Force the union to 64bits
			ITexture* pARMTexture = nullptr;
		};

		union
		{
			Uint64        Force64_4; //Force the union to 64bits
			ITextureView* pDiffuseRV = nullptr;
		};
		union
		{
			Uint64        Force64_5; //Force the union to 64bits
			ITextureView* pNormalRV = nullptr;
		};
		union
		{
			Uint64        Force64_6; //Force the union to 64bits
			ITextureView* pARMRV = nullptr;
		};
	};

	struct SapphireSubmeshLoadingData
	{
		uint32_t m_indicesCount;
		uint32_t m_indicesStart;
		SapphireMaterialLoadingData m_material;
	};

	struct SapphireMeshLoadData
	{
		static constexpr size_t MAX_SUB_MESHES = 4;
		uint32_t m_numVertices;
		uint32_t m_numIndices;
		uint32_t m_numSubmeshes;
		const uint8_t* m_verticesPositionPtr;
		const uint8_t* m_verticesNormalsPtr;
		const uint8_t* m_verticesUVPtr;
		const uint8_t* m_indicesPtr;

		SapphireSubmeshLoadingData m_subMeshes[MAX_SUB_MESHES];

		float3 m_boundingBoxMin;
		float3 m_boundingBoxMax;
		float3 m_boundingSphereCenter;
		float m_boundingSphereRadius;

		RefCntAutoPtr<DataBlobImpl> m_blob;

	};

	struct VertexBufferData
	{
		uint32_t m_numVertices;
		uint32_t m_strideSize;
		uint32_t m_dataSize;
	};

	struct IndexBufferData
	{
		uint32_t m_numIndices;
		uint32_t m_dataSize;
	};

	void loadModelFromFile(const char* file_path, SapphireMeshLoadData* meshLoadData);

	class WorldGPUResourcesManager
	{
	private:
		std::vector<RefCntAutoPtr<IBuffer>> m_vertexBuffers;
		std::vector<RefCntAutoPtr<IBuffer>> m_indexBuffers;
		std::vector<SapphireMaterial> m_materials;
		uint32_t m_numVBs;
		uint32_t m_numIBs;
		uint32_t m_numMaterials;
		uint32_t m_numMeshes;

		std::vector<SapphireMesh> m_meshes;

		std::vector< SapphireMaterialLoadingData> m_materialDefs;
		std::vector< SapphireMeshLoadData> m_meshDefs;

		StringToIndexHashTable m_materialNameToIndex;

		

	public:
		void init();
		void loadGPUResources(IRenderDevice* pDevice, IDeviceContext* pDeviceCtx,const char* resourceFolder);
		void addMaterial(const SapphireMaterialLoadingData& materialLoadData);
		void addMesh(const SapphireMeshLoadData& meshLoadData);
		void createVertexBuffer(IRenderDevice* pDevice, std::vector<StateTransitionDesc>& barriers, const uint8_t* vertices, VertexBufferData& vertexBufferData, uint32_t* outVertexBufferId);
		void createIndexBuffer(IRenderDevice* pDevice, std::vector<StateTransitionDesc>& barriers, const uint8_t* indices, IndexBufferData& indexBufferData, uint32_t* outIndexBufferId);
		void loadMaterial(const char* resourcesFolder, std::vector<StateTransitionDesc>& barriers, SapphireMaterialLoadingData& materialLoadData, uint32_t* outMaterialId);
		void loadMeshResouces(IRenderDevice* pDevice, IDeviceContext* pDeviceCtx, const char* resourceFolder, const SapphireMeshLoadData& meshLoadData, std::vector<StateTransitionDesc>& barriers);

		uint32_t getNumMeshes() const { return m_numMeshes; }
		const SapphireMesh* GetMesh(uint32_t i) const { return &m_meshes[i]; }

		IBuffer* getVertexBuffer(Uint32 iVB) { return m_vertexBuffers[iVB]; }
		IBuffer* getIndexBuffer(Uint32 iIB) { return m_indexBuffers[iIB]; }
	};
} // namespace Sapphire