#include"D3DAPP.h"
#include<windowsX.h>

using namespace std;


using  Microsoft::WRL::ComPtr;

//消息处理函数 注意观察这个东西如何调用
LRESULT	CALLBACK
MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	//处理消息处理函数
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
	//这里将消息处理函数定义为了虚函数可以重写
}

D3DApp* D3DApp::mApp = nullptr;


D3DApp* D3DApp::GetApp()
{
	return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
	:mhAppInst(hInstance)
{
	assert(mApp == nullptr);//一个类似于异常捕获的东西如果有问题或者初始化失败他就会打印出来
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();//强制cpu等待gpu直到gpu处理完队列中的所有命令

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
	return static_cast<float>(mClientWidth) / mClientHeight;//强转类型
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
		OnResize();//调整缓存区尺寸
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
				CalculateFrameStats();//计算帧数
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

//初始化一些3D设备  比如描述符堆的大小之类的
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

	//创建描述符堆
	//描述符堆来存储程序中要用到的描述符
	//rtv描述符 Render Target View
	//深度测试模版视图 Depth/Stencil View 
	//有多少BufferCount 缓存区数量也就 在那个堆里面存储多少个RTV 
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount;//描述符堆中描述符的个数等同于缓存帧数的数字

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
//修改缓存区大小
void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mCommandListAllco);

	//在修改前一定要刷新命令队列
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mCommandListAllco.Get(), nullptr));

	//重置缓存区数据
	for (int  i = 0; i < mSwapChainBufferCount; i++)
	{
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	//重置交换链  最后那个宏作用是允许修改窗口大小的时候改变显示区域的大小
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mSwapChainBufferCount, mClientWidth, mClientHeight
		, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));
	mCurrBackBuffer = 0;


	//结构 描述符是一种对送到gpu的资源进行描述的轻量级结构 因为资源需要被反复利用
	// 描述符又被分为 渲染目标视图资源 和深度视图资源
	//描述符堆 本质就是一个描述符数组用来存放多个描述符
	//每个描述符需要对应一个专门的后台缓冲区

	//创建渲染目标视图
	//为了将后台缓冲区绑定到 流水线的输出合并阶段 需要为后台缓冲区创建一个渲染目标视图
	//所以需要获取交换链中的缓冲区资源 利用GetBuffer函数
	//所谓的后台缓冲区也就是 rendertarget也就是渲染目标 也就是一种运行时写入的纹理
	/*
	SwapChain::GetBuffer{
	是在创建后台缓存区
	UINT Buffer.希望获得的特定后台缓冲区的索引 有时候有多个后台缓存区
	REFIID riid 希望获得的id3d12 资源的comid
	void **ppSurface 一个指向  source资源的指针也就是希望获得的后台缓冲区
	}

	*/
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle
	(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < mSwapChainBufferCount; i++)
	{

		ThrowIfFailed(mSwapChain->GetBuffer(i,IID_PPV_ARGS(&mSwapChainBuffer[i])));//后台缓冲区的获取
		//会在后台增加后台缓冲区com的引用计数
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get()
			, nullptr
			, rtvHeapHandle);
		//为了获取的后台缓冲区获取对应的描述符
		//每一个后台缓冲区需要对应一个描述符 所以用完以后描述符需要偏移
		rtvHeapHandle.Offset(1, mRtvDescirptorSize);
	}

	//创建深度/模版视图
	D3D12_RESOURCE_DESC	 depthStencilDesc = {  };
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;//以纹素为单位来表示的纹理深度 或者是纹理数组的大小
	depthStencilDesc.MipLevels = 1;//对于深度缓存只能有一个mipmap层级
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//对于深度资源指定为这个

	//下一步  gpu资源都存在于堆里面 本质是具有特定属性的gpu显存块 device提供方法 创建提交资源
	//描述符句柄相当于一个指针快速定位 所需要的资源
	//将根据我们所提供的属性创建一个资源一个堆 并把资源提交到堆里面


	//但是他需要指定一个清除优化值 如果不想设置可以设置为nullptr

	CD3DX12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Stencil = 0;
	optClear.DepthStencil.Depth = 1.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,//这里是资源希望提交到堆有关的额外选项标志
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,//不管是什么时候资源都处于一种状态 这里是把他设置为默认状态
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));
	//创建深度缓存
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());//深度缓存只有一个

	  // Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	//最后一步设置视口
	mScreenViewport.TopLeftY = 0.0f;
	mScreenViewport.TopLeftX = 0.0f;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;


	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

}
//消息处理函数
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE://一个窗口被激活或者失去激活状态
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
		//用户抓取调整栏时发送消息
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		//用户释放调整栏时发生
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	case WM_DESTROY://窗口销毁时
		PostQuitMessage(0);
		return 0;

	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_GETMINMAXINFO://防止窗口过小
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
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};//制造一个命令队列结构体
	//指定命令队列结构体类型
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueue.GetAddressOf())));


	/*void ID3D12CommandQueue::ExecuteCommandLists() {
		UINT COUNT;
		第二个参数里命令列表中命令列表的数量
		ID3D12CommandList *const *ppcommandList
		待执行的命令列表数组，指向命令列表的第一个元素指针
	};
	id3d12graphicsCommandList是继承于命令列表的
	在命令列表添加结束后要记得调用close关掉
	nodeMask 对于只有一个gpu的系统设置为0
	对于多个gpu 这个东西（节点掩码）指定的是与所建造命令列表关联的gpu
	*/

	//iidppv返回com接口对于指定类的指针 com接口易于我们管理，在引用为0时自动销毁自身，然后用对应的desc
	//描述com接口生成的对象


	//	allocator 命令分配器接口对象 他是一个内存管理 负责管理每个命令列表里面可以放多少指定的指令
	//	防止内存浪费

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT//一种描述与这个命令分配器相关联的 命令列表类型 此处使用可以直接被gpu执行的命令，还有被打包好的之类的类型
		, IID_PPV_ARGS(mCommandListAllco.GetAddressOf()))//分配器对应的com接口id
	);//在确定gpu执行完了所有的命令之前不要重置命令分配器



	//创建命令列表对象 命令队列里面的东西加入到命令列表里面才可以执行 然后命令列表是一种状态
	// 命令队列需要用 渲染管线对象初始化
	// 他是有着打开和关闭的状态 通过reset方法重置掉里面的已有方法
	//ExecuteComandLists是 commandqueue的接口方法 可以将命令列表里面的命令添加到命令队列中去
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0//nodeMask 对于只有一个gpu的系统设置为0对于多个gpu 这个东西（节点掩码）指定的是与所建造命令列表关联的gpu
		, D3D12_COMMAND_LIST_TYPE_DIRECT
		, mCommandListAllco.Get()//类型要求是一个指向分配器的指针 用com的get方法得到
		, nullptr//指定命令列表的渲染流水线初始状态
		, IID_PPV_ARGS(mCommandList.GetAddressOf())));
	mCommandList->Close();
}
void D3DApp::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = mSwapChainBufferCount;//交换连所用的缓存区数量
	swapChainDesc.BufferDesc.Width= mClientWidth;//flag 指定窗口模式
	swapChainDesc.BufferDesc.Height = mClientHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //将数据渲染到后台缓冲区
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = m4xMsaaState?4:1;//多重采样的质量级别以及每个像素的采样次数 
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	swapChainDesc.OutputWindow = mhMainWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	//创建出实际上的swapchain对象 为swapchain1
	//交换链对象实际上用一个命令队列.get创建的
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&swapChainDesc,
		mSwapChain.GetAddressOf()));
}
//计算帧数
void D3DApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed=0.0f;
	frameCnt++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
		//当计数器向前走了1的时候 计算一次在过去的每次更新计数器
		//的过程中过了多少帧数 当 差了1以上进行计算然后计算帧数并更新
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
//得到深度缓存
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
//得到当前缓冲区对象
ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}
//刷新命令队列
void D3DApp::FlushCommandQueue()
{
	mCurrentFence++;
	//当前围栏值加加
	// 防止cpu提前修改资源导致绘制错误
	//向命令队列里面添加用来设置围栏的命令
	//由于这条命令交由gpu处理所以在gpu处理完命令队列中此signal
	//这个函数传入一个围栏对象和n+1 也就是制造出第n+1个围栏
	//然后通过围栏的 得到完成值方法判断围栏是否被通过来决定是否需要继续用cpu数据更新
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
	
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		//GPU命中围栏 也就是执行到singnal指令  所以触发预定事件允许通过
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence,eventHandle));
		//等待gpu命中围栏
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);

	}

}
void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT)
{

}
