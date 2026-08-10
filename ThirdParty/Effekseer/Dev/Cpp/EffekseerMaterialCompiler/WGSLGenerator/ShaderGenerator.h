#pragma once

#include "../../Effekseer/Effekseer/Material/Effekseer.MaterialCompiler.h"
#include <string>

namespace Effekseer
{
namespace WGSL
{

struct ShaderData
{
	std::string CodeVS;
	std::string CodePS;
};

class ShaderGenerator
{
	std::string Replace(std::string target, const std::string& from, const std::string& to) const;

	std::string GetType(int32_t i) const;

	std::string GetElement(int32_t i) const;

	std::string ExportResources(MaterialFile* materialFile, int stage, int32_t maximumTextureCount) const;

	std::string ExportUniformBlock(MaterialFile* materialFile,
								   MaterialShaderType shaderType,
								   int stage,
								   bool isSprite,
								   int32_t maximumUniformCount,
								   int32_t instanceCount) const;

	std::string ConvertGenericCode(MaterialFile* materialFile, int stage, int32_t maximumTextureCount) const;

	std::string GenerateVertexShader(MaterialFile* materialFile,
									 MaterialShaderType shaderType,
									 int32_t maximumUniformCount,
									 int32_t maximumTextureCount,
									 int32_t instanceCount) const;

	std::string GeneratePixelShader(MaterialFile* materialFile,
									MaterialShaderType shaderType,
									int32_t maximumUniformCount,
									int32_t maximumTextureCount) const;

public:
	ShaderData GenerateShader(MaterialFile* materialFile,
							  MaterialShaderType shaderType,
							  int32_t maximumUniformCount,
							  int32_t maximumTextureCount,
							  int32_t instanceCount) const;
};

} // namespace WGSL
} // namespace Effekseer
