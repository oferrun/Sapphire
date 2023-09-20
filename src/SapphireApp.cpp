
#include "SapphireApp.hpp"
#include "imgui.h"
#include "ColorConversion.h"
#include "GraphicsAccessories.hpp"

using namespace Diligent;

SampleBase* Diligent::CreateSample()
{
    return new Sapphire::SapphireApp();
}


namespace Sapphire
{
    

SapphireApp::~SapphireApp()
{
}

void SapphireApp::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    // Reset default colors
    ImGui::StyleColorsDark();
}

void SapphireApp::UpdateUI()
{
   
   

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &m_ShowDemoWindow); // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &m_ShowAnotherWindow);

        static float f = 0.0f;
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&m_ClearColor); // Edit 3 floats representing a color

        static int counter = 0;
        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (m_ShowAnotherWindow)
    {
        ImGui::Begin("Another Window", &m_ShowAnotherWindow); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            m_ShowAnotherWindow = false;
        ImGui::End();
    }
}

// Render a frame
void SapphireApp::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

    auto ClearColor = m_ClearColor;
    if (GetTextureFormatAttribs(m_pSwapChain->GetDesc().ColorBufferFormat).ComponentType == COMPONENT_TYPE_UNORM_SRGB)
    {
        ClearColor.r = SRGBToLinear(ClearColor.r);
        ClearColor.g = SRGBToLinear(ClearColor.g);
        ClearColor.b = SRGBToLinear(ClearColor.b);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, &ClearColor.x, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}


void SapphireApp::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateUI();
}

void SapphireApp::WindowResize(Uint32 Width, Uint32 Height)
{
}

} // namespace Sapphire
