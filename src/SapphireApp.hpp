
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
    static constexpr TEXTURE_FORMAT RenderTargetFormat = TEX_FORMAT_RGBA8_UNORM;
    static constexpr TEXTURE_FORMAT DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

    static constexpr TEXTURE_FORMAT HighlightRenderTargetFormat = TEX_FORMAT_R8_UNORM;
    

    void CreateSelectionCompositPSO();
    void CreateRenderTargetPSO();
    void CreateSelectionRenderTargetPSO();
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

    // picking
    RefCntAutoPtr<IBuffer>                m_pPickingBuffer;
    RefCntAutoPtr<IBuffer>                m_pPickingBufferStaging;
    RefCntAutoPtr<IFence>                 m_pPickingInfoAvailable;

    RefCntAutoPtr<IBuffer>                m_TerrainVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_TerrainIndexBuffer;

    RefCntAutoPtr<IPipelineState>         m_pRTPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRTSRB;

    RefCntAutoPtr<IPipelineState>         m_pSelectionRTPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pSelectionRTSRB;
    RefCntAutoPtr<IPipelineState>         m_pSelectionCompositPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pSelectionCompositSRB;

    // Offscreen render target and depth-stencil
    RefCntAutoPtr<ITextureView> m_pColorRTV;
    RefCntAutoPtr<ITextureView> m_pDepthDSV;

    // Offscreen render target and depth-stencil
    RefCntAutoPtr<ITextureView> m_pSelectionColorRTV;
    RefCntAutoPtr<ITextureView> m_pSelectionDepthDSV;

    WorldGPUResourcesManager              m_worldResourceManager;

    float3 m_LightDirection;
    float4 m_LightColor = float4(1, 1, 1, 1);
    float  m_LightIntensity = 3.f;

    ///
    double m_timeAccumulator;
};

} // namespace Sapphire
