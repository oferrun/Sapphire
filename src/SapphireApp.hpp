
#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "GrimrockModelLoader.h"

namespace Sapphire
{
   using namespace Diligent;

class SapphireApp final : public SampleBase
{
public:
    ~SapphireApp();

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Dear Imgui Demo"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

    void UpdateLuaFrameStart(double CurrTime, double ElapsedTime);
    void UpdateLuaFrameEnd(double CurrTime, double ElapsedTime);

    WorldGPUResourcesManager* GetWorldGPUResourceManager() { return &m_worldResourceManager; }

private:
    void CreatePipelineState();
    void CreatePlanePipelineState();
    void CreateVertexBuffer();
    void LoadTextures();
    void CreateIndexBuffer();

    float4x4 GetOrthographicMatrix(float orthographicSize);

    void UpdateUI();

    bool   m_ShowDemoWindow    = true;
    bool   m_ShowAnotherWindow = false;
    float4 m_ClearColor        = {0.45f, 0.55f, 0.60f, 1.00f};

    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IPipelineState>         m_pPlanePSO;
    RefCntAutoPtr<IBuffer>                m_CameraAttribsCB;
    RefCntAutoPtr<IBuffer>                m_LightAttribsCB;


    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    RefCntAutoPtr<ITextureView>           m_TextureSRV[4];
    RefCntAutoPtr<IShaderResourceBinding> m_SRB[4];
    float4x4                              m_ViewProjMatrix;

    RefCntAutoPtr<IBuffer>                m_QuadVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_QuadIndexBuffer;


    RefCntAutoPtr<IBuffer>                m_GridAttribsCB;
    RefCntAutoPtr<IShaderResourceBinding> m_PlaneSRB;
    RefCntAutoPtr<IBuffer>                m_PlaneVertexBuffer;

    RefCntAutoPtr<IBuffer>                m_TerrainVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_TerrainIndexBuffer;

    WorldGPUResourcesManager              m_worldResourceManager;

    float3 m_LightDirection;
    float4 m_LightColor = float4(1, 1, 1, 1);
    float  m_LightIntensity = 3.f;

    ///
    double m_timeAccumulator;
};

} // namespace Sapphire
