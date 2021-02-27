#include "ShaderReflection/ShaderReflection.h"
#ifdef DIRECTX_SUPPORT
#include "ShaderReflection/DXILReflection.h"
#endif
#ifdef VULKAN_SUPPORT
#include "ShaderReflection/SPIRVReflection.h"
#endif
#include <cassert>

std::shared_ptr<ShaderReflection> CreateShaderReflection(ShaderBlobType type, const void* data, size_t size)
{
    switch (type)
    {
#ifdef DIRECTX_SUPPORT
    case ShaderBlobType::kDXIL:
        return std::make_shared<DXILReflection>(data, size);
#endif
#ifdef VULKAN_SUPPORT
    case ShaderBlobType::kSPIRV:
        return std::make_shared<SPIRVReflection>(data, size);
#endif
    }
    assert(false);
    return nullptr;
}
