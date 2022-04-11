#include"D3DAPP.h"
#include"MathHelper.h"
#include"UploadBuffer.h"





using Microsoft::WRL::ComPtr;
using namespace DirectX;




struct  BULLE_VERTEX1
{
	DirectX::XMFLOAT3 positon;
	DirectX::XMFLOAT4 color;
};

struct BULLE_VERTEX2
{
	DirectX::XMFLOAT3 Pos;		//pos��ƫ����Ϊ0
	DirectX::XMFLOAT3 Normal;	//����ƫ������12�ֽ�
	DirectX::XMFLOAT2 TEX0;		//ƫ������24�ֽ�
	DirectX::XMFLOAT2 TEX1;		//32�ֽ�
};

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};


class BoxApp :public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);//���캯���޷��̳�
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();
	//�����ʼ������ΪʲôҪ��ô��װ
	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;


	void	BuildCbvSrvUavDescriptorHeaps();
	void	BuildShadersAndInputLayout();
	void	BuildConstantBuffer();
	void	BuildRootSignature();
	void	BuildBoxGeometry();
	void    BuildPSO();

private:
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProject = MathHelper::Identity4x4();

	//�����ø����
	float mXita = 1.5f * XM_PI;
	float mBeta = XM_PIDIV4;
	float mRadiums = 5.0f;

	POINT mLastMousePos;
};
int WINAPI WinMain			//ʼ��Ϊ��
(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
	//ָ��ĳ���ַ��� ������������������ַ�	�������ڴ������
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		BoxApp MyBox(hInstance);
		if (!MyBox.Initialize()) {
			return 0;
		}
		return MyBox.Run();

	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"failed", MB_OK);
		//mb ok������һ���ǵ�ѡ�� mb yesno����һ���Ƿ�
		return 0;
	}
}
BoxApp::BoxApp(HINSTANCE hInstance)
	:D3DApp(hInstance) {}

BoxApp::~BoxApp() 
{}


bool BoxApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;
	
	ThrowIfFailed(mCommandList->Reset(mCommandListAllco.Get(), nullptr));
	
	BuildCbvSrvUavDescriptorHeaps();
	BuildConstantBuffer();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();
	
	

	

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	
	FlushCommandQueue();
	return true;
}
void BoxApp::OnResize()
{	
	D3DApp::OnResize();
	//���¼���ͶӰ����
	//�����ݺ��
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProject, P);

}


void BoxApp::Update(const GameTimer& gt)
{
	//DX����������ϵ
	float x = mRadiums * sinf(mXita) * cosf(mBeta);
	float z = mRadiums * sinf(mXita) * sinf(mBeta);//ǿת��Ϊf��ʽ
	float y = mRadiums * cosf(mXita);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);//�������������ϵ�е�λ��
	XMVECTOR target = XMVectorZero();//target������ĳ����
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProject);
	XMMATRIX mvp = world * view * proj;

	//���³���������
	//��ʱû���о�
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(mvp));

	mObjectCB->CopyData(0, objConstants);
}
void BoxApp::Draw(const GameTimer& gt)
{
	//���ü�¼���������ڴ�
	ThrowIfFailed(mCommandListAllco->Reset());

	//ͨ������executeCommandList�������б������������
	ThrowIfFailed(mCommandList->Reset(mCommandListAllco.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
	//������Դ����;ָʾ��״̬�ĸı� ����Դ�ӳ���ת��Ϊ��Ⱦ
	mCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);


	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };

	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(
		mBoxGeo->DrawArgs["box"].IndexCount,
		1, 0, 0, 0);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % mSwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState ,int x,int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	/*MessageBox(mhMainWnd,L"ddd", L"failed",MB_OK);*/
	SetCapture(mhMainWnd);
	//���ô��ڲ���һ��������ô��ǰ�����������붼�ᱻ��׽��Ϊ��Ըô���
	//�ͷ����� ͨ�� release ����������������ڰ���
}
void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}
void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.5f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.5f * static_cast<float>(y - mLastMousePos.y));

		mXita += dx;
		mBeta += dy;
		
		mBeta = MathHelper::Clamp(mBeta, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if((btnState&MK_RBUTTON)!=0)
	{
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);


		mRadiums += dy - dx;

		mRadiums = MathHelper::Clamp(mRadiums, 3.0f, 15.0f);
	}
	
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}


//
void BoxApp::BuildCbvSrvUavDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.NumDescriptors = 1;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
	
}

void BoxApp::BuildConstantBuffer()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));//����ʵ�ʳ�����������С�Զ�ȡ��

	//�õ�������������Դͷ����ַ
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
	//ƫ�Ƶ������������л��Ƶ�i����������ĳ�������
	int boxCBbufferIndex = 0;
	cbAddress += boxCBbufferIndex * objCBByteSize;


	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	md3dDevice->CreateConstantBufferView(
	&cbvDesc,mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}


void BoxApp::BuildRootSignature()
{
	//�ڻ��Ƶ���֮ǰ ��Ҫ�Ѳ�ͬ��ɫ����Ҫ����Դ�󶨵���Ⱦ��ˮ������
	//ʵ���� ��ͬ���͵���Դ�ᱻ�󶨵��ض��ļĴ��� register slot ������ɫ������
	//����˵�����������Ҫ���ǰ�b0�ĳ���������

	///������ǩ��
	//��ЩӦ�ó��򽫰󶨵���Ⱦ��ˮ���ϵ���Դ
	//���ǻᱻӳ�䵽��ɫ�����Ӧ����Ĵ���
	//��Ϊ��ɫ����ͨ����ǩ����ӳ�������ҵ�������һ��Ҫ�ṩ��Ӧ��ɫ����Ҫ��ȫ����Դ

	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 
		1,//����������
		0);//�󶨵��ļĴ���
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1,slotRootParameter,0,nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	//���涼���Ǻܶ�
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	//������� ���л��󶨵�ĳ�� hresult ��Ϊ����ֵ������
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	
	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout() 
{
	mvsByteCode=d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr,"VS","vs_5_0");
	mpsByteCode=d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS","ps_5_0");
	
	mInputLayout =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

}

//д��������Ϊ��ɾ����
void BoxApp::BuildBoxGeometry()
{
	std::array<BULLE_VERTEX1, 8> vertices =
	{
		BULLE_VERTEX1({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		BULLE_VERTEX1({XMFLOAT3(-1.0f, +1.0f, -1.0f),  XMFLOAT4(Colors::Black)}),
		BULLE_VERTEX1({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		BULLE_VERTEX1({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		BULLE_VERTEX1({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		BULLE_VERTEX1({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		BULLE_VERTEX1({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		BULLE_VERTEX1({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

		std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	}; 
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(BULLE_VERTEX1);
	const UINT ibByteSize = (UINT)vertices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();
	mBoxGeo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride = sizeof(BULLE_VERTEX1);
	mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mBoxGeo->DrawArgs["box"] = submesh;

}


void BoxApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
	//rsDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	rsDesc.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState = rsDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

