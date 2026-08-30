#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

/**
 * @class VulkanShader
 * @brief Loads compiled SPIR-v and creates caller-owned shader modules.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Missing geometry shaders fail initialization. missing geometry3d shaders disable 3D draws.
 * Cmake compiles shader pairs and copies them beside the executable.
 *
 * Validation checks format, not freshness. rebuild stale .spv files if Vulkan output differs
 * from GLSL sources.
 *
 * @code
 * // Shaders are pre-compiled during build:
 * // glslangValidator -V Geometry.vert -o Geometry.vert.spv
 * // glslangValidator -V Geometry.frag -o Geometry.frag.spv
 *
 * // At runtime, load and create modules:
 * auto vertSPV = VulkanShader::GetVertexShaderSPIRV();
 * auto fragSPV = VulkanShader::GetFragmentShaderSPIRV();
 * VkShaderModule vertModule = VulkanShader::CreateShaderModule(device, vertSPV);
 * VkShaderModule fragModule = VulkanShader::CreateShaderModule(device, fragSPV);
 * @endcode
 */
class VulkanShader
{
public:
    /**
     * @fn static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint32_t>& \
     * code)
     * @brief Creates a module owned by the caller.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Release with vkDestroyShaderModule. empty bytecode or Vulkan failure throws
     * std::runtime_error.
     *
     * @code
     * auto spirv = VulkanShader::GetVertexShaderSPIRV();
     * VkShaderModule module = VulkanShader::CreateShaderModule(device, spirv);
     * // Use module in VkPipelineShaderStageCreateInfo...
     * vkDestroyShaderModule(device, module, nullptr);
     * @endcode
     */
    static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint32_t>& code);

    /**
     * @fn static std::vector<uint32_t> LoadSPIRV(const std::string& relativePath)
     * @brief Loads the first valid shader under the runtime search roots.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Searches the working directory, executable directory, and its two parents.
     * Rejects empty files, sizes above 16 MiB, unaligned byte lengths, and invalid SPIR-v magic.
     *
     * @return Empty on missing or invalid input; logs failures.
     */
    static std::vector<uint32_t> LoadSPIRV(const std::string& relativePath);

    /**
     * @fn static std::vector<uint32_t> GetVertexShaderSPIRV()
     * @brief Loads shaders/geometry.vert.spv with LoadSPIRV search and validation.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::vector<uint32_t> GetVertexShaderSPIRV();

    /**
     * @fn static std::vector<uint32_t> GetFragmentShaderSPIRV()
     * @brief Loads shaders/geometry.frag.spv with LoadSPIRV search and validation.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::vector<uint32_t> GetFragmentShaderSPIRV();
};
