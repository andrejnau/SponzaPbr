set(target SponzaPbr)

set(include_path "${CMAKE_CURRENT_SOURCE_DIR}")
set(source_path "${CMAKE_CURRENT_SOURCE_DIR}")
set(shaders_path "${assets_path}/shaders/SponzaPbr")

set(headers
    ${include_path}/GeometryPass.h
    ${include_path}/LightPass.h
    ${include_path}/ImGuiPass.h
    ${include_path}/SSAOPass.h
    ${include_path}/ShadowPass.h
    ${include_path}/IrradianceConversion.h
    ${include_path}/Equirectangular2Cubemap.h
    ${include_path}/BRDFGen.h
    ${include_path}/BackgroundPass.h
    ${include_path}/ComputeLuminance.h
    ${include_path}/IBLCompute.h
    ${include_path}/SkinningPass.h
    ${include_path}/Scene.h
    ${include_path}/ImGuiSettings.h
    ${include_path}/SponzaSettings.h
    ${include_path}/RenderPass.h
)

set(sources
    ${source_path}/GeometryPass.cpp
    ${source_path}/LightPass.cpp
    ${source_path}/ImGuiPass.cpp
    ${source_path}/SSAOPass.cpp
    ${source_path}/ShadowPass.cpp
    ${source_path}/IrradianceConversion.cpp
    ${source_path}/Equirectangular2Cubemap.cpp
    ${source_path}/BRDFGen.cpp
    ${source_path}/BackgroundPass.cpp
    ${source_path}/ComputeLuminance.cpp
    ${source_path}/IBLCompute.cpp
    ${source_path}/SkinningPass.cpp
    ${source_path}/Scene.cpp
    ${source_path}/SponzaSettings.cpp
    ${source_path}/main.cpp
)

set(shader_headers
    ${shaders_path}/BoneTransform.hlsli
)

set(pixel_shaders
    ${shaders_path}/GeometryPass_PS.hlsl
    ${shaders_path}/LightPass_PS.hlsl
    ${shaders_path}/ImGuiPass_PS.hlsl
    ${shaders_path}/HDRApply_PS.hlsl
    ${shaders_path}/SSAOPass_PS.hlsl
    ${shaders_path}/SSAOBlurPass_PS.hlsl
    ${shaders_path}/Equirectangular2Cubemap_PS.hlsl
    ${shaders_path}/IrradianceConvolution_PS.hlsl
    ${shaders_path}/Background_PS.hlsl
    ${shaders_path}/Prefilter_PS.hlsl
    ${shaders_path}/BRDF_PS.hlsl
    ${shaders_path}/ShadowPass_PS.hlsl
    ${shaders_path}/IBLCompute_PS.hlsl
    ${shaders_path}/IBLComputePrePass_PS.hlsl
)

set(vertex_shaders
    ${shaders_path}/GeometryPass_VS.hlsl
    ${shaders_path}/LightPass_VS.hlsl
    ${shaders_path}/ImGuiPass_VS.hlsl
    ${shaders_path}/HDRApply_VS.hlsl
    ${shaders_path}/SSAOPass_VS.hlsl
    ${shaders_path}/Cubemap_VS.hlsl
    ${shaders_path}/Background_VS.hlsl
    ${shaders_path}/BRDF_VS.hlsl
    ${shaders_path}/ShadowPass_VS.hlsl
    ${shaders_path}/IBLCompute_VS.hlsl
)

set(compute_shaders
    ${shaders_path}/HDRLum1DPass_CS.hlsl
    ${shaders_path}/HDRLum2DPass_CS.hlsl
    ${shaders_path}/DownSample_CS.hlsl
    ${shaders_path}/Skinning_CS.hlsl
)

set(headers
    ${headers}
    ${include_path}/RayTracingAOPass.h
)
set(sources
    ${sources}
    ${source_path}/RayTracingAOPass.cpp
)
set(lib_shaders
    ${lib_shaders}
    ${shaders_path}/RayTracingAO.hlsl
)

set_property(SOURCE ${pixel_shaders} PROPERTY SHADER_ENTRYPOINT main)
set_property(SOURCE ${pixel_shaders} PROPERTY SHADER_TYPE Pixel)
set_property(SOURCE ${pixel_shaders} PROPERTY SHADER_MODEL 6.0)

set_property(SOURCE ${vertex_shaders} PROPERTY SHADER_ENTRYPOINT main)
set_property(SOURCE ${vertex_shaders} PROPERTY SHADER_TYPE Vertex)
set_property(SOURCE ${vertex_shaders} PROPERTY SHADER_MODEL 6.0)

set_property(SOURCE ${compute_shaders} PROPERTY SHADER_ENTRYPOINT main)
set_property(SOURCE ${compute_shaders} PROPERTY SHADER_TYPE Compute)
set_property(SOURCE ${compute_shaders} PROPERTY SHADER_MODEL 6.0)

set_property(SOURCE ${lib_shaders} PROPERTY SHADER_ENTRYPOINT " ")
set_property(SOURCE ${lib_shaders} PROPERTY SHADER_TYPE Library)
set_property(SOURCE ${lib_shaders} PROPERTY SHADER_MODEL 6.3)

set(shaders_files ${pixel_shaders} ${vertex_shaders} ${compute_shaders} ${lib_shaders})

include(${project_root}/3rdparty/FlyWrapper/cmake/shader_parser.cmake)
gen_shaders_ref(${target} "${shaders_files}" shaders_ref)

source_group("Shader Files" FILES ${shaders_files} ${shader_headers})
source_group("Shader Ref Files" FILES ${shaders_ref})

add_executable(${target} WIN32 ${headers} ${sources} ${shaders_files} ${shader_headers} ${shaders_ref})

if (WIN32)
    set_target_properties(${target} PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

target_include_directories(${target}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        ${CMAKE_BINARY_DIR}/gen/${target}
)

target_link_libraries(${target}
    RenderDevice
    assimp
    AppBox
    Geometry
    Camera
    Texture
    imgui
    SponzaPbrAssets
)

set_target_properties(${target} PROPERTIES FOLDER "Apps")

install(TARGETS ${target})
