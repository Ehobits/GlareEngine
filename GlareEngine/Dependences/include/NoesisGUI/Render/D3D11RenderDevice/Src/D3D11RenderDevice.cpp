////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


//#undef NS_MINIMUM_LOG_LEVEL
//#define NS_MINIMUM_LOG_LEVEL 0


#include "D3D11RenderDevice.h"

#include <NsCore/Log.h>
#include <NsCore/Ptr.h>
#include <NsCore/Memory.h>
#include <NsCore/StringUtils.h>
#include <NsRender/Texture.h>
#include <NsRender/RenderTarget.h>
#include <EASTL/algorithm.h>


using namespace Noesis;
using namespace NoesisApp;


#define DYNAMIC_VB_SIZE 512 * 1024
#define DYNAMIC_IB_SIZE 128 * 1024

#ifndef NS_PLATFORM_XBOX_ONE
    #define PREALLOCATED_DYNAMIC_PAGES 1
    #define VS_CBUFFER_SIZE 16 * sizeof(float)  // projectionMtx
    #define PS_CBUFFER_SIZE 12 * sizeof(float)  // rgba | radialGrad opacity | opacity
#endif

#define DX_RELEASE(o) \
    if (o != 0) \
    { \
        o->Release(); \
    }

#define DX_DESTROY(o) \
    if (o != 0) \
    { \
        ULONG refs = o->Release(); \
        NS_ASSERT(refs == 0); \
    }

#ifdef NS_PROFILE
    #define DX_BEGIN_EVENT(n) if (mGroupMarker != 0) { mGroupMarker->BeginEvent(n); }
    #define DX_END_EVENT() if (mGroupMarker != 0) { mGroupMarker->EndEvent(); }

    #ifndef NS_PLATFORM_XBOX_ONE
        static const GUID WKPDID_D3DDebugObjectName_ =
        {
            0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 }
        };

        static const int prueba = 10;

        static void SetDebugObjectName(ID3D11DeviceChild* resource, const char* str, ...)
        {
            char name[128];

            va_list args;
            va_start(args, str);
            vsnprintf(name, sizeof(name), str, args);
            va_end(args);

            resource->SetPrivateData(WKPDID_D3DDebugObjectName_, (UINT)strlen(name), name);
        }

        #define DX_NAME(resource, ...) SetDebugObjectName(resource, __VA_ARGS__)
    #endif
#else
    #define DX_BEGIN_EVENT(n) NS_NOOP
    #define DX_END_EVENT() NS_NOOP
    #define DX_NAME(...) NS_UNUSED(__VA_ARGS__)
#endif

#ifdef NS_DEBUG
    #define V(exp) \
        NS_MACRO_BEGIN \
            HRESULT hr_ = (exp); \
            if (FAILED(hr_)) \
            { \
                NS_FATAL("%s[0x%08x]", #exp, hr_); \
            } \
        NS_MACRO_END
#elif defined(_PREFAST_)
    #define V(exp) \
        NS_MACRO_BEGIN \
            HRESULT hr_ = (exp); \
            __analysis_assume(!FAILED(hr_)); \
        NS_MACRO_END
#else
    #define V(exp) exp
#endif

namespace
{

#ifndef NS_PLATFORM_XBOX_ONE
    #include "Shaders.h"
#endif

const uint32_t VFPos = 0;
const uint32_t VFColor = 1;
const uint32_t VFTex0 = 2;
const uint32_t VFTex1 = 4;
const uint32_t VFCoverage = 8;

struct Program
{
    int8_t vShaderIdx;
    int8_t pShaderIdx;
};

// Map from batch shader-ID to vertex and pixel shader objects
const Program Programs[] =
{
    { 0, 0 },    // RGBA
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 0, 1 },    // Mask
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 1, 2 },    // PathSolid
    { 2, 3 },    // PathLinear
    { 2, 4 },    // PathRadial
    { 2, 5 },    // PathPattern
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 3, 6 },    // PathAASolid
    { 4, 7 },    // PathAALinear
    { 4, 8 },    // PathAARadial
    { 4, 9 },    // PathAAPattern
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 5, 10 },   // ImagePaintOpacitySolid
    { 6, 11 },   // ImagePaintOpacityLinear
    { 6, 12 },   // ImagePaintOpacityRadial
    { 6, 13 },   // ImagePaintOpacityPattern
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 5, 14 },   // TextSolid
    { 6, 15 },   // TextLinear
    { 6, 16 },   // TextRadial
    { 6, 17 },   // TextPattern
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class D3D11Texture final: public Texture
{
public:
    D3D11Texture(ID3D11ShaderResourceView* view_, uint32_t width_, uint32_t height_,
        uint32_t levels_, bool isInverted_, void* base_): view(view_), width(width_),
        height(height_), levels(levels_), isInverted(isInverted_), base(base_) {}

    ~D3D11Texture()
    {
        DX_DESTROY(view);
    }

    uint32_t GetWidth() const override { return width; }
    uint32_t GetHeight() const override { return height; }
    bool HasMipMaps() const override { return levels > 1; }
    bool IsInverted() const override { return isInverted; }

    ID3D11ShaderResourceView* view;

    const uint32_t width;
    const uint32_t height;
    const uint32_t levels;
    const bool isInverted;
    void* const base;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class D3D11RenderTarget final: public RenderTarget
{
public:
    D3D11RenderTarget(uint32_t width_, uint32_t height_, MSAA::Enum msaa_): width(width_),
        height(height_), msaa(msaa_), textureRTV(0), color(0), colorRTV(0), colorSRV(0),
        stencil(0), stencilDSV(0) {}

    ~D3D11RenderTarget()
    {
        DX_DESTROY(textureRTV);

        DX_RELEASE(color);
        DX_DESTROY(colorRTV);
        DX_DESTROY(colorSRV);

        DX_RELEASE(stencil);
        DX_DESTROY(stencilDSV);

        texture.Reset();
    }

    Texture* GetTexture() override { return texture; }

    const uint32_t width;
    const uint32_t height;
    const MSAA::Enum msaa;

    Ptr<D3D11Texture> texture;
    ID3D11RenderTargetView* textureRTV;

    ID3D11Texture2D* color;
    ID3D11RenderTargetView* colorRTV;
    ID3D11ShaderResourceView* colorSRV;

    ID3D11Texture2D* stencil;
    ID3D11DepthStencilView* stencilDSV;
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
D3D11RenderDevice::D3D11RenderDevice(ID3D11DeviceContext* context, bool sRGB): mContext(context)
{
    mContext->GetDevice(&mDevice);
    mContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&mGroupMarker);

    FillCaps(sRGB);

    CreateBuffers();
    CreateStateObjects();
    CreateShaders();

    InvalidateStateCache();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
D3D11RenderDevice::~D3D11RenderDevice()
{
    DestroyStateObjects();
    DestroyShaders();
    DestroyBuffers();

    DX_RELEASE(mGroupMarker);
    DX_RELEASE(mDevice);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Texture> D3D11RenderDevice::WrapTexture(ID3D11Texture2D* texture, uint32_t width,
    uint32_t height, uint32_t levels, bool isInverted)
{
    NS_ASSERT(texture != 0);

    if (texture != 0)
    {
        ID3D11Device* device;
        texture->GetDevice(&device);

        D3D11_TEXTURE2D_DESC textureDesc;
        texture->GetDesc(&textureDesc);

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipLevels = unsigned int(-1);
        viewDesc.Texture2D.MostDetailedMip = 0;

        switch (textureDesc.Format)
        {
            case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
                break;
            }
            case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
                break;
            }
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            }
            case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
                break;
            }
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;
            }
            case DXGI_FORMAT_BC1_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC1_UNORM;
                break;
            }
            case DXGI_FORMAT_BC2_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC2_UNORM;
                break;
            }
            case DXGI_FORMAT_BC3_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC3_UNORM;
                break;
            }
            case DXGI_FORMAT_BC4_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC4_UNORM;
                break;
            }
            case DXGI_FORMAT_BC5_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC5_UNORM;
                break;
            }
            case DXGI_FORMAT_BC7_TYPELESS:
            {
                viewDesc.Format = DXGI_FORMAT_BC7_UNORM;
                break;
            }
            default:
            {
                viewDesc.Format = textureDesc.Format;
                break;
            }
        }

        ID3D11ShaderResourceView* view;
        V(device->CreateShaderResourceView(texture, &viewDesc, &view));
        DX_RELEASE(device);

        return *new D3D11Texture(view, width, height, levels, isInverted, nullptr);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DeviceCaps& D3D11RenderDevice::GetCaps() const
{
    return mCaps;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static MSAA::Enum ToMSAA(uint32_t sampleCount)
{
    uint32_t samples = eastl::max_alt(1U, eastl::min_alt(sampleCount, 16U));

    MSAA::Enum mssa = MSAA::x1;
    while (samples >>= 1)
    {
        mssa = (MSAA::Enum)((uint32_t)mssa + 1);
    }

    return mssa;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<RenderTarget> D3D11RenderDevice::CreateRenderTarget(const char* label, uint32_t width,
    uint32_t height, uint32_t sampleCount)
{
    MSAA::Enum msaa = ToMSAA(mSampleDescs[(uint32_t)ToMSAA(sampleCount)].Count);

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc = mSampleDescs[(uint32_t)msaa];
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* colorAA = 0;

    if (msaa != MSAA::x1)
    {
        if (mCaps.linearRendering)
        {
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        }
        else
        {
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        V(mDevice->CreateTexture2D(&desc, 0, &colorAA));
        DX_NAME(colorAA, "%s", label);
    }

    ID3D11Texture2D* stencil;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

#ifndef NS_PLATFORM_XBOX_ONE
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif

    V(mDevice->CreateTexture2D(&desc, 0, &stencil));
    DX_NAME(stencil, "%s_Stencil", label);

    return CreateRenderTarget(label, width, height, msaa, colorAA, stencil);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<RenderTarget> D3D11RenderDevice::CloneRenderTarget(const char* label, RenderTarget* surface_)
{
    D3D11RenderTarget* surface = (D3D11RenderTarget*)surface_;

    ID3D11Texture2D* colorAA = 0;
    if (surface->msaa != MSAA::x1)
    {
        colorAA = surface->color;
        colorAA->AddRef();
    }

    ID3D11Texture2D* stencil = surface->stencil;
    stencil->AddRef();

    uint32_t width = surface->width;
    uint32_t height = surface->height;
    return CreateRenderTarget(label, width, height, surface->msaa, colorAA, stencil);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Texture> D3D11RenderDevice::CreateTexture(const char* label, uint32_t width, uint32_t height,
    uint32_t numLevels, TextureFormat::Enum format_)
{
    NS_ASSERT(format_ == TextureFormat::RGBA8 || format_ == TextureFormat::R8);
    DXGI_FORMAT format = format_ == TextureFormat::R8 ? DXGI_FORMAT_R8_UNORM :
        mCaps.linearRendering ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    void* base = nullptr;

#ifndef NS_PLATFORM_XBOX_ONE
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = numLevels;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* tex;
    V(mDevice->CreateTexture2D(&desc, 0, &tex));
#endif

    DX_NAME(tex, label);
    NS_LOG_TRACE("Texture '%s' %d x %d x %d (%s)", label, width, height, numLevels,
        [](DXGI_FORMAT format)
        {
            switch (format)
            {
                case DXGI_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8";
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_SRGB";
                case DXGI_FORMAT_R8_UNORM: return "R8";
                default: return "";
            }
        }((DXGI_FORMAT)desc.Format));

    ID3D11ShaderResourceView* view;
    V(mDevice->CreateShaderResourceView(tex, 0, &view));
    DX_NAME(view, "%s_SRV", label);
    DX_RELEASE(tex);

    return *new D3D11Texture(view, width, height, numLevels, false, base);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::UpdateTexture(Texture* texture_, uint32_t level, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, const void* data)
{
    NS_ASSERT(level == 0);
    D3D11Texture* texture = (D3D11Texture*)texture_;

    ID3D11Resource* resource;
    texture->view->GetResource(&resource);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    texture->view->GetDesc(&desc);

#ifndef NS_PLATFORM_XBOX_ONE
    D3D11_BOX box;
    box.left = x; 
    box.top = y;
    box.front = 0;
    box.right = x + width;
    box.bottom = y + height;
    box.back = 1;

    unsigned int pitch = desc.Format == DXGI_FORMAT_R8_UNORM ? width : width * 4;
    mContext->UpdateSubresource(resource, 0, &box, data, pitch, 0);
#endif

    DX_RELEASE(resource);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::BeginRender(bool offscreen)
{
    NS_UNUSED(offscreen);
    DX_BEGIN_EVENT(offscreen ? L"Noesis.Offscreen": L"Noesis");
    InvalidateStateCache();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetRenderTarget(RenderTarget* surface_)
{
    DX_BEGIN_EVENT(L"SetRenderTarget");

    ClearTextures();
    D3D11RenderTarget* surface = (D3D11RenderTarget*)surface_;
    mContext->OMSetRenderTargets(1, &surface->colorRTV, surface->stencilDSV);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (FLOAT)surface->width;
    viewport.Height = (FLOAT)surface->height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    mContext->RSSetViewports(1, &viewport);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::BeginTile(const Tile& tile, uint32_t surfaceWidth, uint32_t surfaceHeight)
{
    NS_UNUSED(surfaceWidth);
    DX_BEGIN_EVENT(L"Tile");

    uint32_t x = tile.x;
    uint32_t y = (uint32_t)surfaceHeight - (tile.y + tile.height);

    D3D11_RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + tile.width;
    rect.bottom = y + tile.height;
    mContext->RSSetScissorRects(1, &rect);

    ClearRenderTarget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::EndTile()
{
    DX_END_EVENT();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::ResolveRenderTarget(RenderTarget* surface_, const Tile* tiles, uint32_t size)
{
    D3D11RenderTarget* surface = (D3D11RenderTarget*)surface_;

    if (surface->msaa != MSAA::x1)
    {
        DX_BEGIN_EVENT(L"Resolve");

        SetInputLayout(0);
        SetVertexShader(mQuadVS);
        NS_ASSERT(surface->msaa - 1 < NS_COUNTOF(mResolvePS));
        SetPixelShader(mResolvePS[surface->msaa - 1]);

        SetRasterizerState(mRasterizerStates[2]);
        SetBlendState(mBlendStates[2]);
        SetDepthStencilState(mDepthStencilStates[0], 0);

        ClearTextures();
        mContext->OMSetRenderTargets(1, &surface->textureRTV, 0);
        SetTexture(0, surface->colorSRV);

        for (uint32_t i = 0; i < size; i++)
        {
            const Tile& tile = tiles[i];

            D3D11_RECT rect;
            rect.left = tile.x;
            rect.top = surface->height - (tile.y + tile.height);
            rect.right = tile.x + tile.width;
            rect.bottom = surface->height - tile.y;
            mContext->RSSetScissorRects(1, &rect);

            mContext->Draw(3, 0);
        }

        DX_END_EVENT();
    }

    DX_END_EVENT();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::EndRender()
{
    DX_END_EVENT();

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* D3D11RenderDevice::MapVertices(uint32_t bytes)
{
    return MapBuffer(mVertices, bytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::UnmapVertices()
{
    UnmapBuffer(mVertices);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* D3D11RenderDevice::MapIndices(uint32_t bytes)
{
    return MapBuffer(mIndices, bytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::UnmapIndices()
{
    UnmapBuffer(mIndices);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::DrawBatch(const Batch& batch)
{
    NS_ASSERT(batch.shader.v < NS_COUNTOF(Programs));
    NS_ASSERT(Programs[batch.shader.v].vShaderIdx != -1);
    NS_ASSERT(Programs[batch.shader.v].pShaderIdx != -1);
    NS_ASSERT(Programs[batch.shader.v].vShaderIdx < NS_COUNTOF(mVertexStages));
    NS_ASSERT(Programs[batch.shader.v].pShaderIdx < NS_COUNTOF(mPixelShaders));

    SetShaders(batch);
    SetBuffers(batch);
    SetRenderState(batch);
    SetTextures(batch);

    mContext->DrawIndexed(batch.numIndices, batch.startIndex + mIndices.drawPos / 2, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
D3D11RenderDevice::Page* D3D11RenderDevice::AllocatePage(DynamicBuffer& buffer)
{
    NS_ASSERT(buffer.numPages < NS_COUNTOF(buffer.pages));
    Page* page = &buffer.pages[buffer.numPages];

#ifndef NS_PLATFORM_XBOX_ONE
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = buffer.size;
    desc.BindFlags = buffer.flags;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    V(mDevice->CreateBuffer(&desc, 0, &page->buffer));
#endif

    DX_NAME(page->buffer, "Noesis::%s[%d]", buffer.label, buffer.numPages);
    NS_LOG_TRACE("Page '%s[%d]' created (%d KB)", buffer.label, buffer.numPages, buffer.size);

    buffer.numPages++;
    return page;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CreateBuffer(DynamicBuffer& buffer, uint32_t size, D3D11_BIND_FLAG flags,
    const char* label)
{
    buffer.size = size;
    buffer.pos = 0;
    buffer.drawPos = 0;
    buffer.flags = flags;
    buffer.label = label;

    buffer.numPages = 0;
    memset(buffer.pages, 0, sizeof(buffer.pages));

    buffer.pendingPages = nullptr;
    buffer.freePages = nullptr;

    for (uint32_t i = 0; i < PREALLOCATED_DYNAMIC_PAGES; i++)
    {
        Page* page = AllocatePage(buffer);
        page->next = buffer.freePages;
        buffer.freePages = page;
    }

    NS_ASSERT(buffer.freePages != nullptr);
    buffer.currentPage = buffer.freePages;
    buffer.freePages = buffer.freePages->next;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CreateBuffers()
{
    CreateBuffer(mVertices, DYNAMIC_VB_SIZE, D3D11_BIND_VERTEX_BUFFER, "Vertices");
    CreateBuffer(mIndices, DYNAMIC_IB_SIZE, D3D11_BIND_INDEX_BUFFER, "Indices");
    CreateBuffer(mVertexCB, VS_CBUFFER_SIZE, D3D11_BIND_CONSTANT_BUFFER, "VertexCB");
    CreateBuffer(mPixelCB, PS_CBUFFER_SIZE, D3D11_BIND_CONSTANT_BUFFER, "PixelCB");

    mVertexCBHash = 0;
    mPixelCBHash = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::DestroyBuffer(DynamicBuffer& buffer)
{
    for (uint32_t i = 0; i < buffer.numPages; i++)
    {
        DX_DESTROY(buffer.pages[i].buffer);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::DestroyBuffers()
{
    DestroyBuffer(mVertices);
    DestroyBuffer(mIndices);
    DestroyBuffer(mVertexCB);
    DestroyBuffer(mPixelCB);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* D3D11RenderDevice::MapBuffer(DynamicBuffer& buffer, uint32_t size)
{
    NS_ASSERT(size <= buffer.size);

#ifndef NS_PLATFORM_XBOX_ONE
    D3D11_MAP type;

    // Make sure constant buffers are always discarded (no_overwrite not suported in Win7)
    if (buffer.pos == 0 || buffer.pos + size > buffer.size)
    {
        type = D3D11_MAP_WRITE_DISCARD;
        buffer.pos = 0;
    }
    else
    {
        type = D3D11_MAP_WRITE_NO_OVERWRITE;
    }

    buffer.drawPos = buffer.pos;
    buffer.pos += size;

    D3D11_MAPPED_SUBRESOURCE mapped;
    V(mContext->Map(buffer.currentPage->buffer, 0, type, 0, &mapped));
    return (uint8_t*)mapped.pData + buffer.drawPos;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::UnmapBuffer(DynamicBuffer& buffer) const
{
#ifndef NS_PLATFORM_XBOX_ONE
    mContext->Unmap(buffer.currentPage->buffer, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CreateVertexStage(uint8_t format, const char* label, const void* code,
    uint32_t size, VertexStage& stage)
{
    // Vertex Shader
    V(mDevice->CreateVertexShader(code, size, 0, &stage.shader));
    DX_NAME(stage.shader, "Noesis::%s", label);

    // Input Layout
    stage.stride = 0;
    uint32_t element = 0;

    D3D11_INPUT_ELEMENT_DESC descs[5];

    descs[element].SemanticIndex = 0;
    descs[element].InputSlot = 0;
    descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    descs[element].InstanceDataStepRate = 0;
    descs[element].SemanticName = "POSITION";
    descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
    stage.stride += 8;
    element++;

    if (format & VFColor)
    {
        descs[element].SemanticIndex = 0;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "COLOR";
        descs[element].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        stage.stride += 4;
        element++;
    }

    if (format & VFTex0)
    {
        descs[element].SemanticIndex = 0;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        stage.stride += 8;
        element++;
    }

    if (format & VFTex1)
    {
        descs[element].SemanticIndex = 1;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        stage.stride += 8;
        element++;
    }

    if (format & VFCoverage)
    {
        descs[element].SemanticIndex = 2;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32_FLOAT;
        stage.stride += 4;
        element++;
    }

    V(mDevice->CreateInputLayout(descs, element, code, size, &stage.layout));
    DX_NAME(stage.layout, "Noesis::%sL", label);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static D3D11_FILTER ToD3D(MinMagFilter::Enum minmagFilter, MipFilter::Enum mipFilter)
{
    switch (minmagFilter)
    {
        case MinMagFilter::Nearest:
        {
            switch (mipFilter)
            {
                case MipFilter::Disabled:
                {
                    // TODO: set MaxLOD to 0
                    return D3D11_FILTER_MIN_MAG_MIP_POINT;
                }
                case MipFilter::Nearest:
                {
                    return D3D11_FILTER_MIN_MAG_MIP_POINT;
                }
                case MipFilter::Linear:
                {
                    return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                }
                default:
                {
                    NS_ASSERT_UNREACHABLE;
                }
            }
        }
        case MinMagFilter::Linear:
        {
            switch (mipFilter)
            {
                case MipFilter::Disabled:
                {
                    // TODO: set MaxLOD to 0
                    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                }
                case MipFilter::Nearest:
                {
                    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                }
                case MipFilter::Linear:
                {
                    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                }
                default:
                {
                    NS_ASSERT_UNREACHABLE;
                }
            }
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void ToD3D(WrapMode::Enum mode, D3D_FEATURE_LEVEL featureLevel,
    D3D11_TEXTURE_ADDRESS_MODE& addressU, D3D11_TEXTURE_ADDRESS_MODE& addressV)
{
    switch (mode)
    {
        case WrapMode::ClampToEdge:
        {
            addressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            addressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        }
        case WrapMode::ClampToZero:
        {
            bool hasBorder = featureLevel >= D3D_FEATURE_LEVEL_9_3;
            addressU = hasBorder? D3D11_TEXTURE_ADDRESS_BORDER : D3D11_TEXTURE_ADDRESS_CLAMP;
            addressV = hasBorder? D3D11_TEXTURE_ADDRESS_BORDER : D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        }
        case WrapMode::Repeat:
        {
            addressU = D3D11_TEXTURE_ADDRESS_WRAP;
            addressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        }
        case WrapMode::MirrorU:
        {
            addressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            addressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        }
        case WrapMode::MirrorV:
        {
            addressU = D3D11_TEXTURE_ADDRESS_WRAP;
            addressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
        }
        case WrapMode::Mirror:
        {
            addressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            addressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CreateStateObjects()
{
    // Rasterized states
    {
        D3D11_RASTERIZER_DESC desc;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = false;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.MultisampleEnable = true;
        desc.AntialiasedLineEnable = false;

        desc.FillMode = D3D11_FILL_SOLID;
        desc.ScissorEnable = false;
        V(mDevice->CreateRasterizerState(&desc, &mRasterizerStates[0]));

        desc.FillMode = D3D11_FILL_WIREFRAME;
        desc.ScissorEnable = false;
        V(mDevice->CreateRasterizerState(&desc, &mRasterizerStates[1]));

        desc.FillMode = D3D11_FILL_SOLID;
        desc.ScissorEnable = true;
        V(mDevice->CreateRasterizerState(&desc, &mRasterizerStates[2]));

        desc.FillMode = D3D11_FILL_WIREFRAME;
        desc.ScissorEnable = true;
        V(mDevice->CreateRasterizerState(&desc, &mRasterizerStates[3]));
    }

    // Blend states
    {
        D3D11_BLEND_DESC desc;
        desc.AlphaToCoverageEnable = false;
        desc.IndependentBlendEnable = false;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
        V(mDevice->CreateBlendState(&desc, &mBlendStates[0]));

        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
        V(mDevice->CreateBlendState(&desc, &mBlendStates[1]));

        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        V(mDevice->CreateBlendState(&desc, &mBlendStates[2]));

        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        V(mDevice->CreateBlendState(&desc, &mBlendStates[3]));
    }

    // Depth states
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_NEVER;
        desc.StencilReadMask = 0xff;
        desc.StencilWriteMask = 0xff;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

        // Disabled
        desc.StencilEnable = false;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        V(mDevice->CreateDepthStencilState(&desc, &mDepthStencilStates[0]));

        // Equal_Keep
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        V(mDevice->CreateDepthStencilState(&desc, &mDepthStencilStates[1]));

        // Equal_Incr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        V(mDevice->CreateDepthStencilState(&desc, &mDepthStencilStates[2]));

        // Equal_Decr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        V(mDevice->CreateDepthStencilState(&desc, &mDepthStencilStates[3]));

        // Zero
        desc.StencilEnable = true;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        V(mDevice->CreateDepthStencilState(&desc, &mDepthStencilStates[4]));
    }

    // Sampler states
    {
        memset(mSamplerStates, 0, sizeof(mSamplerStates));

        D3D11_SAMPLER_DESC desc = {};
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        D3D_FEATURE_LEVEL featureLevel = mDevice->GetFeatureLevel();

        for (uint8_t minmag = 0; minmag < MinMagFilter::Count; minmag++)
        {
            for (uint8_t mip = 0; mip < MipFilter::Count; mip++)
            {
                desc.Filter = ToD3D(MinMagFilter::Enum(minmag), MipFilter::Enum(mip));

                for (uint8_t uv = 0; uv < WrapMode::Count; uv++)
                {    
                    ToD3D(WrapMode::Enum(uv), featureLevel, desc.AddressU, desc.AddressV);

                    SamplerState s = {{uv, minmag, mip}};
                    NS_ASSERT(s.v < NS_COUNTOF(mSamplerStates));
                    V(mDevice->CreateSamplerState(&desc, &mSamplerStates[s.v]));
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::DestroyStateObjects()
{
    for (uint32_t i = 0; i < NS_COUNTOF(mRasterizerStates); i++)
    {
        DX_RELEASE(mRasterizerStates[i]);
    }

    for (uint32_t i = 0; i < NS_COUNTOF(mBlendStates); i++)
    {
        DX_RELEASE(mBlendStates[i]);
    }

    for (uint32_t i = 0; i < NS_COUNTOF(mDepthStencilStates); i++)
    {
        DX_RELEASE(mDepthStencilStates[i]);
    }

    for (uint32_t i = 0; i < NS_COUNTOF(mSamplerStates); i++)
    {
        DX_RELEASE(mSamplerStates[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CreateShaders()
{
#define SHADER(n) {#n, n, sizeof(n)}

    struct Shader
    {
        const char* label;
        const BYTE* code;
        uint32_t size;
    };

    const Shader pShaders[] =
    {
        SHADER(RGBA_FS), 
        SHADER(Mask_FS), 
        SHADER(PathSolid_FS), 
        SHADER(PathLinear_FS), 
        SHADER(PathRadial_FS), 
        SHADER(PathPattern_FS), 
        SHADER(PathAASolid_FS), 
        SHADER(PathAALinear_FS), 
        SHADER(PathAARadial_FS), 
        SHADER(PathAAPattern_FS), 
        SHADER(ImagePaintOpacitySolid_FS), 
        SHADER(ImagePaintOpacityLinear_FS), 
        SHADER(ImagePaintOpacityRadial_FS), 
        SHADER(ImagePaintOpacityPattern_FS), 
        SHADER(TextSolid_FS), 
        SHADER(TextLinear_FS), 
        SHADER(TextRadial_FS), 
        SHADER(TextPattern_FS)
    };

    const Shader vShaders[] =
    {
        SHADER(Pos_VS),
        SHADER(PosColor_VS),
        SHADER(PosTex0_VS),
        SHADER(PosColorCoverage_VS),
        SHADER(PosTex0Coverage_VS),
        SHADER(PosColorTex1_VS),
        SHADER(PosTex0Tex1_VS)
    };

    const uint8_t formats[] = 
    {
        VFPos,
        VFPos | VFColor,
        VFPos | VFTex0,
        VFPos | VFColor | VFCoverage,
        VFPos | VFTex0 | VFCoverage,
        VFPos | VFColor | VFTex1,
        VFPos | VFTex0 | VFTex1
    };

    static_assert(NS_COUNTOF(vShaders) == NS_COUNTOF(mVertexStages), "");

    for (uint32_t i = 0; i < NS_COUNTOF(vShaders); i++)
    {
        const Shader& shader = vShaders[i];
        CreateVertexStage(formats[i], shader.label, shader.code, shader.size, mVertexStages[i]);
    }

    static_assert(NS_COUNTOF(pShaders) == NS_COUNTOF(mPixelShaders), "");

    for (uint32_t i = 0; i < NS_COUNTOF(pShaders); i++)
    {
        const Shader& shader = pShaders[i];
        V(mDevice->CreatePixelShader(shader.code, shader.size, 0, &mPixelShaders[i]));
        DX_NAME(mPixelShaders[i], "Noesis::%s", shader.label);
    }

    V(mDevice->CreateVertexShader(Quad_VS, sizeof(Quad_VS), 0, &mQuadVS));
    DX_NAME(mQuadVS, "Noesis::Quad_VS");

    V(mDevice->CreatePixelShader(Resolve2_PS, sizeof(Resolve2_PS), 0, &mResolvePS[0]));
    DX_NAME(mResolvePS[0], "Noesis::Resolve2_PS");

    V(mDevice->CreatePixelShader(Resolve4_PS, sizeof(Resolve4_PS), 0, &mResolvePS[1]));
    DX_NAME(mResolvePS[1], "Noesis::Resolve4_PS");

    V(mDevice->CreatePixelShader(Resolve8_PS, sizeof(Resolve8_PS), 0, &mResolvePS[2]));
    DX_NAME(mResolvePS[2], "Noesis::Resolve8_PS");

    V(mDevice->CreatePixelShader(Resolve16_PS, sizeof(Resolve16_PS), 0, &mResolvePS[3]));
    DX_NAME(mResolvePS[3], "Noesis::Resolve16_PS");

    V(mDevice->CreatePixelShader(Clear_PS, sizeof(Clear_PS), 0, &mClearPS));
    DX_NAME(mClearPS, "Noesis::Clear_PS");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::DestroyShaders()
{
    for (uint32_t i = 0; i < NS_COUNTOF(mVertexStages); i++)
    {
        DX_DESTROY(mVertexStages[i].layout);
        DX_DESTROY(mVertexStages[i].shader);
    }

    for (uint32_t i = 0; i < NS_COUNTOF(mPixelShaders); i++)
    {
        DX_DESTROY(mPixelShaders[i]);
    }

    for (uint32_t i = 0; i < NS_COUNTOF(mResolvePS); i++)
    {
        DX_DESTROY(mResolvePS[i]);
    }

    DX_DESTROY(mQuadVS);
    DX_DESTROY(mClearPS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<RenderTarget> D3D11RenderDevice::CreateRenderTarget(const char* label, uint32_t width,
    uint32_t height, MSAA::Enum msaa, ID3D11Texture2D* colorAA, ID3D11Texture2D* stencil)
{
    Ptr<D3D11RenderTarget> surface = *new D3D11RenderTarget(width, height, msaa);

    NS_LOG_TRACE("RenderTarget '%s' %d x %d %dx", label, width, height, 1 << msaa);
    bool sRGB = mCaps.linearRendering;

    // Texture
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* colorTex;
    V(mDevice->CreateTexture2D(&desc, 0, &colorTex));
    DX_NAME(colorTex, "%s_TEX", label);

    ID3D11ShaderResourceView* viewTex;
    V(mDevice->CreateShaderResourceView(colorTex, 0, &viewTex));
    DX_NAME(viewTex, "%s_TEX_SRV", label);
    DX_RELEASE(colorTex);

    surface->texture = *new D3D11Texture(viewTex, width, height, 1, false, nullptr);

    V(mDevice->CreateRenderTargetView(colorTex, 0, &surface->textureRTV));
    DX_NAME(surface->textureRTV, "%s_TEX_RTV", label);

    // Color
    if (colorAA != 0)
    {
        NS_ASSERT(msaa != MSAA::x1);
        surface->color = colorAA;
    }
    else
    {
        NS_ASSERT(msaa == MSAA::x1);
        surface->color = colorTex;
        surface->color->AddRef();
    }

    V(mDevice->CreateRenderTargetView(surface->color, 0, &surface->colorRTV));
    DX_NAME(surface->colorRTV, "%s_RTV", label);

    V(mDevice->CreateShaderResourceView(surface->color, 0, &surface->colorSRV));
    DX_NAME(surface->colorSRV, "%s_SRV", label);

    // Stencil
    surface->stencil = stencil;

    V(mDevice->CreateDepthStencilView(surface->stencil, 0, &surface->stencilDSV));
    DX_NAME(surface->stencilDSV, "%s_DSV", label);

    return surface;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::CheckMultisample()
{
    NS_ASSERT(mDevice != 0);

    unsigned int counts[MSAA::Count] = {1, 2, 4, 8, 16};

    for (uint32_t i = 0, last = 0; i < NS_COUNTOF(counts); i++)
    {
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        unsigned int count = counts[i];
        unsigned int quality = 0;

        HRESULT hr = mDevice->CheckMultisampleQualityLevels(format, count, &quality);

        if (SUCCEEDED(hr) && quality > 0)
        {
            mSampleDescs[i].Count = count;
            mSampleDescs[i].Quality = 0;
            last = i;
        }
        else
        {
            mSampleDescs[i] = mSampleDescs[last];
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::FillCaps(bool sRGB)
{
    CheckMultisample();

    mCaps = {};
    mCaps.centerPixelOffset = 0.0f;
    mCaps.dynamicVerticesSize = DYNAMIC_VB_SIZE;
    mCaps.dynamicIndicesSize = DYNAMIC_IB_SIZE;
    mCaps.linearRendering = sRGB;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::InvalidateStateCache()
{
    SetVSConstantBuffer(mVertexCB);
    SetPSConstantBuffer(mPixelCB);

    mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mContext->HSSetShader(nullptr, nullptr, 0);
    mContext->DSSetShader(nullptr, nullptr, 0);
    mContext->GSSetShader(nullptr, nullptr, 0);
    mContext->CSSetShader(nullptr, nullptr, 0);
    mContext->SetPredication(nullptr, 0);

    mIndexBuffer = nullptr;
    mLayout = nullptr;
    mVertexShader = nullptr;
    mPixelShader = nullptr;
    mRasterizerState = nullptr;
    mBlendState = nullptr;
    mDepthStencilState = nullptr;
    mStencilRef = (unsigned int)-1;
    memset(mTextures, 0, sizeof(mTextures));
    memset(mSamplers, 0, sizeof(mSamplers));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetIndexBuffer(ID3D11Buffer* buffer)
{
    if (mIndexBuffer != buffer)
    {
#ifndef NS_PLATFORM_XBOX_ONE
        mContext->IASetIndexBuffer(buffer, DXGI_FORMAT_R16_UINT, 0);
#endif

        mIndexBuffer = buffer;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetVertexBuffer(ID3D11Buffer* buffer, uint32_t stride, uint32_t offset)
{
#ifndef NS_PLATFORM_XBOX_ONE
    mContext->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetVSConstantBuffer(DynamicBuffer& buffer)
{
    Page* page = buffer.currentPage;

#ifndef NS_PLATFORM_XBOX_ONE
    mContext->VSSetConstantBuffers(0, 1, &page->buffer);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetPSConstantBuffer(DynamicBuffer& buffer)
{
    Page* page = buffer.currentPage;

#ifndef NS_PLATFORM_XBOX_ONE
    mContext->PSSetConstantBuffers(0, 1, &page->buffer);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetInputLayout(ID3D11InputLayout* layout)
{
    if (layout != mLayout)
    {
        mContext->IASetInputLayout(layout);
        mLayout = layout;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetVertexShader(ID3D11VertexShader* shader)
{
    if (shader != mVertexShader)
    {
        mContext->VSSetShader(shader, 0, 0);
        mVertexShader = shader;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetPixelShader(ID3D11PixelShader* shader)
{
    if (shader != mPixelShader)
    {
        mContext->PSSetShader(shader, 0, 0);
        mPixelShader = shader;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetRasterizerState(ID3D11RasterizerState* state)
{
    if (state != mRasterizerState)
    {
        mContext->RSSetState(state);
        mRasterizerState = state;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetBlendState(ID3D11BlendState* state)
{
    if (state != mBlendState)
    {
        mContext->OMSetBlendState(state, 0, 0xffffffff);
        mBlendState = state;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetDepthStencilState(ID3D11DepthStencilState* state, unsigned int stencilRef)
{
    if (state != mDepthStencilState || stencilRef != mStencilRef)
    {
        mContext->OMSetDepthStencilState(state, stencilRef);
        mDepthStencilState = state;
        mStencilRef = stencilRef;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetSampler(unsigned int slot, ID3D11SamplerState* sampler)
{
    NS_ASSERT(slot < NS_COUNTOF(mSamplers));
    if (sampler != mSamplers[slot])
    {
#ifndef NS_PLATFORM_XBOX_ONE
        mContext->PSSetSamplers(slot, 1, &sampler);
#endif

        mSamplers[slot] = sampler;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetTexture(unsigned int slot, ID3D11ShaderResourceView* texture)
{
    NS_ASSERT(slot < NS_COUNTOF(mTextures));
    if (texture != mTextures[slot])
    {
#ifndef NS_PLATFORM_XBOX_ONE
        mContext->PSSetShaderResources(slot, 1, &texture);
#endif

        mTextures[slot] = texture;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::ClearRenderTarget()
{
    DX_BEGIN_EVENT(L"Clear");

    SetInputLayout(0);
    SetVertexShader(mQuadVS);
    SetPixelShader(mClearPS);

    SetRasterizerState(mRasterizerStates[2]);
    SetBlendState(mBlendStates[2]);
    SetDepthStencilState(mDepthStencilStates[4], 0);

    mContext->Draw(3, 0);

    DX_END_EVENT();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::ClearTextures()
{
    memset(mTextures, 0, sizeof(mTextures));
    mContext->PSSetShaderResources(0, NS_COUNTOF(mTextures), mTextures);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetShaders(const Batch& batch)
{
    const Program& program = Programs[batch.shader.v];
    SetInputLayout(mVertexStages[program.vShaderIdx].layout);
    SetVertexShader(mVertexStages[program.vShaderIdx].shader);
    SetPixelShader(mPixelShaders[program.pShaderIdx]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetBuffers(const Batch& batch)
{
    // Indices
    SetIndexBuffer(mIndices.currentPage->buffer);

    // Vertices
    const Program& program = Programs[batch.shader.v];
    unsigned int stride = mVertexStages[program.vShaderIdx].stride;
    unsigned int offset = mVertices.drawPos + batch.vertexOffset;
    SetVertexBuffer(mVertices.currentPage->buffer, stride, offset);

    // Vertex Constants
    if (mVertexCBHash != batch.projMtxHash)
    {
        void* ptr = MapBuffer(mVertexCB, 16 * sizeof(float));
        memcpy(ptr, batch.projMtx, 16 * sizeof(float));
        UnmapBuffer(mVertexCB);

        mVertexCBHash = batch.projMtxHash;

    }

    // Pixel Constants
    if (batch.rgba != 0 || batch.radialGrad != 0 || batch.opacity != 0)
    {
        uint32_t hash = batch.rgbaHash ^ batch.radialGradHash ^ batch.opacityHash;
        if (mPixelCBHash != hash)
        {
            void* ptr = MapBuffer(mPixelCB, 12 * sizeof(float));

            if (batch.rgba != 0)
            {
                memcpy(ptr, batch.rgba, 4 * sizeof(float));
                ptr = (uint8_t*)ptr + 4 * sizeof(float);
            }

            if (batch.radialGrad != 0)
            {
                memcpy(ptr, batch.radialGrad, 8 * sizeof(float));
                ptr = (uint8_t*)ptr + 8 * sizeof(float);
            }

            if (batch.opacity != 0)
            {
                memcpy(ptr, batch.opacity, sizeof(float));
            }

            UnmapBuffer(mPixelCB);

            mPixelCBHash = hash;

        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetRenderState(const Batch& batch)
{
    RenderState renderState = batch.renderState;

    uint32_t rasterizerIdx = renderState.f.wireframe | (renderState.f.scissorEnable << 1);
    ID3D11RasterizerState* rasterizer = mRasterizerStates[rasterizerIdx];
    SetRasterizerState(rasterizer);

    uint32_t blendIdx = renderState.f.blendMode | (renderState.f.colorEnable << 1);
    ID3D11BlendState* blend = mBlendStates[blendIdx];
    SetBlendState(blend);

    uint32_t depthIdx = renderState.f.stencilMode;
    ID3D11DepthStencilState* depth = mDepthStencilStates[depthIdx];
    SetDepthStencilState(depth, batch.stencilRef);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderDevice::SetTextures(const Batch& batch)
{
    if (batch.pattern != 0)
    {
        D3D11Texture* t = (D3D11Texture*)batch.pattern;
        SetTexture(TextureSlot::Pattern, t->view);
        SetSampler(TextureSlot::Pattern, mSamplerStates[batch.patternSampler.v]);
    }

    if (batch.ramps != 0)
    {
        D3D11Texture* t = (D3D11Texture*)batch.ramps;
        SetTexture(TextureSlot::Ramps, t->view);
        SetSampler(TextureSlot::Ramps, mSamplerStates[batch.rampsSampler.v]);
    }

    if (batch.image != 0)
    {
        D3D11Texture* t = (D3D11Texture*)batch.image;
        SetTexture(TextureSlot::Image, t->view);
        SetSampler(TextureSlot::Image, mSamplerStates[batch.imageSampler.v]);
    }

    if (batch.glyphs != 0)
    {
        D3D11Texture* t = (D3D11Texture*)batch.glyphs;
        SetTexture(TextureSlot::Glyphs, t->view);
        SetSampler(TextureSlot::Glyphs, mSamplerStates[batch.glyphsSampler.v]);
    }
}
