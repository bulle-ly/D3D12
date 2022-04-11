#include"D3DAPP.h"
#include<windowsX.h>

using namespace std;


using  Microsoft::WRL::ComPtr;

//��Ϣ������ ע��۲����������ε���
LRESULT	CALLBACK
MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	//������Ϣ������
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
	//���ｫ��Ϣ����������Ϊ���麯��������д
}

D3DApp* D3DApp::mApp = nullptr;


D3DApp* D3DApp::GetApp()
{
	return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
	:mhAppInst(hInstance)
{
	assert(mApp == nullptr);//һ���������쳣����Ķ��������������߳�ʼ��ʧ�����ͻ��ӡ����
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();//ǿ��cpu�ȴ�gpuֱ��gpu����������е���������

}

HINSTANCE D3DApp::AppInstance() const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;//ǿת����
}

bool D3DApp::Get4xMsaaState()const
{
	return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;
		CreateSwapChain();
		OnResize();//�����������ߴ�
	}
}

int D3DApp::Run()
{
	MSG msg = {0};
	
	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();//����֡��
				Update(mTimer);
				Draw(mTimer);
			}
			else {
				Sleep(100);
			}

		}
	}
	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;
	if (!InitDirct3D())
		return false;

	OnResize();
	return true;
}

//��ʼ��һЩ3D�豸  �����������ѵĴ�С֮���
bool D3DApp::InitDirct3D()
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr, D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));
	mRtvDescirptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	CreatCommandObjects();
	CreateSwapChain();
	CreatRtvAndDsvDescriptorHeaps();
		return true;
}
void D3DApp::CreatRtvAndDsvDescriptorHeaps()
{

	//������������
	//�����������洢������Ҫ�õ���������
	//rtv������ Render Target View
	//��Ȳ���ģ����ͼ Depth/Stencil View 
	//�ж���BufferCount ����������Ҳ�� ���Ǹ�������洢���ٸ�RTV 
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount;//�����������������ĸ�����ͬ�ڻ���֡��������

	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(mRtvHeap.GetAddressOf())));
	
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};

	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.Type= D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}
//�޸Ļ�������С
void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mCommandListAllco);

	//���޸�ǰһ��Ҫˢ���������
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mCommandListAllco.Get(), nullptr));

	//���û���������
	for (int  i = 0; i < mSwapChainBufferCount; i++)
	{
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	//���ý�����  ����Ǹ��������������޸Ĵ��ڴ�С��ʱ��ı���ʾ����Ĵ�С
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mSwapChainBufferCount, mClientWidth, mClientHeight
		, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));
	mCurrBackBuffer = 0;


	//�ṹ ��������һ�ֶ��͵�gpu����Դ�����������������ṹ ��Ϊ��Դ��Ҫ����������
	// �������ֱ���Ϊ ��ȾĿ����ͼ��Դ �������ͼ��Դ
	//�������� ���ʾ���һ������������������Ŷ��������
	//ÿ����������Ҫ��Ӧһ��ר�ŵĺ�̨������

	//������ȾĿ����ͼ
	//Ϊ�˽���̨�������󶨵� ��ˮ�ߵ�����ϲ��׶� ��ҪΪ��̨����������һ����ȾĿ����ͼ
	//������Ҫ��ȡ�������еĻ�������Դ ����GetBuffer����
	//��ν�ĺ�̨������Ҳ���� rendertargetҲ������ȾĿ�� Ҳ����һ������ʱд�������
	/*
	SwapChain::GetBuffer{
	���ڴ�����̨������
	UINT Buffer.ϣ����õ��ض���̨������������ ��ʱ���ж����̨������
	REFIID riid ϣ����õ�id3d12 ��Դ��comid
	void **ppSurface һ��ָ��  source��Դ��ָ��Ҳ����ϣ����õĺ�̨������
	}

	*/
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle
	(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < mSwapChainBufferCount; i++)
	{

		ThrowIfFailed(mSwapChain->GetBuffer(i,IID_PPV_ARGS(&mSwapChainBuffer[i])));//��̨�������Ļ�ȡ
		//���ں�̨���Ӻ�̨������com�����ü���
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get()
			, nullptr
			, rtvHeapHandle);
		//Ϊ�˻�ȡ�ĺ�̨��������ȡ��Ӧ��������
		//ÿһ����̨��������Ҫ��Ӧһ�������� ���������Ժ���������Ҫƫ��
		rtvHeapHandle.Offset(1, mRtvDescirptorSize);
	}

	//�������/ģ����ͼ
	D3D12_RESOURCE_DESC	 depthStencilDesc = {  };
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;//������Ϊ��λ����ʾ��������� ��������������Ĵ�С
	depthStencilDesc.MipLevels = 1;//������Ȼ���ֻ����һ��mipmap�㼶
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//���������Դָ��Ϊ���

	//��һ��  gpu��Դ�������ڶ����� �����Ǿ����ض����Ե�gpu�Դ�� device�ṩ���� �����ύ��Դ
	//����������൱��һ��ָ����ٶ�λ ����Ҫ����Դ
	//�������������ṩ�����Դ���һ����Դһ���� ������Դ�ύ��������


	//��������Ҫָ��һ������Ż�ֵ ����������ÿ�������Ϊnullptr

	CD3DX12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Stencil = 0;
	optClear.DepthStencil.Depth = 1.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,//��������Դϣ���ύ�����йصĶ���ѡ���־
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,//������ʲôʱ����Դ������һ��״̬ �����ǰ�������ΪĬ��״̬
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));
	//������Ȼ���
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());//��Ȼ���ֻ��һ��

	  // Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	//���һ�������ӿ�
	mScreenViewport.TopLeftY = 0.0f;
	mScreenViewport.TopLeftX = 0.0f;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;


	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

}
//��Ϣ������
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE://һ�����ڱ��������ʧȥ����״̬
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
	case WM_SIZE:
		mClientHeight = LOWORD(lParam);
		mClientWidth = HIWORD(lParam);

		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{


				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}


				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing) {

				}
				else
				{
					OnResize();
				}
			}
		}
	case WM_ENTERSIZEMOVE:
		//�û�ץȡ������ʱ������Ϣ
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		//�û��ͷŵ�����ʱ����
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	case WM_DESTROY://��������ʱ
		PostQuitMessage(0);
		return 0;

	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_GETMINMAXINFO://��ֹ���ڹ�С
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);


		return 0;
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
}
bool D3DApp::InitMainWindow()
{
	WNDCLASSEX wx = {};

	wx.cbSize		= sizeof(WNDCLASSEX);
	wx.style		= CS_GLOBALCLASS;
	wx.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wx.hCursor		= LoadCursor(nullptr, IDC_ARROW);;
	wx.lpfnWndProc	= MainWndProc;
	wx.cbClsExtra	= 0;
	wx.cbWndExtra	= 0;
	wx.hInstance	= mhAppInst;
	wx.hIcon		= nullptr;
	wx.hCursor		= nullptr;
	wx.lpszMenuName = nullptr;
	wx.lpszClassName = WINDOW_CLASS_NAME;
	wx.hIconSm		= nullptr;

	if (!RegisterClassEx(&wx))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	RECT R = { 0,0,mClientWidth,mClientHeight };

	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);

	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(WINDOW_CLASS_NAME,
		mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height, 0, 0, mhAppInst, 0);

	if (!mhMainWnd) {
		MessageBox(0, L"CreatWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);
	return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),mCurrBackBuffer,mRtvDescirptorSize);
}

void D3DApp::CreatCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};//����һ��������нṹ��
	//ָ��������нṹ������
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueue.GetAddressOf())));


	/*void ID3D12CommandQueue::ExecuteCommandLists() {
		UINT COUNT;
		�ڶ��������������б��������б������
		ID3D12CommandList *const *ppcommandList
		��ִ�е������б����飬ָ�������б�ĵ�һ��Ԫ��ָ��
	};
	id3d12graphicsCommandList�Ǽ̳��������б��
	�������б���ӽ�����Ҫ�ǵõ���close�ص�
	nodeMask ����ֻ��һ��gpu��ϵͳ����Ϊ0
	���ڶ��gpu ����������ڵ����룩ָ�������������������б������gpu
	*/

	//iidppv����com�ӿڶ���ָ�����ָ�� com�ӿ��������ǹ���������Ϊ0ʱ�Զ���������Ȼ���ö�Ӧ��desc
	//����com�ӿ����ɵĶ���


	//	allocator ����������ӿڶ��� ����һ���ڴ���� �������ÿ�������б�������ԷŶ���ָ����ָ��
	//	��ֹ�ڴ��˷�

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT//һ��������������������������� �����б����� �˴�ʹ�ÿ���ֱ�ӱ�gpuִ�е�������б�����õ�֮�������
		, IID_PPV_ARGS(mCommandListAllco.GetAddressOf()))//��������Ӧ��com�ӿ�id
	);//��ȷ��gpuִ���������е�����֮ǰ��Ҫ�������������



	//���������б���� �����������Ķ������뵽�����б�����ſ���ִ�� Ȼ�������б���һ��״̬
	// ���������Ҫ�� ��Ⱦ���߶����ʼ��
	// �������Ŵ򿪺͹رյ�״̬ ͨ��reset�������õ���������з���
	//ExecuteComandLists�� commandqueue�Ľӿڷ��� ���Խ������б������������ӵ����������ȥ
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0//nodeMask ����ֻ��һ��gpu��ϵͳ����Ϊ0���ڶ��gpu ����������ڵ����룩ָ�������������������б������gpu
		, D3D12_COMMAND_LIST_TYPE_DIRECT
		, mCommandListAllco.Get()//����Ҫ����һ��ָ���������ָ�� ��com��get�����õ�
		, nullptr//ָ�������б����Ⱦ��ˮ�߳�ʼ״̬
		, IID_PPV_ARGS(mCommandList.GetAddressOf())));
	mCommandList->Close();
}
void D3DApp::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = mSwapChainBufferCount;//���������õĻ���������
	swapChainDesc.BufferDesc.Width= mClientWidth;//flag ָ������ģʽ
	swapChainDesc.BufferDesc.Height = mClientHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //��������Ⱦ����̨������
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = m4xMsaaState?4:1;//���ز��������������Լ�ÿ�����صĲ������� 
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	swapChainDesc.OutputWindow = mhMainWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	//������ʵ���ϵ�swapchain���� Ϊswapchain1
	//����������ʵ������һ���������.get������
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&swapChainDesc,
		mSwapChain.GetAddressOf()));
}
//����֡��
void D3DApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed=0.0f;
	frameCnt++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
		//����������ǰ����1��ʱ�� ����һ���ڹ�ȥ��ÿ�θ��¼�����
		//�Ĺ����й��˶���֡�� �� ����1���Ͻ��м���Ȼ�����֡��������
	{
		float fps = float(frameCnt);
		float mspf = 1000 / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;
		SetWindowText(mhMainWnd, windowText.c_str());
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}
void D3DApp::LogAdapters()
{
}
void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
}
//�õ���Ȼ���
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
//�õ���ǰ����������
ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}
//ˢ���������
void D3DApp::FlushCommandQueue()
{
	mCurrentFence++;
	//��ǰΧ��ֵ�Ӽ�
	// ��ֹcpu��ǰ�޸���Դ���»��ƴ���
	//������������������������Χ��������
	//�������������gpu����������gpu��������������д�signal
	//�����������һ��Χ�������n+1 Ҳ�����������n+1��Χ��
	//Ȼ��ͨ��Χ���� �õ����ֵ�����ж�Χ���Ƿ�ͨ���������Ƿ���Ҫ������cpu���ݸ���
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
	
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		//GPU����Χ�� Ҳ����ִ�е�singnalָ��  ���Դ���Ԥ���¼�����ͨ��
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence,eventHandle));
		//�ȴ�gpu����Χ��
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);

	}

}
void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT)
{

}
