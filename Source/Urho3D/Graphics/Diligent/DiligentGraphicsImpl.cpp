//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../../Precompiled.h"

#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"

#include "../../DebugNew.h"

namespace Urho3D
{

GraphicsImpl::GraphicsImpl()
    : device_(nullptr)
    , deviceContext_(nullptr)
    , swapChain_(nullptr)
    , depthStencilView_(nullptr)
    ,
    /*defaultRenderTargetView_(nullptr),
    defaultDepthTexture_(nullptr),
    defaultDepthStencilView_(nullptr),
    */
    resolveTexture_(nullptr)
    , shaderProgram_(nullptr)
    , renderBackend_(RenderBackend::D3D11)
    , viewportDirty_(true)
    , adapterId_(M_MAX_UNSIGNED)
{
#ifdef PLATFORM_MACOS
    metalView_ = nullptr;
#endif
    for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        renderTargetViews_[i] = nullptr;

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        shaderResourceViews_[i] = nullptr;
        samplers_[i] = nullptr;
    }

    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
    {
        vertexBuffers_[i] = nullptr;
        // vertexSizes_[i] = 0;
        vertexOffsets_[i] = 0;
    }

    ea::fill(ea::begin(constantBuffers_), ea::end(constantBuffers_), nullptr);
    ea::fill(ea::begin(constantBuffersStartSlots_), ea::end(constantBuffersStartSlots_), 0u);
    ea::fill(ea::begin(constantBuffersNumSlots_), ea::end(constantBuffersNumSlots_), 0u);
}

bool GraphicsImpl::CheckMultiSampleSupport(Diligent::TEXTURE_FORMAT format, unsigned sampleCount) const
{
    using namespace Diligent;
    if (sampleCount < 2)
        return true; // Not multisampled

    const TextureFormatInfoExt& colorFmtInfo = device_->GetTextureFormatInfoExt(format);
    return colorFmtInfo.SampleCounts > 0;
}

unsigned GraphicsImpl::GetMultiSampleQuality(Diligent::TEXTURE_FORMAT format, unsigned sampleCount) const
{
    using namespace Diligent;
    if (sampleCount < 2)
        return 0;

    const TextureFormatInfoExt& colorFmtInfo = device_->GetTextureFormatInfoExt(format);
    if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_64)
        return 64;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_32)
        return 32;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_16)
        return 16;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_8)
        return 8;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_4)
        return 4;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_2)
        return 2;
    else if (colorFmtInfo.SampleCounts & SAMPLE_COUNT_1)
        return 1;
    return 0;

    // if (sampleCount < 2)
    //     return 0; // Not multisampled, should use quality 0

    // if (device_->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_1)
    //     return 0xffffffff; // D3D10.1+ standard level

    // UINT numLevels = 0;
    // if (FAILED(device_->CheckMultisampleQualityLevels(format, sampleCount, &numLevels)) || !numLevels)
    //     return 0; // Errored or sample count not supported
    // else
    //     return numLevels - 1; // D3D10.0 and below: use the best quality
}

unsigned GraphicsImpl::FindBestAdapter(Diligent::IEngineFactory* engineFactory, Diligent::Version& version)
{
    using namespace Diligent;
    unsigned numAdapters = 0;
    engineFactory->EnumerateAdapters(version, numAdapters, nullptr);
    ea::vector<GraphicsAdapterInfo> adapters(numAdapters);
    engineFactory->EnumerateAdapters(version, numAdapters, adapters.data());

    if (adapterId_ == M_MAX_UNSIGNED || adapterId_ >= numAdapters)
    {
        // Find best quality device
        unsigned result = DEFAULT_ADAPTER_ID;
        for (unsigned i = 0; i < numAdapters; ++i)
        {
            auto adapter = adapters[i];
            if (adapter.Type == ADAPTER_TYPE_INTEGRATED || adapter.Type == ADAPTER_TYPE_DISCRETE)
            {
                result = i;
                // Always prefer discrete gpu
                if (adapter.Type == ADAPTER_TYPE_DISCRETE)
                    break;
            }
        }
        return result;
    }
    else
    {
        return adapterId_;
    }
}
} // namespace Urho3D