
#include "core/shader_library.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>
#include <cstdio>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace AuroraGlass {

namespace {

void ReportShaderError(const fs::path& file, const std::string& entry, ID3DBlob* errors) {
    std::string msg = "[AuroraGlass] shader compile failed: ";
    msg += file.string();
    msg += " entry=" + entry;
    if (errors && errors->GetBufferSize() > 0) {
        msg += "\n";
        msg += static_cast<const char*>(errors->GetBufferPointer());
    }
    msg += "\n";
    OutputDebugStringA(msg.c_str());
    fprintf(stderr, "%s", msg.c_str());
}

} // namespace

// Minimal local-filesystem include handler for #include "file.hlsl".
class LocalInclude : public ID3DInclude {
public:
    explicit LocalInclude(fs::path baseDir) : baseDir_(std::move(baseDir)) {}

    HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE /*includeType*/, LPCSTR pFileName,
                                   LPCVOID /*pParentData*/, LPCVOID* ppData, UINT* pBytes) override {
        fs::path path = baseDir_ / pFileName;
        std::ifstream f(path, std::ios::binary);
        if (!f) return E_FAIL;
        std::stringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        auto buf = std::make_unique<std::vector<char>>(content.begin(), content.end());
        *ppData = buf->data();
        *pBytes = static_cast<UINT>(buf->size());
        store_.push_back(std::move(buf));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Close(LPCVOID /*pData*/) override { return S_OK; }

private:
    fs::path baseDir_;
    std::vector<std::unique_ptr<std::vector<char>>> store_;
};

void ShaderLibrary::Init(std::wstring shaderDir) {
    baseDir_ = std::move(shaderDir);
}

ComPtr<ID3DBlob> ShaderLibrary::Compile(const std::wstring& file, const std::string& entry,
                                        const std::string& profile) {
    fs::path base(baseDir_);
    fs::path full = base / file;

    ComPtr<ID3DBlob> code, errors;
    LocalInclude inc(base);
    HRESULT hr = D3DCompileFromFile(full.wstring().c_str(), nullptr, &inc,
                                    entry.c_str(), profile.c_str(),
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr) || !code) {
        ReportShaderError(full, entry, errors.Get());
        return nullptr;
    }
    return code;
}

} // namespace AuroraGlass
