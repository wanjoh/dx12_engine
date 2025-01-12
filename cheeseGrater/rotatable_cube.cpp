#include "rotatable_cube.hpp"

#include <application.hpp>
#include <command_queue.hpp>
#include <window.hpp>

#include <algorithm>

using namespace DirectX;

struct VertexInput
{
	XMFLOAT3 position;
	XMFLOAT3 color;
};

namespace
{
float g_nearPlane = 0.1f;
float g_farPlane = 100.0f;
FLOAT g_clearColor[4] = { 0.2f, 0.6f, 0.8f, 1.0f };
float g_maxFov = 90.f;
float g_minFov = 12.f;

VertexInput g_cubeVertices[8] = {
{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
{ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }
};

WORD g_cubeIndices[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

const float g_magicMult = 0.01f;
}


RotatableCube::RotatableCube(const std::wstring& name, int width, int height, bool vSync)
    : Game(name, width, height, vSync)
    , m_scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_viewport(CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(width), static_cast<float>(height)))
    , m_fov(45.f)
    , m_contentLoaded(false)
    , m_fenceValues{0}
    , m_rotationDirection({0.f})
{
}

bool RotatableCube::LoadContent()
{
    auto device = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList = commandQueue->GetCommandList();

    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateVertexBuffer;
    UpdateBufferResource(commandList.Get(), &m_vertexBuffer, &intermediateVertexBuffer, _countof(g_cubeVertices), sizeof(VertexInput), g_cubeVertices);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = sizeof(g_cubeVertices);
    m_vertexBufferView.StrideInBytes = sizeof(VertexInput);

    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateIndexBuffer;
    UpdateBufferResource(commandList.Get(), &m_indexBuffer,
        &intermediateIndexBuffer, _countof(g_cubeIndices), sizeof(WORD), g_cubeIndices);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = sizeof(g_cubeIndices);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    // load shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"vertex_shader.cso", &vertexShaderBlob));
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"pixel_shader.cso", &pixelShaderBlob));

    // create vertex input layout 
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // TODO: create functions to make this function cleaner
    // create root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_PARAMETER1 rootParameters[1];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    // serialize the root signature
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

    // create the root signature
    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    // create pipeline state object
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS vs;
        CD3DX12_PIPELINE_STATE_STREAM_PS ps;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    pipelineStateStream.pRootSignature = m_rootSignature.Get();
    pipelineStateStream.inputLayout = { inputLayout, _countof(inputLayout) };
    pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.dsvFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.rtvFormats = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(pipelineStateStream), &pipelineStateStream
    };
    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState)));

    uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    m_contentLoaded = true;

    ResizeDepthBuffer(GetClientWidth(), GetClientHeight());

    return true;
}

void RotatableCube::UnloadContent()
{
}

void RotatableCube::OnUpdate(UpdateEventArgs& e)
{
    static uint32_t frameCount = 0;
    static double totalTime = 0.;

    Game::OnUpdate(e);

    totalTime += e.ElapsedTime;
    frameCount++;
    /*if (totalTime >= 1.0)
    {
        double fps = static_cast<double>(frameCount) / totalTime;
        char buffer[256];
        sprintf_s(buffer, "FPS: %f", fps);
        OutputDebugStringA(buffer);
        frameCount = 0;
        totalTime = 0.;
    }*/

    // model matrix
    static float xRot = 0.f;
    static float yRot = 0.f;
    xRot +=
        (m_rotationDirection.f[m_keyToIndex.at(KeyCode::W)] - m_rotationDirection.f[m_keyToIndex.at(KeyCode::S)])
        * g_magicMult;
    yRot +=
        (m_rotationDirection.f[m_keyToIndex.at(KeyCode::A)] - m_rotationDirection.f[m_keyToIndex.at(KeyCode::D)])
        * g_magicMult;
    m_modelMatrix = XMMatrixMultiply(XMMatrixRotationX(xRot), XMMatrixRotationY(yRot));

    // view matrix
    const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
    const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
    const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
    m_viewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    // projection
    float aspectRatio = GetClientWidth() / static_cast<float>(GetClientHeight());
    m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov), aspectRatio, g_nearPlane, g_farPlane);

}

void RotatableCube::OnRender(RenderEventArgs& e)
{
    Game::OnRender(e);

    auto commandQueue = Application::Get().GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();

    UINT currentBackBufferIndex = m_window->GetCurrentBackBufferIndex();
    auto backBuffer = m_window->GetCurrentBackBuffer();
    auto rtv = m_window->GetCurrentRenderTargetView();
    auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // clear render targets
    {
        TransitionResource(commandList, backBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        ClearRTV(commandList, rtv, g_clearColor);
        ClearDepth(commandList, dsv);
    }

    // set pipeline state and root signature
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // set up the input assembler
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    // set up the rasterizer state
    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    // bind the render targets
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // update mvp
    XMMATRIX mvp = XMMatrixMultiply(m_modelMatrix, m_viewMatrix);
    mvp = XMMatrixMultiply(mvp, m_projectionMatrix);
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvp, 0);

    // draw
    commandList->DrawIndexedInstanced(_countof(g_cubeIndices), 1, 0, 0, 0);

    // present
    {
        TransitionResource(commandList, backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_fenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);
        currentBackBufferIndex = m_window->Present();
        commandQueue->WaitForFenceValue(m_fenceValues[currentBackBufferIndex]);
    }
}

void RotatableCube::OnKeyPressed(KeyEventArgs& e)
{
    Game::OnKeyPressed(e);
    switch (e.Key)
    {
    case KeyCode::Escape:
        Application::Get().Quit(0);
        break;
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        m_window->ToggleFullscreen();
        break;
        }
    case KeyCode::V:
        m_window->ToggleVSync();
        break;
    case KeyCode::W:
    case KeyCode::S:
    case KeyCode::A:
    case KeyCode::D:
        UpdateRotation(e.Key);
        break;
    default:
        break;
    }
}

void RotatableCube::OnKeyReleased(KeyEventArgs& e)
{
    Game::OnKeyReleased(e);
    switch (e.Key)
    {
    case KeyCode::W:
    case KeyCode::S:
    case KeyCode::A:
    case KeyCode::D:
        UpdateRotation(e.Key, true);
        break;
    default:
        break;
    }
}

void RotatableCube::OnMouseWheel(MouseWheelEventArgs& e)
{
    m_fov -= e.WheelDelta;
    m_fov = std::clamp(m_fov, g_minFov, g_maxFov);
    char buffer[256];
    sprintf_s(buffer, "FoV: %f\n", m_fov);
    OutputDebugStringA(buffer);
}

void RotatableCube::OnResize(ResizeEventArgs& e)
{
    if (e.Width != GetClientWidth() || e.Height != GetClientHeight())
    {
        Game::OnResize(e);
        m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(e.Width),
            static_cast<float>(e.Height), 0.0f, 1.0f);
        ResizeDepthBuffer(e.Width, e.Height);
    }
}

void RotatableCube::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
    Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        beforeState, afterState);

    commandList->ResourceBarrier(1, &barrier);
}

void RotatableCube::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    FLOAT* clearColor)
{
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void RotatableCube::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
    FLOAT depth)
{
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void RotatableCube::UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource,
                                         ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
    auto device = Application::Get().GetDevice();
    size_t bufferSize = numElements * elementSize;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(pDestinationResource)));

    if (bufferData)
    {
        auto heapProperties1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc1 = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties1,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc1,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pIntermediateResource)
            ));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(), *pDestinationResource, *pIntermediateResource,
            0, 0, 1, &subresourceData);
    }
}

void RotatableCube::ResizeDepthBuffer(int width, int height)
{
    // TODO: this can also be split into 2 functions - create ds, and update dsv
    
    if (!m_contentLoaded)
    {
        // TODO: add error/debug logs
        return;
    }

    // flush any gpu commands that might be referencing the depth buffer
    Application::Get().Flush();

    width = std::max(1, width);
    height = std::max(1, height);
    auto device = Application::Get().GetDevice();

    D3D12_CLEAR_VALUE optimizedClearValue = {};
    optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    optimizedClearValue.DepthStencil = {1.0f, 0};

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDescTex = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1,
        0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescTex,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optimizedClearValue,
        IID_PPV_ARGS(&m_depthBuffer)
        ));

    // update depth-stencil view
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void RotatableCube::UpdateRotation(KeyCode::Key key, bool released)
{
    if (m_keyToIndex.find(key) == m_keyToIndex.end())
    {
        return;
    }
    m_rotationDirection.f[m_keyToIndex.at(key)] = (released) ? 0 : 1.f;
}
