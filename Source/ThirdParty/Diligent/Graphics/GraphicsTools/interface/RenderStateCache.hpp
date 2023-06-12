/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// C++ struct wrappers for render state cache.

#include "RenderStateCache.h"
#include "../../GraphicsEngine/interface/GraphicsTypesX.hpp"
#include "../../../Common/interface/FileWrapper.hpp"
#include "../../../Common/interface/DataBlobImpl.hpp"

namespace Diligent
{

/// C++ wrapper over IRenderDevice and IRenderStateCache.
template <bool ThrowOnError = true>
class RenderDeviceWithCache : public RenderDeviceX<ThrowOnError>
{
public:
    using TBase = RenderDeviceX<ThrowOnError>;

    RenderDeviceWithCache() noexcept {}

    explicit RenderDeviceWithCache(IRenderDevice*     pDevice,
                                   IRenderStateCache* pCache = nullptr) noexcept :
        TBase{pDevice},
        m_pCache{pCache}
    {
    }

    // clang-format off
    RenderDeviceWithCache           (const RenderDeviceWithCache&) noexcept = default;
    RenderDeviceWithCache& operator=(const RenderDeviceWithCache&) noexcept = default;
    RenderDeviceWithCache           (RenderDeviceWithCache&&)      noexcept = default;
    RenderDeviceWithCache& operator=(RenderDeviceWithCache&&)      noexcept = default;
    // clang-format on

    RenderDeviceWithCache(IRenderDevice*                    pDevice,
                          const RenderStateCacheCreateInfo& CacheCI) :
        TBase{pDevice}
    {
        CreateRenderStateCache(CacheCI);
    }

    ~RenderDeviceWithCache()
    {
        if (m_pCache != nullptr && !m_CacheFilePath.empty())
        {
            // Save render state cache data to the file
            RefCntAutoPtr<IDataBlob> pCacheData;
            if (m_pCache->WriteToBlob(&pCacheData))
            {
                if (pCacheData)
                {
                    FileWrapper CacheDataFile{m_CacheFilePath.c_str(), EFileAccessMode::Overwrite};
                    if (CacheDataFile->Write(pCacheData->GetConstDataPtr(), pCacheData->GetSize()))
                        LOG_INFO_MESSAGE("Successfully saved state cache file ", m_CacheFilePath, " (", FormatMemorySize(pCacheData->GetSize()), ").");
                }
            }
            else
            {
                LOG_ERROR_MESSAGE("Failed to write cache data.");
            }
        }
    }

    RefCntAutoPtr<IShader> CreateShader(const ShaderCreateInfo& ShaderCI) noexcept(!ThrowOnError)
    {
        return m_pCache ?
            UnpackCachedObject<IShader>("shader", ShaderCI.Desc.Name, &IRenderStateCache::CreateShader, ShaderCI) :
            TBase::CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IPipelineState> CreateGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return m_pCache ?
            UnpackCachedObject<IPipelineState>("graphics pipeline", CreateInfo.PSODesc.Name, &IRenderStateCache::CreateGraphicsPipelineState, CreateInfo) :
            TBase::CreateGraphicsPipelineState(CreateInfo);
    }

    RefCntAutoPtr<IPipelineState> CreateComputePipelineState(const ComputePipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return m_pCache ?
            UnpackCachedObject<IPipelineState>("compute pipeline", CreateInfo.PSODesc.Name, &IRenderStateCache::CreateComputePipelineState, CreateInfo) :
            TBase::CreateComputePipelineState(CreateInfo);
    }

    RefCntAutoPtr<IPipelineState> CreateRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return m_pCache ?
            UnpackCachedObject<IPipelineState>("ray-tracing pipeline", CreateInfo.PSODesc.Name, &IRenderStateCache::CreateRayTracingPipelineState, CreateInfo) :
            TBase::CreateRayTracingPipelineState(CreateInfo);
    }

    RefCntAutoPtr<IPipelineState> CreateTilePipelineState(const TilePipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return m_pCache ?
            UnpackCachedObject<IPipelineState>("tile pipeline", CreateInfo.PSODesc.Name, &IRenderStateCache::CreateTilePipelineState, CreateInfo) :
            TBase::CreateTilePipelineState(CreateInfo);
    }

    RefCntAutoPtr<IPipelineState> CreatePipelineState(const GraphicsPipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return CreateGraphicsPipelineState(CreateInfo);
    }
    RefCntAutoPtr<IPipelineState> CreatePipelineState(const ComputePipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return CreateComputePipelineState(CreateInfo);
    }
    RefCntAutoPtr<IPipelineState> CreatePipelineState(const RayTracingPipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return CreateRayTracingPipelineState(CreateInfo);
    }
    RefCntAutoPtr<IPipelineState> CreatePipelineState(const TilePipelineStateCreateInfo& CreateInfo) noexcept(!ThrowOnError)
    {
        return CreateTilePipelineState(CreateInfo);
    }

    IRenderStateCache* GetCache() const noexcept
    {
        return m_pCache;
    }

    operator IRenderStateCache*() const noexcept
    {
        return GetCache();
    }

    void CreateRenderStateCache(RenderStateCacheCreateInfo CacheCI) noexcept
    {
        if (m_pCache)
        {
            UNEXPECTED("Render state cache is already initialized");
            return;
        }

        if (CacheCI.pDevice == nullptr)
            CacheCI.pDevice = this->GetDevice();

        Diligent::CreateRenderStateCache(CacheCI, &m_pCache);
        VERIFY_EXPR(m_pCache);
    }

    void LoadCacheFromFile(const char* FilePath, bool UpdateOnExit)
    {
        if (!m_pCache)
        {
            UNEXPECTED("Render state cache is not initialized");
            return;
        }

        if (FilePath == nullptr)
        {
            UNEXPECTED("File path is null");
            return;
        }

        if (UpdateOnExit)
            m_CacheFilePath = FilePath;

        if (!FileSystem::FileExists(FilePath))
            return;

        FileWrapper CacheDataFile{FilePath};
        if (!CacheDataFile)
        {
            LOG_ERROR_MESSAGE("Failed to open render state cache file ", FilePath);
            return;
        }

        auto pCacheData = DataBlobImpl::Create();
        if (!CacheDataFile->Read(pCacheData))
        {
            LOG_ERROR_MESSAGE("Failed to read render state cache file ", FilePath);
            return;
        }

        if (!m_pCache->Load(pCacheData))
            LOG_ERROR_MESSAGE("Failed to load render state cache data from file ", FilePath);
    }

private:
    template <typename ObjectType, typename CreateMethodType, typename... CreateArgsType>
    RefCntAutoPtr<ObjectType> UnpackCachedObject(const char* ObjectTypeName, const char* ObjectName, CreateMethodType Create, CreateArgsType&&... Args) noexcept(!ThrowOnError)
    {
        RefCntAutoPtr<ObjectType> pObject;
        (m_pCache->*Create)(std::forward<CreateArgsType>(Args)..., &pObject);
        if (ThrowOnError)
        {
            if (!pObject)
                LOG_ERROR_AND_THROW("Failed to create ", ObjectTypeName, " '", (ObjectName != nullptr ? ObjectName : "<unnamed>"), "'.");
        }
        return pObject;
    }

private:
    RefCntAutoPtr<IRenderStateCache> m_pCache;
    std::string                      m_CacheFilePath;
};

/// Special string to indicate that the render state cache file should be stored in the application data folder.
static constexpr char RenderStateCacheLocationAppData[] = "<AppData>";

/// Returns the path to the render state cache file.

/// \param [in] CacheLocation - Cache location. If it is equal to RenderStateCacheLocationAppData,
///                             the function returns the path to the cache file in the application data folder.
///                             Otherwise, the function returns the path to the cache file in the specified folder.
/// \param [in] AppName       - Application name.
/// \param [in] DeviceType    - Render device type.
/// \return                   Render state cache file path.
std::string GetRenderStateCacheFilePath(const char*        CacheLocation,
                                        const char*        AppName,
                                        RENDER_DEVICE_TYPE DeviceType);

} // namespace Diligent