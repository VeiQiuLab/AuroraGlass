
#pragma once
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

namespace AuroraGlass {

// Runtime HLSL compilation (D3DCompile) with local #include support.
// P0 keeps shaders as editable files under shaders/, compiled at startup.
// This class only produces bytecode blobs; the renderer creates GPU objects.
class ShaderLibrary {
public:
    void Init(std::wstring shaderDir);
    const std::wstring& ShaderDir() const { return baseDir_; }

    // profile examples: "vs_5_0" / "ps_5_0". entry must match a function in the file.
    Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::wstring& file,
                                             const std::string& entry,
                                             const std::string& profile);

private:
    std::wstring baseDir_;
};

} // namespace AuroraGlass
