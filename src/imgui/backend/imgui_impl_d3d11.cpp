// dear imgui: Renderer Backend for DirectX11
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'ID3D11ShaderResourceView*' as texture identifier. Read the FAQ about ImTextureID/ImTextureRef!
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).
//  [X] Renderer: Expose selected render state for draw callbacks to use. Access in '(ImGui_ImplXXXX_RenderState*)GetPlatformIO().Renderer_RenderState'.
//  [X] Renderer: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2025-XX-XX: Platform: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2025-06-11: DirectX11: Added support for ImGuiBackendFlags_RendererHasTextures, for dynamic font atlas.
//  2025-05-07: DirectX11: Honor draw_data->FramebufferScale to allow for custom backends and experiment using it (consistently with other renderer backends, even though in normal condition it is not set under Windows).
//  2025-02-24: [Docking] Added undocumented ImGui_ImplDX11_SetSwapChainDescs() to configure swap chain creation for secondary viewports.
//  2025-01-06: DirectX11: Expose VertexConstantBuffer in ImGui_ImplDX11_RenderState. Reset projection matrix in ImDrawCallback_ResetRenderState handler.
//  2024-10-07: DirectX11: Changed default texture sampler to Clamp instead of Repeat/Wrap.
//  2024-10-07: DirectX11: Expose selected render state in ImGui_ImplDX11_RenderState, which you can access in 'void* platform_io.Renderer_RenderState' during draw callbacks.
//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-05-19: DirectX11: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-02-18: DirectX11: Change blending equation to preserve alpha in output buffer.
//  2019-08-01: DirectX11: Fixed code querying the Geometry Shader state (would generally error with Debug layer enabled).
//  2019-07-21: DirectX11: Backup, clear and restore Geometry Shader is any is bound when calling ImGui_ImplDX11_RenderDrawData. Clearing Hull/Domain/Compute shaders without backup/restore.
//  2019-05-29: DirectX11: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.
//  2019-04-30: DirectX11: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2018-12-03: Misc: Added #pragma comment statement to automatically link with d3dcompiler.lib when using D3DCompile().
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-08-01: DirectX11: Querying for IDXGIFactory instead of IDXGIFactory1 to increase compatibility.
//  2018-07-13: DirectX11: Fixed unreleased resources in Init and Shutdown functions.
//  2018-06-08: Misc: Extracted imgui_impl_dx11.cpp/.h away from the old combined DX11+Win32 example.
//  2018-06-08: DirectX11: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplDX11_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2016-05-07: DirectX11: Disabling depth-write.

#include "imgui/3rdparty/imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui/backend/imgui_custom_draw.h"
#include "imgui/backend/imgui_impl_d3d11.h"
#include <vector>

// DirectX
#include <stdio.h>
#include <d3d11.h>

#if 0 // do not use default shaders
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast                            // yes, they are more terse.
#pragma clang diagnostic ignored "-Wsign-conversion"        // warning: implicit conversion changes signedness
#endif
#endif

#include "shader_imgui_default_ps.h"
#include "shader_imgui_default_vs.h"
#include "shader_imgui_waveform_ps.h"
#include "shader_imgui_waveform_vs.h"

// DirectX11 data
struct ImGui_ImplDX11_Texture
{
    ID3D11Texture2D*            pTexture;
    ID3D11ShaderResourceView*   pTextureView;
};

struct ImGui_ImplDX11_Data
{
    ID3D11Device*               pd3dDevice;
    ID3D11DeviceContext*        pd3dDeviceContext;
    IDXGIFactory*               pFactory;
    ID3D11Buffer*               pVB;
    ID3D11Buffer*               pIB;
    ID3D11VertexShader*         pVertexShader;
    ID3D11VertexShader*         WaveformVS;
    ID3D11InputLayout*          pInputLayout;
    ID3D11Buffer*               pVertexConstantBuffer;
    ID3D11Buffer*               WaveformConstantBuffer;
    ID3D11PixelShader*          pPixelShader;
    ID3D11PixelShader*          WaveformPS;
    ID3D11SamplerState*         pFontSampler;
    ID3D11ShaderResourceView*   pFontTextureView;
    ID3D11RasterizerState*      pRasterizerState;
    ID3D11BlendState*           pBlendState;
    ID3D11DepthStencilState*    pDepthStencilState;
    int                         VertexBufferSize;
    int                         IndexBufferSize;
    ImVector<DXGI_SWAP_CHAIN_DESC> SwapChainDescsForViewports;

    ImGui_ImplDX11_Data()       { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct VERTEX_CONSTANT_BUFFER_DX11
{
    float   mvp[4][4];
};

struct WAVEFORM_CONSTANT_BUFFER
{
	struct { vec2 Pos, UV; } PerVertex[6];
	struct { vec2 Size, SizeInv; } CB_RectSize;
	struct { f32 R, G, B, A; } Color;
	float Amplitudes[CustomDraw::WaveformPixelsPerChunk];
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Forward Declarations
static void ImGui_ImplDX11_InitMultiViewportSupport();
static void ImGui_ImplDX11_ShutdownMultiViewportSupport();
namespace CustomDraw
{
	static void DX11RenderInit(ImGui_ImplDX11_Data* bd);
	static void DX11BeginRenderDrawData(ImDrawData* drawData);
	static void DX11EndRenderDrawData(ImDrawData* drawData);
	static void DX11ReleaseDeferedResources(ImGui_ImplDX11_Data* bd);
}

// Functions
static void ImGui_ImplDX11_SetupRenderState(ImDrawData* draw_data, ID3D11DeviceContext* device_ctx)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

    // Setup viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
    vp.Height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0;
    device_ctx->RSSetViewports(1, &vp);

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    if (device_ctx->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) == S_OK)
    {
        VERTEX_CONSTANT_BUFFER_DX11* constant_buffer = (VERTEX_CONSTANT_BUFFER_DX11*)mapped_resource.pData;
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] =
        {
            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
        };
        memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
        device_ctx->Unmap(bd->pVertexConstantBuffer, 0);
    }

    // Setup shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;
    device_ctx->IASetInputLayout(bd->pInputLayout);
    device_ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);
    device_ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_ctx->VSSetShader(bd->pVertexShader, nullptr, 0);
	ID3D11Buffer* constantBuffers[] = { bd->pVertexConstantBuffer, bd->WaveformConstantBuffer };
	device_ctx->VSSetConstantBuffers(0, 2, constantBuffers);
	device_ctx->PSSetConstantBuffers(0, 2, constantBuffers);
    device_ctx->PSSetShader(bd->pPixelShader, nullptr, 0);
    device_ctx->PSSetSamplers(0, 1, &bd->pFontSampler);
    device_ctx->GSSetShader(nullptr, nullptr, 0);
    device_ctx->HSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..
    device_ctx->DSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..
    device_ctx->CSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..

    // Setup render state
    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    device_ctx->OMSetBlendState(bd->pBlendState, blend_factor, 0xffffffff);
    device_ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);
    device_ctx->RSSetState(bd->pRasterizerState);
}

// Render function
void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ID3D11DeviceContext* device = bd->pd3dDeviceContext;

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplDX11_UpdateTexture(tex);

    // Create and grow vertex/index buffers if needed
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = bd->VertexBufferSize * sizeof(ImDrawVert);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVB) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = bd->IndexBufferSize * sizeof(ImDrawIdx);
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pIB) < 0)
            return;
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    if (device->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
        return;
    if (device->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
        return;
    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += draw_list->VtxBuffer.Size;
        idx_dst += draw_list->IdxBuffer.Size;
    }
    device->Unmap(bd->pVB, 0);
    device->Unmap(bd->pIB, 0);

    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
    struct BACKUP_DX11_STATE
    {
        UINT                        ScissorRectsCount, ViewportsCount;
        D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState*      RS;
        ID3D11BlendState*           pBlendState;
        FLOAT                       BlendFactor[4];
        UINT                        SampleMask;
        UINT                        StencilRef;
        ID3D11DepthStencilState*    pDepthStencilState;
        ID3D11ShaderResourceView*   PSShaderResource;
        ID3D11SamplerState*         PSSampler;
        ID3D11PixelShader*          PS;
        ID3D11VertexShader*         VS;
        ID3D11GeometryShader*       GS;
        UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];   // 256 is max according to PSSetShader documentation
        D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
        ID3D11Buffer*               pIB, *pVB, *VSConstantBuffer;
        UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT                 IndexBufferFormat;
        ID3D11InputLayout*          pInputLayout;
    };
    BACKUP_DX11_STATE old = {};
    old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    device->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
    device->RSGetViewports(&old.ViewportsCount, old.Viewports);
    device->RSGetState(&old.RS);
    device->OMGetBlendState(&old.pBlendState, old.BlendFactor, &old.SampleMask);
    device->OMGetDepthStencilState(&old.pDepthStencilState, &old.StencilRef);
    device->PSGetShaderResources(0, 1, &old.PSShaderResource);
    device->PSGetSamplers(0, 1, &old.PSSampler);
    old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
    device->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
    device->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
    device->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
    device->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

    device->IAGetPrimitiveTopology(&old.PrimitiveTopology);
    device->IAGetIndexBuffer(&old.pIB, &old.IndexBufferFormat, &old.IndexBufferOffset);
    device->IAGetVertexBuffers(0, 1, &old.pVB, &old.VertexBufferStride, &old.VertexBufferOffset);
    device->IAGetInputLayout(&old.pInputLayout);

    // Setup desired DX state
    ImGui_ImplDX11_SetupRenderState(draw_data, device);
	CustomDraw::DX11BeginRenderDrawData(draw_data);

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplDX11_RenderState render_state;
    render_state.Device = bd->pd3dDevice;
    render_state.DeviceContext = bd->pd3dDeviceContext;
    render_state.SamplerDefault = bd->pFontSampler;
    render_state.VertexConstantBuffer = bd->pVertexConstantBuffer;
    platform_io.Renderer_RenderState = &render_state;

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_idx_offset = 0;
    int global_vtx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = draw_data->FramebufferScale;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX11_SetupRenderState(draw_data, device);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                device->RSSetScissorRects(1, &r);

                // Bind texture, Draw
                ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();
                device->PSSetShaderResources(0, 1, &texture_srv);
                device->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }

	CustomDraw::DX11EndRenderDrawData(draw_data);

    platform_io.Renderer_RenderState = nullptr;

    // Restore modified DX state
    device->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
    device->RSSetViewports(old.ViewportsCount, old.Viewports);
    device->RSSetState(old.RS); if (old.RS) old.RS->Release();
    device->OMSetBlendState(old.pBlendState, old.BlendFactor, old.SampleMask); if (old.pBlendState) old.pBlendState->Release();
    device->OMSetDepthStencilState(old.pDepthStencilState, old.StencilRef); if (old.pDepthStencilState) old.pDepthStencilState->Release();
    device->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();
    device->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();
    device->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
    for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
    device->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
    device->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
    device->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
    for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
    device->IASetPrimitiveTopology(old.PrimitiveTopology);
    device->IASetIndexBuffer(old.pIB, old.IndexBufferFormat, old.IndexBufferOffset); if (old.pIB) old.pIB->Release();
    device->IASetVertexBuffers(0, 1, &old.pVB, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.pVB) old.pVB->Release();
    device->IASetInputLayout(old.pInputLayout); if (old.pInputLayout) old.pInputLayout->Release();
}

static void ImGui_ImplDX11_DestroyTexture(ImTextureData* tex)
{
    ImGui_ImplDX11_Texture* backend_tex = (ImGui_ImplDX11_Texture*)tex->BackendUserData;
    if (backend_tex == nullptr)
        return;
    IM_ASSERT(backend_tex->pTextureView == (ID3D11ShaderResourceView*)(intptr_t)tex->TexID);
    backend_tex->pTextureView->Release();
    backend_tex->pTexture->Release();
    IM_DELETE(backend_tex);

    // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
    tex->BackendUserData = nullptr;
}

void ImGui_ImplDX11_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        unsigned int* pixels = (unsigned int*)tex->GetPixels();
        ImGui_ImplDX11_Texture* backend_tex = IM_NEW(ImGui_ImplDX11_Texture)();

        // Create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = (UINT)tex->Width;
        desc.Height = (UINT)tex->Height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = pixels;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &backend_tex->pTexture);
        IM_ASSERT(backend_tex->pTexture != nullptr && "Backend failed to create texture!");

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        bd->pd3dDevice->CreateShaderResourceView(backend_tex->pTexture, &srvDesc, &backend_tex->pTextureView);
        IM_ASSERT(backend_tex->pTextureView != nullptr && "Backend failed to create texture!");

        // Store identifiers
        tex->SetTexID((ImTextureID)(intptr_t)backend_tex->pTextureView);
        tex->SetStatus(ImTextureStatus_OK);
        tex->BackendUserData = backend_tex;
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->Updates[] but you can use tex->UpdateRect to upload a single region.
        ImGui_ImplDX11_Texture* backend_tex = (ImGui_ImplDX11_Texture*)tex->BackendUserData;
        IM_ASSERT(backend_tex->pTextureView == (ID3D11ShaderResourceView*)(intptr_t)tex->TexID);
        for (ImTextureRect& r : tex->Updates)
        {
            D3D11_BOX box = { (UINT)r.x, (UINT)r.y, (UINT)0, (UINT)(r.x + r.w), (UINT)(r.y + r .h), (UINT)1 };
            bd->pd3dDeviceContext->UpdateSubresource(backend_tex->pTexture, 0, &box, tex->GetPixelsAt(r.x, r.y), (UINT)tex->GetPitch(), 0);
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
        ImGui_ImplDX11_DestroyTexture(tex);
}

bool ImGui_ImplDX11_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Upload texture to graphics system
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D11Texture2D* pTexture = nullptr;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = pixels;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
        IM_ASSERT(pTexture != nullptr);

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
        pTexture->Release();
    }

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);

    // Create texture sampler
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        bd->pd3dDevice->CreateSamplerState(&desc, &bd->pFontSampler);
    }

    return true;
}

bool    ImGui_ImplDX11_CreateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;
    ImGui_ImplDX11_InvalidateDeviceObjects();

    // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
    // If you would like to use this DX11 sample code but remove this dependency you can:
    //  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
    //  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
    // See https://github.com/ocornut/imgui/pull/638 for sources and details.

    // Create the vertex shader
    {
#if 0 // do not use default shaders
        static const char* vertexShader =
            "cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

        ID3DBlob* vertexShaderBlob;
        if (FAILED(D3DCompile(vertexShader, strlen(vertexShader), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vertexShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &bd->pVertexShader) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }
#endif

        if (bd->pd3dDevice->CreateVertexShader(shader_imgui_default_vs_bytecode, sizeof(shader_imgui_default_vs_bytecode), nullptr, &bd->pVertexShader) != S_OK)
            return false;

        if (bd->pd3dDevice->CreateVertexShader(shader_imgui_waveform_vs_bytecode, sizeof(shader_imgui_waveform_vs_bytecode), nullptr, &bd->WaveformVS) != S_OK)
            return false;

        // Create the input layout
        D3D11_INPUT_ELEMENT_DESC local_layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
#if 0
        if (bd->pd3dDevice->CreateInputLayout(local_layout, 3, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &bd->pInputLayout) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }
        vertexShaderBlob->Release();
#endif
        if (bd->pd3dDevice->CreateInputLayout(local_layout, 3, shader_imgui_default_vs_bytecode, sizeof(shader_imgui_default_vs_bytecode), &bd->pInputLayout) != S_OK)
            return false;

        // Create the constant buffer
        {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER_DX11);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVertexConstantBuffer);

            desc.ByteWidth = sizeof(WAVEFORM_CONSTANT_BUFFER);
            bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->WaveformConstantBuffer);
        }
    }

    // Create the pixel shader
    {
#if 0 // do not use default shaders
        static const char* pixelShader =
            "struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            sampler sampler0;\
            Texture2D texture0;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
            }";

        ID3DBlob* pixelShaderBlob;
        if (FAILED(D3DCompile(pixelShader, strlen(pixelShader), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &pixelShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &bd->pPixelShader) != S_OK)
        {
            pixelShaderBlob->Release();
            return false;
        }
        pixelShaderBlob->Release();
#endif
		if (bd->pd3dDevice->CreatePixelShader(shader_imgui_default_ps_bytecode, sizeof(shader_imgui_default_ps_bytecode), NULL, &bd->pPixelShader) != S_OK)
			return false;

        if (bd->pd3dDevice->CreatePixelShader(shader_imgui_waveform_ps_bytecode, sizeof(shader_imgui_waveform_ps_bytecode), NULL, &bd->WaveformPS) != S_OK)
            return false;
    }

    // Create the blending setup
    {
        D3D11_BLEND_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bd->pd3dDevice->CreateBlendState(&desc, &bd->pBlendState);
    }

    // Create the rasterizer state
    {
        D3D11_RASTERIZER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        bd->pd3dDevice->CreateRasterizerState(&desc, &bd->pRasterizerState);
    }

    // Create depth-stencil State
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        bd->pd3dDevice->CreateDepthStencilState(&desc, &bd->pDepthStencilState);
    }

    // Create texture sampler
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    {
        D3D11_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        bd->pd3dDevice->CreateSamplerState(&desc, &bd->pFontSampler);
    }

    ImGui_ImplDX11_CreateFontsTexture();

    return true;
}

void    ImGui_ImplDX11_InvalidateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return;

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
            ImGui_ImplDX11_DestroyTexture(tex);

    if (bd->pFontSampler)           { bd->pFontSampler->Release(); bd->pFontSampler = nullptr; }
    if (bd->pFontTextureView)        { bd->pFontTextureView->Release(); bd->pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(0); } // we copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
    if (bd->pIB)                    { bd->pIB->Release(); bd->pIB = nullptr; }
    if (bd->pVB)                    { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pBlendState)            { bd->pBlendState->Release(); bd->pBlendState = nullptr; }
    if (bd->pDepthStencilState)     { bd->pDepthStencilState->Release(); bd->pDepthStencilState = nullptr; }
    if (bd->pRasterizerState)       { bd->pRasterizerState->Release(); bd->pRasterizerState = nullptr; }
    if (bd->pPixelShader)           { bd->pPixelShader->Release(); bd->pPixelShader = nullptr; }
    if (bd->WaveformPS)             { bd->WaveformPS->Release(); bd->WaveformPS = nullptr; }
    if (bd->WaveformConstantBuffer) { bd->WaveformConstantBuffer->Release(); bd->WaveformConstantBuffer = nullptr; }
    if (bd->pVertexConstantBuffer)  { bd->pVertexConstantBuffer->Release(); bd->pVertexConstantBuffer = nullptr; }
    if (bd->pInputLayout)           { bd->pInputLayout->Release(); bd->pInputLayout = nullptr; }
    if (bd->pVertexShader)          { bd->pVertexShader->Release(); bd->pVertexShader = nullptr; }
    if (bd->WaveformVS)             { bd->WaveformVS->Release(); bd->WaveformVS = nullptr; }
}

bool ImGui_ImplDX11_RecreateFontTexture()
{
	ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;

    if (bd->pFontSampler) { bd->pFontSampler->Release(); bd->pFontSampler = nullptr; }
    if (bd->pFontTextureView) { bd->pFontTextureView->Release(); bd->pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(0); } // we copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
    ImGui_ImplDX11_CreateFontsTexture();
    return true;
}

bool    ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    // Get factory from device
    IDXGIDevice* pDXGIDevice = nullptr;
    IDXGIAdapter* pDXGIAdapter = nullptr;
    IDXGIFactory* pFactory = nullptr;

    if (device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK)
        if (pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK)
            if (pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK)
            {
                bd->pd3dDevice = device;
                bd->pd3dDeviceContext = device_context;
                bd->pFactory = pFactory;
            }
    if (pDXGIDevice) pDXGIDevice->Release();
    if (pDXGIAdapter) pDXGIAdapter->Release();
    bd->pd3dDevice->AddRef();
    bd->pd3dDeviceContext->AddRef();

    ImGui_ImplDX11_InitMultiViewportSupport();

	CustomDraw::DX11RenderInit(bd);

    return true;
}

void ImGui_ImplDX11_Shutdown()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    CustomDraw::DX11ReleaseDeferedResources(bd);

    ImGui_ImplDX11_ShutdownMultiViewportSupport();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    if (bd->pFactory)             { bd->pFactory->Release(); }
    if (bd->pd3dDevice)           { bd->pd3dDevice->Release(); }
    if (bd->pd3dDeviceContext)    { bd->pd3dDeviceContext->Release(); }
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasViewports);
    IM_DELETE(bd);
}

void ImGui_ImplDX11_NewFrame()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX11_Init()?");

    if (!bd->pVertexShader)
        if (!ImGui_ImplDX11_CreateDeviceObjects())
            IM_ASSERT(0 && "ImGui_ImplDX11_CreateDeviceObjects() failed!");
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplDX11_ViewportData
{
    IDXGISwapChain*                 SwapChain;
    ID3D11RenderTargetView*         RTView;

    ImGui_ImplDX11_ViewportData()   { SwapChain = nullptr; RTView = nullptr; }
    ~ImGui_ImplDX11_ViewportData()  { IM_ASSERT(SwapChain == nullptr && RTView == nullptr); }
};

// Multi-Viewports: configure templates used when creating swapchains for secondary viewports. Will try them in order.
// This is intentionally not declared in the .h file yet, so you will need to copy this declaration:
void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC* desc_templates, int desc_templates_count);
void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC* desc_templates, int desc_templates_count)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    bd->SwapChainDescsForViewports.resize(desc_templates_count);
    memcpy(bd->SwapChainDescsForViewports.Data, desc_templates, sizeof(DXGI_SWAP_CHAIN_DESC));
}

static void ImGui_ImplDX11_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = IM_NEW(ImGui_ImplDX11_ViewportData)();
    viewport->RendererUserData = vd;

    // PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL's WindowID).
    // Some backends will leave PlatformHandleRaw == 0, in which case we assume PlatformHandle will contain the HWND.
    HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
    IM_ASSERT(hwnd != 0);
    IM_ASSERT(vd->SwapChain == nullptr && vd->RTView == nullptr);

    // Create swap chain
    HRESULT hr = DXGI_ERROR_UNSUPPORTED;
    for (const DXGI_SWAP_CHAIN_DESC& sd_template : bd->SwapChainDescsForViewports)
    {
        IM_ASSERT(sd_template.BufferDesc.Width == 0 && sd_template.BufferDesc.Height == 0 && sd_template.OutputWindow == nullptr);
        DXGI_SWAP_CHAIN_DESC sd = sd_template;
        sd.BufferDesc.Width = (UINT)viewport->Size.x;
        sd.BufferDesc.Height = (UINT)viewport->Size.y;
        sd.OutputWindow = hwnd;
        hr = bd->pFactory->CreateSwapChain(bd->pd3dDevice, &sd, &vd->SwapChain);
        if (SUCCEEDED(hr))
            break;
    }
    IM_ASSERT(SUCCEEDED(hr));

    // Create the render target
    if (vd->SwapChain != nullptr)
    {
        ID3D11Texture2D* pBackBuffer;
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_DestroyWindow(ImGuiViewport* viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == nullptr since we didn't create the data for it.
    if (ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData)
    {
        if (vd->SwapChain)
            vd->SwapChain->Release();
        vd->SwapChain = nullptr;
        if (vd->RTView)
            vd->RTView->Release();
        vd->RTView = nullptr;
        IM_DELETE(vd);
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_ImplDX11_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    if (vd->RTView)
    {
        vd->RTView->Release();
        vd->RTView = nullptr;
    }
    if (vd->SwapChain)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        vd->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer == nullptr) { fprintf(stderr, "ImGui_ImplDX11_SetWindowSize() failed creating buffers.\n"); return; }
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    bd->pd3dDeviceContext->OMSetRenderTargets(1, &vd->RTView, nullptr);
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
        bd->pd3dDeviceContext->ClearRenderTargetView(vd->RTView, (float*)&clear_color);
    ImGui_ImplDX11_RenderDrawData(viewport->DrawData);
}

static void ImGui_ImplDX11_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    vd->SwapChain->Present(0, 0); // Present without vsync
}

static void ImGui_ImplDX11_InitMultiViewportSupport()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_ImplDX11_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplDX11_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplDX11_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplDX11_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_ImplDX11_SwapBuffers;

    // Default swapchain format
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;
    ImGui_ImplDX11_SetSwapChainDescs(&sd, 1);
}

static void ImGui_ImplDX11_ShutdownMultiViewportSupport()
{
    ImGui::DestroyPlatformWindows();
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE

namespace CustomDraw
{
	struct DX11GPUTextureData
	{
		// NOTE: An ID of 0 denotes an empty slot
		u32 GenerationID;
		GPUTextureDesc Desc;
		ID3D11Texture2D* Texture2D;
		ID3D11ShaderResourceView* ResourceView;
	};

	// NOTE: First valid ID starts at 1
	static u32 LastTextureGenreationID;
	static std::vector<DX11GPUTextureData> LoadedTextureSlots;
	static std::vector<ID3D11DeviceChild*> DeviceResourcesToDeferRelease;

	inline DX11GPUTextureData* ResolveHandle(GPUTextureHandle handle)
	{
		auto& slots = LoadedTextureSlots;
		return (handle.GenerationID != 0) && (handle.SlotIndex < slots.size()) && (slots[handle.SlotIndex].GenerationID == handle.GenerationID) ? &slots[handle.SlotIndex] : nullptr;
	}

	void GPUTexture::Load(const GPUTextureDesc& desc)
	{
		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
		assert(ResolveHandle(Handle) == nullptr);

		DX11GPUTextureData* slot = nullptr;
		for (auto& it : LoadedTextureSlots) { if (it.GenerationID == 0) { slot = &it; break; } }
		if (slot == nullptr) { slot = &LoadedTextureSlots.emplace_back(); }

		slot->GenerationID = ++LastTextureGenreationID;
		slot->Desc = desc;

		HRESULT result = bd->pd3dDevice->CreateTexture2D(PtrArg(D3D11_TEXTURE2D_DESC
			{
				static_cast<UINT>(desc.Size.x), static_cast<UINT>(desc.Size.y), 1u, 1u,
				(desc.Format == GPUPixelFormat::RGBA) ? DXGI_FORMAT_R8G8B8A8_UNORM : (desc.Format == GPUPixelFormat::BGRA) ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_UNKNOWN,
				DXGI_SAMPLE_DESC { 1u, 0u },
				(desc.Access == GPUAccessType::Dynamic) ? D3D11_USAGE_DYNAMIC : (desc.Access == GPUAccessType::Static) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				(desc.Access == GPUAccessType::Dynamic) ? D3D11_CPU_ACCESS_WRITE : 0u, 0u
			}),
			PtrArg(D3D11_SUBRESOURCE_DATA{ desc.InitialPixels, static_cast<UINT>(desc.Size.x * 4), 0u }),
			//(desc.InitialPixels != nullptr) ? PtrArg(D3D11_SUBRESOURCE_DATA { desc.InitialPixels, static_cast<UINT>(desc.Size.x * 4), 0u }) : nullptr,
			&slot->Texture2D);
		assert(SUCCEEDED(result));

		result = bd->pd3dDevice->CreateShaderResourceView(slot->Texture2D, nullptr, &slot->ResourceView);
		assert(SUCCEEDED(result));

		Handle = GPUTextureHandle { static_cast<u32>(ArrayItToIndex(slot, &LoadedTextureSlots[0])), slot->GenerationID };
	}

	void GPUTexture::Unload()
	{
		if (auto* data = ResolveHandle(Handle); data != nullptr)
		{
			DeviceResourcesToDeferRelease.push_back(data->Texture2D);
			DeviceResourcesToDeferRelease.push_back(data->ResourceView);
			*data = DX11GPUTextureData {};
		}
		Handle = GPUTextureHandle {};
	}

	void GPUTexture::UpdateDynamic(ivec2 size, const void* newPixels)
	{
		auto* data = ResolveHandle(Handle);
		if (data == nullptr)
			return;

		assert(data->Desc.Access == GPUAccessType::Dynamic);
		assert(data->Desc.Size == size);
		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

		if (D3D11_MAPPED_SUBRESOURCE mapped; SUCCEEDED(bd->pd3dDeviceContext->Map(data->Texture2D, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			const size_t outStride = mapped.RowPitch;
			const size_t outByteSize = mapped.DepthPitch;
			u8* outData = static_cast<u8*>(mapped.pData);
			assert(outData != nullptr);

			assert(data->Desc.Format == GPUPixelFormat::RGBA || data->Desc.Format == GPUPixelFormat::BGRA);
			static constexpr u32 rgbaBitsPerPixel = (sizeof(u32) * BitsPerByte);
			const size_t inStride = (size.x * rgbaBitsPerPixel) / BitsPerByte;
			const size_t inByteSize = (size.y * inStride);
			const u8* inData = static_cast<const u8*>(newPixels);

			if (outByteSize == inByteSize)
			{
				memcpy(outData, inData, inByteSize);
			}
			else
			{
				assert(outByteSize == (outStride * size.x));
				for (size_t y = 0; y < size.y; y++)
					memcpy(&outData[outStride * y], &inData[inStride * y], inStride);
			}

			bd->pd3dDeviceContext->Unmap(data->Texture2D, 0);
		}
	}

	b8 GPUTexture::IsValid() const { return (ResolveHandle(Handle) != nullptr); }
	ivec2 GPUTexture::GetSize() const { auto* data = ResolveHandle(Handle); return data ? data->Desc.Size : ivec2(0, 0); }
	vec2 GPUTexture::GetSizeF32() const { return vec2(GetSize()); }
	GPUPixelFormat GPUTexture::GetFormat() const { auto* data = ResolveHandle(Handle); return data ? data->Desc.Format : GPUPixelFormat {}; }
	ImTextureID GPUTexture::GetTexID() const { auto* data = ResolveHandle(Handle); return data ? (ImTextureID)data->ResourceView : 0; }

	// NOTE: The most obvious way to extend this would be to either add an enum command type + a union of parameters
	//		 or (better?) a per command type commands vector with the render callback userdata storing a packed type+index
	struct DX11CustomDrawCommand
	{
		Rect Rect;
		ImVec4 Color;
		CustomDraw::WaveformChunk WaveformChunk;
	};

	static ImDrawData* ThisFrameImDrawData = nullptr;
	static std::vector<DX11CustomDrawCommand> CustomDrawCommandsThisFrame;

	static void DX11RenderInit(ImGui_ImplDX11_Data* bd)
	{
		CustomDrawCommandsThisFrame.reserve(64);
		LoadedTextureSlots.reserve(8);
		DeviceResourcesToDeferRelease.reserve(LoadedTextureSlots.capacity() * 2);
	}

	static void DX11BeginRenderDrawData(ImDrawData* drawData)
	{
		ThisFrameImDrawData = drawData;
	}

	static void DX11EndRenderDrawData(ImDrawData* drawData)
	{
		assert(drawData == ThisFrameImDrawData);
		ThisFrameImDrawData = nullptr;
		CustomDrawCommandsThisFrame.clear();
	}

	static void DX11ReleaseDeferedResources(ImGui_ImplDX11_Data* bd)
	{
		assert(bd != nullptr && bd->pd3dDevice != nullptr);

		if (!DeviceResourcesToDeferRelease.empty())
		{
			for (ID3D11DeviceChild* it : DeviceResourcesToDeferRelease) { if (it != nullptr) it->Release(); }
			DeviceResourcesToDeferRelease.clear();
		}
	}

	void DrawWaveformChunk(ImDrawList* drawList, Rect rect, u32 color, const WaveformChunk& chunk)
	{
		ImDrawCallback callback = [](const ImDrawList* parentList, const ImDrawCmd* cmd)
		{
			static_assert(sizeof(WAVEFORM_CONSTANT_BUFFER::Amplitudes) == sizeof(WaveformChunk::PerPixelAmplitude));
			const DX11CustomDrawCommand& customCommand = CustomDrawCommandsThisFrame[reinterpret_cast<size_t>(cmd->UserCallbackData)];

			ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
			ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;

			if (D3D11_MAPPED_SUBRESOURCE mapped; ctx->Map(bd->WaveformConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK)
			{
				constexpr vec2 uvTL = vec2(0.0f, 0.0f); constexpr vec2 uvTR = vec2(1.0f, 0.0f);
				constexpr vec2 uvBL = vec2(0.0f, 1.0f); constexpr vec2 uvBR = vec2(1.0f, 1.0f);

				WAVEFORM_CONSTANT_BUFFER* data = static_cast<WAVEFORM_CONSTANT_BUFFER*>(mapped.pData);
				data->PerVertex[0] = { customCommand.Rect.GetBL(), uvBL };
				data->PerVertex[1] = { customCommand.Rect.GetBR(), uvBR };
				data->PerVertex[2] = { customCommand.Rect.GetTR(), uvTR };
				data->PerVertex[3] = { customCommand.Rect.GetBL(), uvBL };
				data->PerVertex[4] = { customCommand.Rect.GetTR(), uvTR };
				data->PerVertex[5] = { customCommand.Rect.GetTL(), uvTL };
				data->CB_RectSize = { customCommand.Rect.GetSize(), vec2(1.0f) / customCommand.Rect.GetSize() };
				data->Color = { customCommand.Color.x, customCommand.Color.y, customCommand.Color.z, customCommand.Color.w };
				memcpy(&data->Amplitudes, &customCommand.WaveformChunk.PerPixelAmplitude, sizeof(data->Amplitudes));
				ctx->Unmap(bd->WaveformConstantBuffer, 0);
			}

			// HACK: Duplicated from regular render command loop
			ImVec2 clip_off = ThisFrameImDrawData->DisplayPos;
			ImVec2 clip_min(cmd->ClipRect.x - clip_off.x, cmd->ClipRect.y - clip_off.y);
			ImVec2 clip_max(cmd->ClipRect.z - clip_off.x, cmd->ClipRect.w - clip_off.y);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				return;

			const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
			ctx->RSSetScissorRects(1, &r);

			ctx->VSSetShader(bd->WaveformVS, nullptr, 0);
			ctx->PSSetShader(bd->WaveformPS, nullptr, 0);
			ctx->Draw(6, 0);

			// HACK: Always reset state for now even if it's immediately set back by the next render command
			ctx->VSSetShader(bd->pVertexShader, nullptr, 0);
			ctx->PSSetShader(bd->pPixelShader, nullptr, 0);
		};

		void* userData = reinterpret_cast<void*>(CustomDrawCommandsThisFrame.size());
		CustomDrawCommandsThisFrame.push_back(DX11CustomDrawCommand { rect, ImGui::ColorConvertU32ToFloat4(color), chunk });

		drawList->AddCallback(callback, userData);
	}
}
