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


#ifndef TEST_DIRECTX11_CONTEXT_PROXY_H
#define TEST_DIRECTX11_CONTEXT_PROXY_H
#include <d3d11.h>
#include <unordered_set>

// Test-only synchronous wrapper. All native interfaces/resources keep their
// real context/device. The owner must restore the engine pointer before exit.
class DIRECTX11_CONTEXT_PROXY : public ID3D11DeviceContext
{
public:
    enum class MAP_KIND { NONE, MATRIX, LIGHT, NORMAL };
    ID3D11DeviceContext *real;
    MAP_KIND kind = MAP_KIND::NONE;
    unsigned ordinal = 0, seen = 0, hits = 0;
    bool balanced = true;
    std::unordered_set<ID3D11Resource *> mappedResources;
    explicit DIRECTX11_CONTEXT_PROXY(ID3D11DeviceContext *value) noexcept : real(value) {}
    void arm(MAP_KIND value = MAP_KIND::NONE, unsigned nth = 0) noexcept
    { kind = value; ordinal = nth; seen = hits = 0; }
    bool failMap(ID3D11Resource *resource, D3D11_MAP mode)
    {
        if (kind == MAP_KIND::NONE || mode != D3D11_MAP_WRITE_DISCARD) return false;
        ID3D11Buffer *buffer = nullptr;
        if (FAILED(resource->QueryInterface(__uuidof(ID3D11Buffer),reinterpret_cast<void **>(&buffer)))) return false;
        D3D11_BUFFER_DESC desc = {};
        buffer->GetDesc(&desc);
        buffer->Release();
        if (!(desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)) return false;
        // Reserved static pipeline: two float4x4 matrices, normal settings
        // float4, or the larger generated light block (all supported caps).
        MAP_KIND actual = MAP_KIND::LIGHT;
        if (desc.ByteWidth == 128) actual = MAP_KIND::MATRIX;
        else if (desc.ByteWidth == 16) actual = MAP_KIND::NORMAL;
        if (actual != kind || ++seen != ordinal) return false;
        ++hits;
        return true;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
    {
        if (!out) return E_POINTER;
        if (id == __uuidof(IUnknown) || id == __uuidof(ID3D11DeviceChild) || id == __uuidof(ID3D11DeviceContext))
        { *out = this; AddRef(); return S_OK; }
        return real->QueryInterface(id,out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return real->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return real->Release(); }
    void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) override
    {
        real->GetDevice(ppDevice);
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
    void STDMETHODCALLTYPE VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE VSSetShader(ID3D11VertexShader *pVertexShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override
    {
        real->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
    }
    void STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertexLocation) override
    {
        real->Draw(VertexCount, StartVertexLocation);
    }
    HRESULT STDMETHODCALLTYPE Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource) override
    {
        if (failMap(pResource,MapType))
        {
            if (pMappedResource) *pMappedResource = {};
            return E_OUTOFMEMORY;
        }
        const HRESULT hr = real->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
        if (SUCCEEDED(hr)) balanced = mappedResources.insert(pResource).second && balanced;
        return hr;
    }
    void STDMETHODCALLTYPE Unmap(ID3D11Resource *pResource, UINT Subresource) override
    {
        if (mappedResources.erase(pResource) != 1) { balanced = false; return; }
        real->Unmap(pResource, Subresource);
    }
    void STDMETHODCALLTYPE PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout *pInputLayout) override
    {
        real->IASetInputLayout(pInputLayout);
    }
    void STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets) override
    {
        real->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }
    void STDMETHODCALLTYPE IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) override
    {
        real->IASetIndexBuffer(pIndexBuffer, Format, Offset);
    }
    void STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override
    {
        real->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
    }
    void STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override
    {
        real->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
    }
    void STDMETHODCALLTYPE GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->GSSetShader(pShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) override
    {
        real->IASetPrimitiveTopology(Topology);
    }
    void STDMETHODCALLTYPE VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE Begin(ID3D11Asynchronous *pAsync) override
    {
        real->Begin(pAsync);
    }
    void STDMETHODCALLTYPE End(ID3D11Asynchronous *pAsync) override
    {
        real->End(pAsync);
    }
    HRESULT STDMETHODCALLTYPE GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override
    {
        return real->GetData(pAsync, pData, DataSize, GetDataFlags);
    }
    void STDMETHODCALLTYPE SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue) override
    {
        real->SetPredication(pPredicate, PredicateValue);
    }
    void STDMETHODCALLTYPE GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView) override
    {
        real->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
    }
    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts) override
    {
        real->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
    }
    void STDMETHODCALLTYPE OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[ 4 ], UINT SampleMask) override
    {
        real->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
    }
    void STDMETHODCALLTYPE OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) override
    {
        real->OMSetDepthStencilState(pDepthStencilState, StencilRef);
    }
    void STDMETHODCALLTYPE SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets) override
    {
        real->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
    }
    void STDMETHODCALLTYPE DrawAuto() override
    {
        real->DrawAuto();
    }
    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override
    {
        real->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    }
    void STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override
    {
        real->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    }
    void STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override
    {
        real->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
    }
    void STDMETHODCALLTYPE DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override
    {
        real->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    }
    void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState *pRasterizerState) override
    {
        real->RSSetState(pRasterizerState);
    }
    void STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports) override
    {
        real->RSSetViewports(NumViewports, pViewports);
    }
    void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) override
    {
        real->RSSetScissorRects(NumRects, pRects);
    }
    void STDMETHODCALLTYPE CopySubresourceRegion(ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox) override
    {
        real->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
    }
    void STDMETHODCALLTYPE CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource) override
    {
        real->CopyResource(pDstResource, pSrcResource);
    }
    void STDMETHODCALLTYPE UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) override
    {
        real->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
    }
    void STDMETHODCALLTYPE CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) override
    {
        real->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
    }
    void STDMETHODCALLTYPE ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[ 4 ]) override
    {
        real->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
    }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[ 4 ]) override
    {
        real->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
    }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[ 4 ]) override
    {
        real->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
    }
    void STDMETHODCALLTYPE ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) override
    {
        real->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
    }
    void STDMETHODCALLTYPE GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) override
    {
        real->GenerateMips(pShaderResourceView);
    }
    void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD) override
    {
        real->SetResourceMinLOD(pResource, MinLOD);
    }
    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource *pResource) override
    {
        return real->GetResourceMinLOD(pResource);
    }
    void STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) override
    {
        real->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
    }
    void STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override
    {
        real->ExecuteCommandList(pCommandList, RestoreContextState);
    }
    void STDMETHODCALLTYPE HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override
    {
        real->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts) override
    {
        real->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
    }
    void STDMETHODCALLTYPE CSSetShader(ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override
    {
        real->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
    }
    void STDMETHODCALLTYPE CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override
    {
        real->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override
    {
        real->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout **ppInputLayout) override
    {
        real->IAGetInputLayout(ppInputLayout);
    }
    void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets) override
    {
        real->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }
    void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) override
    {
        real->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
    }
    void STDMETHODCALLTYPE GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) override
    {
        real->IAGetPrimitiveTopology(pTopology);
    }
    void STDMETHODCALLTYPE VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue) override
    {
        real->GetPredication(ppPredicate, pPredicateValue);
    }
    void STDMETHODCALLTYPE GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView) override
    {
        real->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
    }
    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override
    {
        real->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
    }
    void STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[ 4 ], UINT *pSampleMask) override
    {
        real->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
    }
    void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef) override
    {
        real->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
    }
    void STDMETHODCALLTYPE SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets) override
    {
        real->SOGetTargets(NumBuffers, ppSOTargets);
    }
    void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState **ppRasterizerState) override
    {
        real->RSGetState(ppRasterizerState);
    }
    void STDMETHODCALLTYPE RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports) override
    {
        real->RSGetViewports(pNumViewports, pViewports);
    }
    void STDMETHODCALLTYPE RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects) override
    {
        real->RSGetScissorRects(pNumRects, pRects);
    }
    void STDMETHODCALLTYPE HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override
    {
        real->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override
    {
        real->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
    }
    void STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override
    {
        real->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
    }
    void STDMETHODCALLTYPE CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override
    {
        real->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }
    void STDMETHODCALLTYPE CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override
    {
        real->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }
    void STDMETHODCALLTYPE ClearState() override
    {
        real->ClearState();
    }
    void STDMETHODCALLTYPE Flush() override
    {
        real->Flush();
    }
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() override
    {
        return real->GetType();
    }
    UINT STDMETHODCALLTYPE GetContextFlags() override
    {
        return real->GetContextFlags();
    }
    HRESULT STDMETHODCALLTYPE FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override
    {
        return real->FinishCommandList(RestoreDeferredContextState, ppCommandList);
    }
};
#endif
