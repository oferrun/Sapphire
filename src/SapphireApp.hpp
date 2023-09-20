
#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"


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

private:
    void UpdateUI();

    bool   m_ShowDemoWindow    = true;
    bool   m_ShowAnotherWindow = false;
    float4 m_ClearColor        = {0.45f, 0.55f, 0.60f, 1.00f};
};

} // namespace Sapphire
