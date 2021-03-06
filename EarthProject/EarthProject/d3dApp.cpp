//=======================================================================================
// d3dApp.cpp by Frank Luna (C) 2008 All Rights Reserved.
//=======================================================================================

#include <sstream>
#include <iostream>

#include "d3dApp.h"
#include "DevConsole.hpp"

using namespace std;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static D3DApp* app = 0;

  switch( msg )
  {
    case WM_CREATE:
    {
      DevConsole::create();
      // Get the 'this' pointer we passed to CreateWindow via the lpParam parameter.
      CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
      app = (D3DApp*)cs->lpCreateParams;

      return 0;
    }
  }

  // Don't start processing messages until after WM_CREATE.
  if( app )
    return app->msgProc(msg, wParam, lParam);
  else
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3DApp::D3DApp(HINSTANCE hInstance)
{
  mhAppInst   = hInstance;
  mhMainWnd   = 0;
  mAppPaused  = false;
  mMinimized  = false;
  mMaximized  = false;
  mResizing   = false;

  mFrameStats = L"";
 
  md3dDevice          = 0;
  mSwapChain          = 0;
  mDepthStencilBuffer = 0;
  m_depthDisabled	  = 0;
  mRenderTargetView   = 0;
  mDepthStencilView   = 0;
  mFont               = 0;
  pRS				  = 0;
  mMainWndCaption = L"EARTH PROJECT";
  md3dDriverType  = D3D10_DRIVER_TYPE_HARDWARE;
  mClearColor     = BLACK; //D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f);
  mClientWidth    = 800;
  mClientHeight   = 600;
}

D3DApp::~D3DApp()
{
  ReleaseCOM(mRenderTargetView);
  ReleaseCOM(mDepthStencilView);
  ReleaseCOM(mSwapChain);
  ReleaseCOM(mDepthStencilBuffer);
  ReleaseCOM(md3dDevice);
  ReleaseCOM(mFont);
  ReleaseCOM(pRS);
}

HINSTANCE D3DApp::getAppInst()
{
  return mhAppInst;
}

HWND D3DApp::getMainWnd()
{
  return mhMainWnd;
}

int D3DApp::run()
{
  MSG msg = {0};
 
  mTimer.reset();

  while(msg.message != WM_QUIT)
  {
    // If there are Window messages then process them.
    if(PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
    {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
    }
    // Otherwise, do animation/game stuff.
    else
    {	
      mTimer.tick();

      if( !mAppPaused )
        updateScene(mTimer.getDeltaTime());	
      else
        Sleep(20);

      drawScene();
     }
  }
  return (int)msg.wParam;
}

void D3DApp::initApp()
{
  initMainWindow();
  initDirect3D();

  D3DX10_FONT_DESC fontDesc;
  fontDesc.Height            = 24;
    fontDesc.Width           = 0;
    fontDesc.Weight          = 0;
    fontDesc.MipLevels       = 1;
    fontDesc.Italic          = false;
    fontDesc.CharSet         = DEFAULT_CHARSET;
    fontDesc.OutputPrecision = OUT_DEFAULT_PRECIS;
    fontDesc.Quality         = DEFAULT_QUALITY;
    fontDesc.PitchAndFamily  = DEFAULT_PITCH | FF_DONTCARE;
   wcscpy_s(fontDesc.FaceName, L"Times New Roman");

  D3DX10CreateFontIndirect(md3dDevice, &fontDesc, &mFont);
}
 
void D3DApp::onResize()
{
  // Release the old views, as they hold references to the buffers we
  // will be destroying.  Also release the old depth/stencil buffer.

  ReleaseCOM(mRenderTargetView);
  ReleaseCOM(mDepthStencilView);
  ReleaseCOM(mDepthStencilBuffer);

  // Resize the swap chain and recreate the render target view.

  HR(mSwapChain->ResizeBuffers(1, mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
  ID3D10Texture2D* backBuffer;
  HR(mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer)));
  HR(md3dDevice->CreateRenderTargetView(backBuffer, 0, &mRenderTargetView));
  ReleaseCOM(backBuffer); //The swap chain is built so we are finished with this COM object.

  // Create the depth/stencil buffer and view.

  D3D10_TEXTURE2D_DESC depthStencilDesc;
  
  depthStencilDesc.Width     = mClientWidth;
  depthStencilDesc.Height    = mClientHeight;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.ArraySize = 1;
  depthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depthStencilDesc.SampleDesc.Count   = 1; // Multisampling must match this. Number of samples per pixel.
  depthStencilDesc.SampleDesc.Quality = 0; // Swap chain values. Higher quaility more processing of the samples.
  depthStencilDesc.Usage          = D3D10_USAGE_DEFAULT;
  depthStencilDesc.BindFlags      = D3D10_BIND_DEPTH_STENCIL; //D3D10_BIND_RENDER_TARGET;
  depthStencilDesc.CPUAccessFlags = 0; 
  depthStencilDesc.MiscFlags      = 0;

  HR(md3dDevice->CreateTexture2D(&depthStencilDesc, 0, &mDepthStencilBuffer));
  HR(md3dDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));

  // Bind the render target view and depth/stencil view to the pipeline.
  md3dDevice->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

  D3D10_DEPTH_STENCIL_DESC depthDisabledDesc;
  	ZeroMemory(&depthDisabledDesc, sizeof(D3D10_DEPTH_STENCIL_DESC));

	// Now create a second depth stencil state which turns off the Z buffer for 2D rendering.  The only difference is 
	// that DepthEnable is set to false, all other parameters are the same as the other depth stencil state.
	depthDisabledDesc.DepthEnable = false;
	depthDisabledDesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
	depthDisabledDesc.DepthFunc = D3D10_COMPARISON_LESS;
	depthDisabledDesc.StencilEnable = true;
	depthDisabledDesc.StencilReadMask = 0xFF;
	depthDisabledDesc.StencilWriteMask = 0xFF;
	depthDisabledDesc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
	depthDisabledDesc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR;
	depthDisabledDesc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
	depthDisabledDesc.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
	depthDisabledDesc.BackFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
	depthDisabledDesc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR;
	depthDisabledDesc.BackFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
	depthDisabledDesc.BackFace.StencilFunc = D3D10_COMPARISON_ALWAYS;

	// Create the state using the device.
	
	if(FAILED( md3dDevice->CreateDepthStencilState(&depthDisabledDesc, &m_depthDisabled)))
	{
		MessageBoxW(0, L"Cannot Create Disable Stencil State", 0, 0);
	}






  // Set the viewport transform.

  D3D10_VIEWPORT vp;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  vp.Width    = mClientWidth;
  vp.Height   = mClientHeight;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;

  md3dDevice->RSSetViewports(1, &vp);

  D3D10_RASTERIZER_DESC rasterizerState;
	rasterizerState.CullMode = D3D10_CULL_BACK;
	rasterizerState.FillMode = D3D10_FILL_SOLID;
	rasterizerState.FrontCounterClockwise = false;
	rasterizerState.DepthBias = false;
	rasterizerState.DepthBiasClamp = 0;
	rasterizerState.SlopeScaledDepthBias = 0;
	rasterizerState.DepthClipEnable = true;
	rasterizerState.ScissorEnable = false;
	rasterizerState.MultisampleEnable = false;
	rasterizerState.AntialiasedLineEnable = true;

	
	md3dDevice->CreateRasterizerState( &rasterizerState, &pRS);
	md3dDevice->RSSetState(pRS);
}

void D3DApp::updateScene(float dt)
{
  // Code computes the average frames per second, and also the 
  // average time it takes to render one frame.

  static int frameCnt = 0;
  static float t_base = 0.0f;

  frameCnt++;

  // Compute averages over one second period.
  if( (mTimer.getGameTime() - t_base) >= 1.0f )
  {
    float fps = (float)frameCnt; // fps = frameCnt / 1
    float mspf = 1000.0f / fps;

    std::wostringstream outs;   
    outs.precision(6);
    outs << L"FPS: " << fps << L"\n" 
       << "Milliseconds: Per Frame: " << mspf;
    mFrameStats = outs.str();
    cout << "FPS: " << fps << " " 
       << "Milliseconds: Per Frame: " << mspf << endl;
    // Reset for next average.
    frameCnt = 0;
    t_base  += 1.0f;
  }
}

void D3DApp::drawScene()
{
  md3dDevice->ClearRenderTargetView(mRenderTargetView, mClearColor);
  md3dDevice->ClearDepthStencilView(mDepthStencilView, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
  
}

LRESULT D3DApp::msgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch( msg )
  {
  // WM_ACTIVATE is sent when the window is activated or deactivated.  
  // We pause the game when the window is deactivated and unpause it 
  // when it becomes active.  
  case WM_ACTIVATE:
    if( LOWORD(wParam) == WA_INACTIVE )
    {
      mAppPaused = true;
      mTimer.stop();
    }
    else
    {
      mAppPaused = false;
      mTimer.start();
    }
    return 0;

  // WM_SIZE is sent when the user resizes the window.  
  case WM_SIZE:
    // Save the new client area dimensions.
    mClientWidth  = LOWORD(lParam);
    mClientHeight = HIWORD(lParam);
    if( md3dDevice )
    {
      if( wParam == SIZE_MINIMIZED )
      {
        mAppPaused = true;
        mMinimized = true;
        mMaximized = false;
      }
      else if( wParam == SIZE_MAXIMIZED )
      {
        mAppPaused = false;
        mMinimized = false;
        mMaximized = true;
        onResize();
      }
      else if( wParam == SIZE_RESTORED )
      {
        
        // Restoring from minimized state?
        if( mMinimized )
        {
          mAppPaused = false;
          mMinimized = false;
          onResize();
        }

        // Restoring from maximized state?
        else if( mMaximized )
        {
          mAppPaused = false;
          mMaximized = false;
          onResize();
        }
        else if( mResizing )
        {
          // If user is dragging the resize bars, we do not resize 
          // the buffers here because as the user continuously 
          // drags the resize bars, a stream of WM_SIZE messages are
          // sent to the window, and it would be pointless (and slow)
          // to resize for each WM_SIZE message received from dragging
          // the resize bars.  So instead, we reset after the user is 
          // done resizing the window and releases the resize bars, which 
          // sends a WM_EXITSIZEMOVE message.
          onResize();
        }
        else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
        {
          onResize();
        }
      }
    }
    return 0;

  // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
  case WM_ENTERSIZEMOVE:
    //mAppPaused = true;
    mResizing  = true;
    //mTimer.stop();
    return 0;

  // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
  // Here we reset everything based on the new window dimensions.
  case WM_EXITSIZEMOVE:
    mAppPaused = false;
    mResizing  = false;
    //mTimer.start();
    onResize();
    return 0;
 
  // WM_DESTROY is sent when the window is being destroyed.
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  // The WM_MENUCHAR message is sent when a menu is active and the user presses 
  // a key that does not correspond to any mnemonic or accelerator key. 
  case WM_MENUCHAR:
   // Don't beep when we alt-enter.
   return MAKELRESULT(0, MNC_CLOSE);

  // Catch this message so to prevent the window from becoming too small.
  case WM_GETMINMAXINFO:
    ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
    ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
    return 0;
  }

  return DefWindowProc(mhMainWnd, msg, wParam, lParam);
}


void D3DApp::initMainWindow()
{
  WNDCLASS wc;
  wc.style         = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc   = MainWndProc; 
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = mhAppInst;
  wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
  wc.hCursor       = LoadCursor(0, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
  wc.lpszMenuName  = 0;
  wc.lpszClassName = L"D3DWndClassName";

  if( !RegisterClass(&wc) )
  {
    MessageBox(0, L"RegisterClass FAILED", 0, 0);
    PostQuitMessage(0);
  }

  // Compute window rectangle dimensions based on requested client area dimensions.
  RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
  int width  = R.right - R.left;
  int height = R.bottom - R.top;

  mhMainWnd = CreateWindow(L"D3DWndClassName", mMainWndCaption.c_str(), 
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, this); 
  if( !mhMainWnd )
  {
    MessageBox(0, L"CreateWindow FAILED", 0, 0);
    PostQuitMessage(0);
  }

  ShowWindow(mhMainWnd, SW_SHOW);
  UpdateWindow(mhMainWnd);
}

void D3DApp::initDirect3D()
{
  // Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.

  DXGI_SWAP_CHAIN_DESC sd;
  sd.BufferDesc.Width  = mClientWidth;
  sd.BufferDesc.Height = mClientHeight;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

  // No multisampling.
  sd.SampleDesc.Count   = 1;
  sd.SampleDesc.Quality = 0;

  sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT; //We render to the backbuffer render target.
  sd.BufferCount  = 1;
  sd.OutputWindow = mhMainWnd;
  sd.Windowed     = true; //False for full screen
  sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD; //Onced swapped don't save the backbuffer
  sd.Flags        = 0;


  // Create the device.

  UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

  HR( D3D10CreateDeviceAndSwapChain(
      0,                 //default adapter
      md3dDriverType,
      0,                 // no software device
      createDeviceFlags, 
      D3D10_SDK_VERSION,
      &sd,
      &mSwapChain,
      &md3dDevice) );


  // The remaining steps that need to be carried out for d3d creation
  // also need to be executed every time the window is resized.  So
  // just call the onResize method here to avoid code duplication.
  
  onResize();
  SetBackBufferRenderTarget();
}


void D3DApp::SetBackBufferRenderTarget()
{
	md3dDevice->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

}
void D3DApp::ZBuffOff()
{
	md3dDevice->OMSetDepthStencilState(m_depthDisabled, 1);
}

void D3DApp::ZBuffOn()
{
	md3dDevice->OMSetDepthStencilState(0, 0);
}

void D3DApp::resetViewport()
{  
	D3D10_VIEWPORT vp;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  vp.Width    = mClientWidth;
  vp.Height   = mClientHeight;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
	 md3dDevice->RSSetViewports(1, &vp);

}