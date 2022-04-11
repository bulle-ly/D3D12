#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"
#include<tchar.h>

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


#define WINDOW_CLASS_NAME _T("window class")
#define WINDOW_TITLE	_T("WINDOW!")



class D3DApp{
	//渲染管线对象在实际中在创建

protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;//禁止生成默认构造函数
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public:

	static D3DApp* GetApp();

	HINSTANCE	AppInstance()const;//后加const表示这是一个只读函数
	HWND		MainWnd()const;//实际的窗口句柄 上面那个更像一个内存空间起始点
	float		AspectRatio()const;

	bool		Get4xMsaaState()const;
	void		Set4xMsaaState(bool value);//设置当前msaa状态

	int			Run();

	virtual bool Initialize();
	virtual	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void CreatRtvAndDsvDescriptorHeaps();
	virtual void OnResize();// 调整缓冲区尺寸
	virtual	void Update(const GameTimer& gt)=0;
	virtual void Draw(const GameTimer& gt) = 0;

	virtual void OnMouseDown(WPARAM btnState,int x,int y){}
	virtual void OnMouseUp(WPARAM btnState,int x,int y){}
	virtual void OnMouseMove(WPARAM btnState, int x, int y){}

protected:

	bool InitMainWindow();
	
	bool InitDirct3D();
	void CreatCommandObjects();
	void CreateSwapChain();

	void FlushCommandQueue();
	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE	CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE	DepthStencilView()const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:

	static D3DApp* mApp;

	HINSTANCE	mhAppInst = nullptr;
	HWND		mhMainWnd = nullptr;
	bool		mAppPaused = false;
	bool		mMinimized = false;
	bool		mResizing = false;
	bool		mFullscreenState = false;
	bool		mMaximized = false;

	bool		m4xMsaaState = false;
	UINT		m4xMsaaQuality = 0;
	UINT		mCurrentFence = 0;
	GameTimer	mTimer;

	//设备生成
	Microsoft::WRL::ComPtr<IDXGIFactory4>	mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>	mSwapChain;
	
	Microsoft::WRL::ComPtr<ID3D12Device>	md3dDevice;
	Microsoft::WRL::ComPtr<ID3D12Fence>		mFence;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandListAllco;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	   mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;//继承了命令队列

	static const int mSwapChainBufferCount = 3;
	int mCurrBackBuffer = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource>	mSwapChainBuffer[mSwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource>  mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT	   mScissorRect;

	UINT	mRtvDescirptorSize = 0;
	UINT	mDsvDescriptorSize = 0;
	UINT	mCbvSrvUavDescriptorSize = 0;//作用未知

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;
	std::wstring mMainWndCaption = L"WINDOW!";

};
