////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "D3D11RenderContext.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/Category.h>
#include <NsCore/TypeId.h>
#include <NsCore/Log.h>
#include <NsCore/UTF8.h>
#include <NsRender/D3D11Factory.h>
#include <NsRender/RenderDevice.h>
#include <NsRender/RenderTarget.h>
#include <NsRender/Texture.h>
#include <NsRender/Image.h>
#include <NsDrawing/Color.h>


using namespace Noesis;
using namespace NoesisApp;


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

#ifdef NS_DEBUG
    #define V(exp) \
        NS_MACRO_BEGIN \
            HRESULT hr_ = (exp); \
            if (FAILED(hr_)) \
            { \
                NS_FATAL("%s[0x%08x]", #exp, hr_); \
            } \
        NS_MACRO_END
#else
    #define V(exp) exp
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
D3D11RenderContext::D3D11RenderContext()
{
#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    mD3D11 = LoadLibraryA("d3d11.dll");
    if (mD3D11 != 0)
    {
        D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(mD3D11, "D3D11CreateDevice");
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
D3D11RenderContext::~D3D11RenderContext()
{
#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    if (mD3D11 != 0)
    {
        FreeLibrary(mD3D11);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* D3D11RenderContext::Description() const
{
    return "D3D11";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t D3D11RenderContext::Score() const
{
    return 200;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool D3D11RenderContext::Validate() const
{
    return D3D11CreateDevice != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::Init(void* window, uint32_t& samples, bool vsync, bool sRGB)
{
    NS_ASSERT(Validate());
    NS_LOG_DEBUG("Creating D3D11 render context");

#ifndef NS_PLATFORM_XBOX_ONE
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef NS_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL features[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    D3D_FEATURE_LEVEL level;

    HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, features,
        NS_COUNTOF(features), D3D11_SDK_VERSION, &mDevice, &level, &mContext);

#ifdef NS_DEBUG
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        // The debug layer needs D3D11*SDKLayers.dll installed; otherwise, device creation fails
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, features,
            NS_COUNTOF(features), D3D11_SDK_VERSION, &mDevice, &level, &mContext);
    }
#endif

    NS_ASSERT(SUCCEEDED(hr));
    NS_LOG_DEBUG(" Feature Level: %d_%d", (level >> 12) & 0xf, (level >> 8) & 0xf);
#endif

    CreateSwapChain(window, samples, sRGB);
    CreateQueries();

    mVSync = vsync;
    mRenderer = D3D11Factory::CreateDevice(mContext, sRGB);

#ifndef NS_PLATFORM_XBOX_ONE
    NS_LOG_DEBUG(" Adapter: %s", [this]()
    {
        IDXGIDevice* dxgiDevice;
        V(mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));

        IDXGIAdapter* dxgiAdapter;
        V(dxgiDevice->GetAdapter(&dxgiAdapter));

        DXGI_ADAPTER_DESC desc;
        V(dxgiAdapter->GetDesc(&desc));

        static char description[128];
        uint32_t numChars = UTF8::UTF16To8((uint16_t*)desc.Description, description, 128);
        NS_ASSERT(numChars <= 128);

        DX_RELEASE(dxgiAdapter);
        DX_RELEASE(dxgiDevice);

        return description;
    }()); 
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::Shutdown()
{
    mRenderer.Reset();

    for (uint32_t i = 0; i < NS_COUNTOF(mFrames); i++)
    {
        DX_DESTROY(mFrames[i].begin);
        DX_DESTROY(mFrames[i].end);
        DX_DESTROY(mFrames[i].disjoint);
    }

    DX_DESTROY(mSwapChain);
    DX_DESTROY(mDepthStencil);

#ifndef NS_PLATFORM_XBOX_ONE
    DX_DESTROY(mRenderTarget);
#endif

    DX_DESTROY(mContext);
    DX_DESTROY(mDevice);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RenderDevice* D3D11RenderContext::GetDevice() const
{
    return mRenderer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::BeginRender()
{
#ifdef NS_PROFILE
    const Frame& w = mFrames[mWriteFrame];
    mContext->Begin(w.disjoint);
    mContext->End(w.begin);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::EndRender()
{
#ifdef NS_PROFILE
    const Frame& w = mFrames[mWriteFrame];
    mContext->End(w.end);
    mContext->End(w.disjoint);

    mWriteFrame = (mWriteFrame + 1) % NS_COUNTOF(mFrames);
    if (mWriteFrame == mReadFrame)
    {
        const Frame& r = mFrames[mReadFrame];

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data;
        V(mContext->GetData(r.disjoint, &data, sizeof(data), D3D11_ASYNC_GETDATA_DONOTFLUSH));

        if (!data.Disjoint)
        {
            UINT64 begin;
            V(mContext->GetData(r.begin, &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH));

            UINT64 end;
            V(mContext->GetData(r.end, &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH));

            mGPUTime = (float)(1000 * double(end - begin) / data.Frequency);
        }

        mReadFrame = (mReadFrame + 1) % NS_COUNTOF(mFrames);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::Resize()
{
#ifndef NS_PLATFORM_XBOX_ONE
    NS_ASSERT(mContext != 0);
    mContext->OMSetRenderTargets(0, nullptr, nullptr);

    DX_DESTROY(mDepthStencil);
    DX_DESTROY(mRenderTarget);

    NS_ASSERT(mSwapChain != 0);
    V(mSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
#endif

    CreateBuffers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float D3D11RenderContext::GetGPUTimeMs() const
{
    return mGPUTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::SetClearColor(float r, float g, float b, float a)
{
    if (mRenderer->GetCaps().linearRendering)
    {
        Color color(r, g, b, a);
        color.ToLinearRGB(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]);
    }
    else
    {
        mClearColor[0] = r;
        mClearColor[1] = g;
        mClearColor[2] = b;
        mClearColor[3] = a;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::SetDefaultRenderTarget(uint32_t, uint32_t, bool doClearColor)
{
#ifndef NS_PLATFORM_XBOX_ONE
    ID3D11RenderTargetView* rtv = mRenderTarget;
#endif

    if (doClearColor)
    {
        mContext->ClearRenderTargetView(rtv, mClearColor);
    }

    mContext->ClearDepthStencilView(mDepthStencil, D3D11_CLEAR_STENCIL, 0.0f, 0);

    mContext->OMSetRenderTargets(1, &rtv, mDepthStencil);
    mContext->RSSetViewports(1, &mViewport);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::Image> D3D11RenderContext::CaptureRenderTarget(RenderTarget* surface) const
{
    struct D3D11Texture: public Texture
    {
        ID3D11ShaderResourceView* view;
    };

    ID3D11Texture2D* source;
    ((D3D11Texture*)surface->GetTexture())->view->GetResource((ID3D11Resource**)&source);

    D3D11_TEXTURE2D_DESC desc;
    source->GetDesc(&desc);

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;

    ID3D11Texture2D* dest;
    V(mDevice->CreateTexture2D(&desc, 0, &dest));
    mContext->CopyResource(dest, source);

    D3D11_MAPPED_SUBRESOURCE mapped;
    V(mContext->Map(dest, 0, D3D11_MAP_READ, 0, &mapped));

    Ptr<Image> image = *new Image(desc.Width, desc.Height);
    const uint8_t* src = (const uint8_t*)(mapped.pData);
    uint8_t* dst = (uint8_t*)(image->Data());

    for (uint32_t i = 0; i < desc.Height; i++)
    {
        for (uint32_t j = 0; j < desc.Width; j++)
        {
            // RGBA -> BGRA
            dst[4 * j + 0] = src[4 * j + 2];
            dst[4 * j + 1] = src[4 * j + 1];
            dst[4 * j + 2] = src[4 * j + 0];
            dst[4 * j + 3] = src[4 * j + 3];
        }

        src += mapped.RowPitch;
        dst += image->Stride();
    }

    mContext->Unmap(dest, 0);

    DX_RELEASE(source);
    DX_DESTROY(dest);

    return image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::Swap()
{
#ifndef NS_PLATFORM_XBOX_ONE
    V(mSwapChain->Present(mVSync ? 1 : 0, 0));
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DXGI_SAMPLE_DESC D3D11RenderContext::GetSampleDesc(uint32_t samples) const
{
    NS_ASSERT(mDevice != 0);

    DXGI_SAMPLE_DESC descs[16];
    samples = eastl::max_alt(1U, eastl::min_alt(samples, 16U));

    descs[0].Count = 1;
    descs[0].Quality = 0;

    for (uint32_t i = 1, last = 0; i < samples; i++)
    {
        unsigned int count = i + 1;
        unsigned int quality = 0;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        HRESULT hr = mDevice->CheckMultisampleQualityLevels(format, count, &quality);

        if (SUCCEEDED(hr) && quality > 0)
        {
            descs[i].Count = count;
            descs[i].Quality = 0;
            last = i;
        }
        else
        {
            descs[i] = descs[last];
        }
    }

    return descs[samples - 1];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::CreateSwapChain(void* window, uint32_t& samples, bool sRGB)
{
    IDXGIDevice* dxgiDevice;
    V(mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));

    IDXGIAdapter* dxgiAdapter;
    V(dxgiDevice->GetAdapter(&dxgiAdapter));

#ifndef NS_PLATFORM_XBOX_ONE
    IDXGIFactory* factory;
    V(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory));

    DX_RELEASE(dxgiDevice);
    DX_RELEASE(dxgiAdapter);

    mHwnd = (HWND)window;

    DXGI_SWAP_CHAIN_DESC desc;
    desc.BufferDesc.Width = 0;
    desc.BufferDesc.Height = 0;
    desc.BufferDesc.RefreshRate.Numerator = 0;
    desc.BufferDesc.RefreshRate.Denominator = 0;
    desc.BufferDesc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc = GetSampleDesc(samples);
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = mHwnd;
    desc.Windowed = true;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags = 0;

    V(factory->CreateSwapChain(mDevice, &desc, &mSwapChain));
    samples = desc.SampleDesc.Count;
    DX_RELEASE(factory);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::CreateBuffers()
{
    NS_ASSERT(mSwapChain != 0);

    DXGI_SWAP_CHAIN_DESC desc;
    V(mSwapChain->GetDesc(&desc));

    // Grab swap chain render targets
#ifndef NS_PLATFORM_XBOX_ONE
    ID3D11Texture2D* color;
    V(mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&color));
    V(mDevice->CreateRenderTargetView(color, 0, &mRenderTarget));
    DX_RELEASE(color);
#endif

    // Stencil buffer
    D3D11_TEXTURE2D_DESC textureDesc;
    textureDesc.Width = desc.BufferDesc.Width;
    textureDesc.Height = desc.BufferDesc.Height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.SampleDesc = desc.SampleDesc;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

#ifndef NS_PLATFORM_XBOX_ONE
    textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif

    ID3D11Texture2D* depthStencil;
    V(mDevice->CreateTexture2D(&textureDesc, 0, &depthStencil));
    V(mDevice->CreateDepthStencilView(depthStencil, 0, &mDepthStencil));
    DX_RELEASE(depthStencil);

    // Viewport
    mViewport.TopLeftX = 0.0f;
    mViewport.TopLeftY = 0.0f;
    mViewport.Width = (FLOAT)desc.BufferDesc.Width;
    mViewport.Height = (FLOAT)desc.BufferDesc.Height;
    mViewport.MinDepth = 0.0f;
    mViewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void D3D11RenderContext::CreateQueries()
{
    for (uint32_t i = 0; i < NS_COUNTOF(mFrames); i++)
    {
        D3D11_QUERY_DESC desc;
        desc.MiscFlags = 0;

        desc.Query = D3D11_QUERY_TIMESTAMP;
        V(mDevice->CreateQuery(&desc, &mFrames[i].begin));
        V(mDevice->CreateQuery(&desc, &mFrames[i].end));

        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        V(mDevice->CreateQuery(&desc, &mFrames[i].disjoint));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(D3D11RenderContext)
{
    NsMeta<TypeId>("D3D11RenderContext");
    NsMeta<Category>("RenderContext");
}
