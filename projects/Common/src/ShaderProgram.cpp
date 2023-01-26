module;
#include <cassert>
#include <format>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
module ShaderProgram;

import Application;
import ErrorHandling;

namespace
{
	using gg::AssetLoadException;

	std::string readFile(std::string const& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw AssetLoadException(std::format("failed to open file: {}", filename));
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::string buffer{};
		buffer.resize(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
}

namespace gg
{
	ShaderProgram::ShaderProgram(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
		: vertexShaderBlob{ readFile(std::filesystem::absolute(vertexShaderRelativePath).generic_string()) }
		, fragmentShaderBlob{ readFile(std::filesystem::absolute(fragmentShaderRelativePath).generic_string()) }
	{
		BreakIfFalse(vertexShaderBlob.size() != 0);
		BreakIfFalse(fragmentShaderBlob.size() != 0);
	}

} // namespace gg
