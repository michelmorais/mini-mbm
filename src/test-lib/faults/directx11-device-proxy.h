/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/


#ifndef TEST_DIRECTX11_DEVICE_PROXY_H
#define TEST_DIRECTX11_DEVICE_PROXY_H
#include <d3d11.h>
#include <cstring>

// Test-only synchronous proxy. Native children and all unhandled interfaces
// retain their real device; the engine pointer is restored before teardown.
class DIRECTX11_DEVICE_PROXY : public ID3D11Device
{
public:
    ID3D11Device *real;
    const char *fault = "";
    unsigned ordinal = 0, seen = 0, hits = 0, calls = 0, shaderCalls = 0;
    explicit DIRECTX11_DEVICE_PROXY(ID3D11Device *device) : real(device) {}
    void arm(const char *method = "", unsigned nth = 0)
    {
        fault = method; ordinal = nth; seen = hits = calls = shaderCalls = 0;
    }
    bool fail(const char *method)
    {
        ++calls;
        if (std::strcmp(method,"CreateBuffer") != 0) ++shaderCalls;
        if (std::strcmp(method,fault) != 0 || ++seen != ordinal) return false;
        ++hits;
        return true;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
    {
        if (!out) return E_POINTER;
        if (id == __uuidof(IUnknown) || id == __uuidof(ID3D11Device))
        { *out = this; AddRef(); return S_OK; }
        return real->QueryInterface(id,out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return real->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return real->Release(); }
    virtual void created(ID3D11DeviceChild *, bool) {}
    HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer) override
    {
        if (fail("CreateBuffer")) { if (ppBuffer) *ppBuffer = nullptr; return E_OUTOFMEMORY; }
        const HRESULT hr = real->CreateBuffer(pDesc, pInitialData, ppBuffer);
        if (SUCCEEDED(hr) && ppBuffer && *ppBuffer) created(*ppBuffer, (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER) != 0 || ((pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER) != 0 && pDesc->ByteWidth > 16));
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D) override
    {
        return real->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
    }
    HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D) override
    {
        return real->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
    }
    HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D) override
    {
        return real->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
    }
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView) override
    {
        return real->CreateShaderResourceView(pResource, pDesc, ppSRView);
    }
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUAView) override
    {
        return real->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
    }
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView) override
    {
        return real->CreateRenderTargetView(pResource, pDesc, ppRTView);
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView) override
    {
        return real->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
    }
    HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout) override
    {
        if (fail("CreateInputLayout")) { if (ppInputLayout) *ppInputLayout = nullptr; return E_OUTOFMEMORY; }
        const HRESULT hr = real->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
        if (SUCCEEDED(hr) && ppInputLayout && *ppInputLayout) created(*ppInputLayout, false);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader) override
    {
        if (fail("CreateVertexShader")) { if (ppVertexShader) *ppVertexShader = nullptr; return E_OUTOFMEMORY; }
        const HRESULT hr = real->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        if (SUCCEEDED(hr) && ppVertexShader && *ppVertexShader) created(*ppVertexShader, false);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader) override
    {
        return real->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
    }
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader) override
    {
        return real->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
    }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader) override
    {
        if (fail("CreatePixelShader")) { if (ppPixelShader) *ppPixelShader = nullptr; return E_OUTOFMEMORY; }
        const HRESULT hr = real->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
        if (SUCCEEDED(hr) && ppPixelShader && *ppPixelShader) created(*ppPixelShader, false);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) override
    {
        return real->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
    }
    HRESULT STDMETHODCALLTYPE CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader) override
    {
        return real->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
    }
    HRESULT STDMETHODCALLTYPE CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader) override
    {
        return real->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
    }
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) override
    {
        return real->CreateClassLinkage(ppLinkage);
    }
    HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState) override
    {
        return real->CreateBlendState(pBlendStateDesc, ppBlendState);
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState) override
    {
        return real->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
    }
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState) override
    {
        return real->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
    }
    HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState) override
    {
        if (fail("CreateSamplerState")) { if (ppSamplerState) *ppSamplerState = nullptr; return E_OUTOFMEMORY; }
        const HRESULT hr = real->CreateSamplerState(pSamplerDesc, ppSamplerState);
        if (SUCCEEDED(hr) && ppSamplerState && *ppSamplerState) created(*ppSamplerState, false);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery) override
    {
        return real->CreateQuery(pQueryDesc, ppQuery);
    }
    HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc, ID3D11Predicate **ppPredicate) override
    {
        return real->CreatePredicate(pPredicateDesc, ppPredicate);
    }
    HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter) override
    {
        return real->CreateCounter(pCounterDesc, ppCounter);
    }
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext) override
    {
        return real->CreateDeferredContext(ContextFlags, ppDeferredContext);
    }
    HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource) override
    {
        return real->OpenSharedResource(hResource, ReturnedInterface, ppResource);
    }
    HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport) override
    {
        return real->CheckFormatSupport(Format, pFormatSupport);
    }
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) override
    {
        return real->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
    }
    void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo) override
    {
        real->CheckCounterInfo(pCounterInfo);
    }
    HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) override
    {
        return real->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
    }
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) override
    {
        return real->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override
    {
        return real->GetPrivateData(guid, pDataSize, pData);
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override
    {
        return real->SetPrivateData(guid, DataSize, pData);
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override
    {
        return real->SetPrivateDataInterface(guid, pData);
    }
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() override
    {
        return real->GetFeatureLevel();
    }
    UINT STDMETHODCALLTYPE GetCreationFlags() override
    {
        return real->GetCreationFlags();
    }
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override
    {
        return real->GetDeviceRemovedReason();
    }
    void STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) override
    {
        real->GetImmediateContext(ppImmediateContext);
    }
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override
    {
        return real->SetExceptionMode(RaiseFlags);
    }
    UINT STDMETHODCALLTYPE GetExceptionMode() override
    {
        return real->GetExceptionMode();
    }
};
#endif
