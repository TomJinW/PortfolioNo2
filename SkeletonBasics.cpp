//------------------------------------------------------------------------------
// <copyright file="SkeletonBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------
#include "Square.h"
#include "stdafx.h"
#include <strsafe.h>
#include "SkeletonBasics.h"
//#include "resource.h"
#include <stdio.h>
#include <iostream>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib,"WINMM.LIB")

using namespace std;

Square obj1 = Square(0,0,0.15,30,false);
Square obj2 = Square(0,0,0.20,40,false);
Square obj3 = Square(0,0,0.23,50,false);
Square obj4 = Square(0,0,0.37,60,false);
Square obj5 = Square(0,0,0.35,40,false);

bool gameOver = false;
bool gameStarted = false;
bool gamePaused = false;
int gameinited = false;
int healthCount = 1;
int recoverPoints = 1000;
int hiScore = 0;
int combo = 0;
int life = 3;
int exetimes = 0;
int YALINtimes = 0;
int YALINtimesAll = 0;
int score = 0;
float g_strTrackedBoneThickness = 3.0f;
float g_TrackedBoneThickness = 6.0f;
float g_numTrackedBoneThickness = 6.0f;
static const float g_JointThickness = 3.0f;
//static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    CSkeletonBasics application;
    application.Run(hInstance, nCmdShow);
}

/// <summary>
/// Constructor
/// </summary>
CSkeletonBasics::CSkeletonBasics() :
    m_pD2DFactory(NULL),
    m_hNextSkeletonEvent(INVALID_HANDLE_VALUE),
    m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
    m_bSeatedMode(false),
    m_pRenderTarget(NULL),
    m_pBrushJointTracked(NULL),
    m_pBrushJointInferred(NULL),
    m_pBrushBoneTracked(NULL),
    m_pBrushBoneInferred(NULL),
    m_pNuiSensor(NULL)
{
    ZeroMemory(m_Points,sizeof(m_Points));
}

/// <summary>
/// Destructor
/// </summary>
CSkeletonBasics::~CSkeletonBasics()
{
    if (m_pNuiSensor)
    {
        m_pNuiSensor->NuiShutdown();
    }

    if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
    {
        CloseHandle(m_hNextSkeletonEvent);
    }

    // clean up Direct2D objects
    DiscardDirect2DResources();

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    SafeRelease(m_pNuiSensor);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CSkeletonBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc  = {0};

    // Dialog custom window class
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"SkeletonBasicsAppDlgWndClass";
	
    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        hInstance,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CSkeletonBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
	
    ShowWindow(hWndApp, nCmdShow);
	SetWindowPos(hWndApp, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetFocus(hWndApp);

    const int eventCount = 1;
    HANDLE hEvents[eventCount];

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        hEvents[0] = m_hNextSkeletonEvent;

        // Check to see if we have either a message (by passing in QS_ALLEVENTS)
        // Or a Kinect event (hEvents)
        // Update() will check for Kinect events individually, in case more than one are signalled
        MsgWaitForMultipleObjects(eventCount, hEvents, FALSE, INFINITE, QS_ALLINPUT);

        // Explicitly check the Kinect frame event since MsgWaitForMultipleObjects
        // can return for other reasons even though it is signaled.
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if ((hWndApp != NULL) && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

/// <summary>
/// Main processing function
/// </summary>
void CSkeletonBasics::Update()
{
    if (NULL == m_pNuiSensor)
    {
        return;
    }

    // Wait for 0ms, just quickly test if it is time to process a skeleton
    if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0) )
    {
        ProcessSkeleton();
    }
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CSkeletonBasics* pThis = NULL;

    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CSkeletonBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }

    else
    {
        pThis = reinterpret_cast<CSkeletonBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Look for a connected Kinect, and create it if found
            CreateFirstConnected();
        }
        break;

        // If the titlebar X is clicked, destroy app
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        // Quit the main message pump
        PostQuitMessage(0);
        break;

        // Handle button press
    case WM_COMMAND:
        // If it was for the near mode control and a clicked event, change near mode
        if (IDC_CHECK_SEATED == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
        {
            // Toggle out internal state for near mode
            m_bSeatedMode = !m_bSeatedMode;

            if (NULL != m_pNuiSensor)
            {
                // Set near mode for sensor based on our internal state
                m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, m_bSeatedMode ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0);
            }
        }
        break;
    }

    return FALSE;
}

/// <summary>
/// Create the first connected Kinect found 
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CSkeletonBasics::CreateFirstConnected()
{
    INuiSensor * pNuiSensor;

    int iSensorCount = 0;
    HRESULT hr = NuiGetSensorCount(&iSensorCount);
    if (FAILED(hr))
    {
        return hr;
    }

    // Look at each Kinect sensor
    for (int i = 0; i < iSensorCount; ++i)
    {
        // Create the sensor so we can check status, if we can't create it, move on to the next
        hr = NuiCreateSensorByIndex(i, &pNuiSensor);
        if (FAILED(hr))
        {
            continue;
        }

        // Get the status of the sensor, and if connected, then we can initialize it
        hr = pNuiSensor->NuiStatus();
        if (S_OK == hr)
        {
            m_pNuiSensor = pNuiSensor;
            break;
        }

        // This sensor wasn't OK, so release it since we're not using it
        pNuiSensor->Release();
    }

    if (NULL != m_pNuiSensor)
    {
        // Initialize the Kinect and specify that we'll be using skeleton
        hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON); 
        if (SUCCEEDED(hr))
        {
            // Create an event that will be signaled when skeleton data is available
            m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

            // Open a skeleton stream to receive skeleton data
            hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0); 
        }
    }

    if (NULL == m_pNuiSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!");
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new skeleton data
/// </summary>
void CSkeletonBasics::ProcessSkeleton()
{
    NUI_SKELETON_FRAME skeletonFrame = {0};

    HRESULT hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
    if ( FAILED(hr) )
    {
        return;
    }

    // smooth out the skeleton data
    m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

    // Endure Direct2D is ready to draw
    hr = EnsureDirect2DResources( );
    if ( FAILED(hr) )
    {
        return;
    }

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear( );

    RECT rct;
    GetClientRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rct);
    int width = rct.right;
    int height = rct.bottom;

    for (int i = 0 ; i < NUI_SKELETON_COUNT; ++i)
    {
        NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

        if (NUI_SKELETON_TRACKED == trackingState)
        {
            // We're tracking the skeleton, draw it
            DrawSkeleton(skeletonFrame.SkeletonData[i], width, height);
        }
        else if (NUI_SKELETON_POSITION_ONLY == trackingState)
        {
            // we've only received the center point of the skeleton, draw that
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                SkeletonToScreen(skeletonFrame.SkeletonData[i].Position, width, height),
                g_JointThickness,
                g_JointThickness
                );

            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }
    }

    hr = m_pRenderTarget->EndDraw();

    // Device lost, need to recreate the render target
    // We'll dispose it now and retry drawing
    if (D2DERR_RECREATE_TARGET == hr)
    {
        hr = S_OK;
        DiscardDirect2DResources();
    }
}void CSkeletonBasics::DrawStr(int x,int y,string letter,int Len){
	int count=letter.length();
	for (int i = 0 ; i<count;i++){
		char Test = letter[i];
		DrawSingleChar(x+Len*i+10*i,y,Test,Len);
	}
}
void CSkeletonBasics::DrawSingleChar(int x,int y,char letter,int Len){
	bool A,B,C,D,E,F,G,H,I,J,K,L,M;
	//letter = 65;
	A = false;B = false;C = false;D = false; E = false; F= false; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
	
	switch (letter){
	
	case 65:
		A = true;B = true;C = true;D = false; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 66:
		A = false;B = false;C = true;D = true; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 67:
		A = true;B = false;C = false;D = true; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 68:
		A = false;B = true;C = true;D = true; E = true; F= false; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 69:
		A = true;B = false;C = false;D = true; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 70:
		A = true;B = false;C = false;D = false; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 71:
		A = true;B = false;C = true;D = true; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 72:
		A = false;B = true;C = true;D = false; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 73:
		A = true;B = false;C = false;D = true; E = false; F= false; G = false;H = false;I = true;J = false,K = false,L = true, M = false ;
		break;
	case 74:
		A = true;B = true;C = true;D = false; E = false; F= false; G = false;H = false;I = false;J = false,K = true,L = false, M = false ;
		break;
	case 75:
		A = false;B = false;C = false;D = false; E = true; F= true; G = false;H = false;I = false;J = true,K = true,L = false, M = false ;
		break;
	case 76:
		A = false;B = false;C = false;D = true; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 77:
		A = true;B = true;C = true;D = false; E = true; F= true; G = false;H = false;I = true;J = false,K = false,L = false, M = false ;
		break;
	case 78:
		A = false;B = true;C = true;D = false; E = true; F= true; G = false;H = true;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 79:
		A = true;B = true;C = true;D = true; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 80:
		A = true;B = true;C = false;D = false; E = true; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 81:
		A = true;B = true;C = true;D = false; E = false; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 82:
		A = true;B = true;C = false;D = false; E = true; F= true; G = true;H = false;I = false;J = false,K = true,L = false, M = false ;
		break;
	case 83:
		A = true;B = false;C = true;D = true; E = false; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 84:
		A = true;B = false;C = false;D = false; E = false; F= false; G = false;H = false;I = true;J = false,K = false,L = true, M = false ;
		break;
	case 85:
		A = false;B = true;C = true;D = true; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 86:
		A = false;B = true;C = false;D = false; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = false, M = true ;
		break;
	case 87:
		A = false;B = true;C = true;D = true; E = true; F= true; G = false;H = false;I = false;J = false,K = false,L = true, M = false ;
		break;
	case 88:
		A = false;B = false;C = false;D = false; E = false; F= false; G = false;H = false;I = false;J = false,K = true,L = false, M = true ;
		break;
	case 89:
		A = false;B = true;C = true;D = true; E = false; F= true; G = true;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 90:
		A = true;B = false;C = false;D = true; E = true; F= false; G = false;H = false;I = false;J = true,K = false,L = false, M = false ;
		break;
	case 95:
		A = false;B = false;C = false;D = true; E = false; F= false; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	case 32:
		A = false;B = false;C = false;D = false; E = false; F= false; G = false;H = false;I = false;J = false,K = false,L = false, M = false ;
		break;
	}
	if (A){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);
	}
	if (B){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);
	}
	if (C){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len,y+Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (D){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+2*Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (E){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (F){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (G){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (H){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);
	}
	if (I){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len/2,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len/2,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (J){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (K){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (L){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len/2,y+L);
		D2D1_POINT_2F kk = D2D1::Point2F(x+Len/2,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}
	if (M){
		D2D1_POINT_2F ll = D2D1::Point2F(x+Len,y+Len);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+2*Len);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_strTrackedBoneThickness);	
	}

}

void CSkeletonBasics::DrawNum(int x,int y,int number,int L,int count){
	int base = 1;
	for (int i = 1;i <= count;i++){
		base = base * 10;
	}
	int input = number % base;
	int digit = count-1;
	while (digit >= 0){
		int num = input % 10;
		DrawSingleNum(x+digit*L+10*digit,y,num,L);
		input = input /10;
		digit--;
	}
}
void CSkeletonBasics::DrawSingleNum(int x,int y,int number,int L){
	bool A,B,C,D,E,F,G;
	switch (number){
	
	case 0:
		A = true;B = true;C = true;D = true; E = true; F= true; G = false;
		break;
	case 1:
		A = false;B = true;C = true;D = false; E = false; F= false; G = false;
		break;
	case 2:
		A = true;B = true;C = false;D = true; E = true; F= false; G = true;
		break;
	case 3:
		A = true;B = true;C = true;D = true; E = false; F= false; G = true;
		break;
	case 4:
		A = false;B = true;C = true;D = false; E = false; F= true; G = true;
		break;
	case 5:
		A = true;B = false;C = true;D = true; E = false; F= true; G = true;
		break;
	case 6:
		A = true;B = false;C = true;D = true; E = true; F= true; G = true;
		break;
	case 7:
		A = true;B = true;C = true;D = false; E = false; F= false; G = false;
		break;
	case 8:
		A = true;B = true;C = true;D = true; E = true; F= true; G = true;
		break;
	case 9:
		A = true;B = true;C = true;D = true; E = false; F= true; G = true;
		break;

	}
	if (A){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+L,y);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);
	}
	if (B){
		D2D1_POINT_2F ll = D2D1::Point2F(x+L,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x+L,y+L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);
	}
	if (C){
		D2D1_POINT_2F ll = D2D1::Point2F(x+L,y+L);
		D2D1_POINT_2F kk = D2D1::Point2F(x+L,y+2*L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}
	if (D){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+2*L);
		D2D1_POINT_2F kk = D2D1::Point2F(x+L,y+2*L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}
	if (E){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+L);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+2*L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}
	if (F){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y);
		D2D1_POINT_2F kk = D2D1::Point2F(x,y+L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}
	if (G){
		D2D1_POINT_2F ll = D2D1::Point2F(x,y+L);
		D2D1_POINT_2F kk = D2D1::Point2F(x+L,y+L);
		m_pRenderTarget->DrawLine(ll, kk, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}

}
void CSkeletonBasics::DrawRect(Square sqr){
	if (sqr.isValid == true){
		D2D1_POINT_2F A1 = D2D1::Point2F(sqr.x,sqr.y);
		D2D1_POINT_2F A2 = D2D1::Point2F(sqr.x+sqr.sqrsize,sqr.y);
		D2D1_POINT_2F A3 = D2D1::Point2F(sqr.x+sqr.sqrsize,sqr.y+sqr.sqrsize);
		D2D1_POINT_2F A4 = D2D1::Point2F(sqr.x,sqr.y+sqr.sqrsize);
		m_pRenderTarget->DrawLine(A1, A2, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A2, A3, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A3, A4, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A4, A1, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
	}
}
void CSkeletonBasics::showLife(int life,int scrHeight){
		switch (life){
	case 3:
		DrawStr(10,scrHeight-80,"OOO",30);
		break;
	case 2:
		DrawStr(10,scrHeight-80,"OOX",30);
		break;
	case 1:
		DrawStr(10,scrHeight-80,"OXX",30);
		break;
	case 0:
		DrawStr(10,scrHeight-80,"XXX",30);
		life = 3;
		break;	
	}
}

void CSkeletonBasics::hitDetect(Square sqr,int x,int y){
	if (sqr.isHitted(x,y) == true){
		score = score + 1 + combo;
		combo ++;
	}
}

void CSkeletonBasics::bottomDetect(Square sqr,int width,int height){
		if (sqr.isReachedBottom(height) == -1){
			sqr.randAppear(width);
			life = life -1;
		}
		if (sqr.isReachedBottom(height) == 1){
			sqr.randAppear(width);
		}
}
/// <summary>
/// Draws a skeleton
/// </summary>
/// <param name="skel">skeleton to draw</param>
/// <param name="windowWidth">width (in pixels) of output buffer</param>
/// <param name="windowHeight">height (in pixels) of output buffer</param>
void CSkeletonBasics::DrawSkeleton(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight)
{   

    int i;
	RECT rc;
    GetWindowRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );  
    int scrWidth = rc.right - rc.left;
    int scrHeight = rc.bottom - rc.top;


    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
        m_Points[i] = SkeletonToScreen(skel.SkeletonPositions[i], windowWidth, windowHeight);
    }
	
    // Render Torso
    DrawBone(skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
    DrawBone(skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

    // Left Arm
	//
	NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT];
    NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_WRIST_LEFT];
	if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
    {
		if(m_Points[NUI_SKELETON_POSITION_SHOULDER_LEFT].y > m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y)
		{
			if(exetimes == 0 )
			{	
				exetimes = 1;
				YALINtimes ++;
				YALINtimesAll ++;
				//m_pRenderTarget->DrawLine(m_Points[NUI_SKELETON_POSITION_SHOULDER_LEFT], m_Points[NUI_SKELETON_POSITION_WRIST_LEFT], m_pBrushBoneTracked, g_TrackedBoneThickness);
			}
		}
    }

	if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
    {
		if(m_Points[NUI_SKELETON_POSITION_SHOULDER_LEFT].y < m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y)
		{
			if(exetimes == 1 )
			{	
				exetimes = 0;
				//m_pRenderTarget->DrawLine(m_Points[NUI_SKELETON_POSITION_SHOULDER_LEFT], m_Points[NUI_SKELETON_POSITION_WRIST_LEFT], m_pBrushBoneTracked, g_TrackedBoneThickness);
			}
		}
    }

	
	//DrawSingleChar(50,50,'A',30);
//				if(YALINtimesAll >= 10)
//			{
//				DrawStr(0,100,"GREAT_ GOOD JOB_",30);
//			}


	if (gameStarted == true){
		if (obj1.initialized == false){
			obj1.randAppear(scrWidth-60);obj2.randAppear(scrWidth-60);obj3.randAppear(scrWidth-60);obj4.randAppear(scrWidth-60);
		}
		DrawRect(obj1);DrawRect(obj2);DrawRect(obj3);DrawRect(obj4);
		if (obj1.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x,m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y) == true
		||(obj1.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x,m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y) == true)||
		obj1.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].x,m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y) == true
		||(obj1.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].x,m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].y) == true)||
		obj1.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].x,m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].y) == true
		||(obj1.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].x,m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].y) == true)){
			score = score + 1 + combo;
			combo ++;
		}
		if (obj2.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x,m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y) == true
		||(obj2.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x,m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y) == true)||
		obj2.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].x,m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y) == true
		||(obj2.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].x,m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].y) == true)||
		obj2.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].x,m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].y) == true
		||(obj2.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].x,m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].y) == true)){
			score = score + 1 + combo;
			combo ++;
		}
		if (obj3.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x,m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y) == true
		||(obj3.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x,m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y) == true)||
		obj3.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].x,m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y) == true
		||(obj3.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].x,m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].y) == true)||
		obj3.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].x,m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].y) == true
		||(obj3.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].x,m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].y) == true)		
		){
			score = score + 1 + combo;
			combo ++;
		}
		if (obj4.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x,m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y) == true
		||(obj4.isHitted(m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x,m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y) == true)||
		obj4.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].x,m_Points[NUI_SKELETON_POSITION_WRIST_LEFT].y) == true
		||(obj4.isHitted(m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].x,m_Points[NUI_SKELETON_POSITION_WRIST_RIGHT].y) == true)||
		obj4.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].x,m_Points[NUI_SKELETON_POSITION_KNEE_LEFT].y) == true
		||(obj4.isHitted(m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].x,m_Points[NUI_SKELETON_POSITION_KNEE_RIGHT].y) == true)		
		){
			score = score + 1 + combo;
			combo ++;
		}
		obj1.goDown(10+combo/5+score/500);obj2.goDown(15+combo/5+score/500);obj3.goDown(18+combo/5+score/500);obj4.goDown(15+combo/5+score/500);
			if (obj1.isReachedBottom(scrHeight) == -1){
				obj1.randAppear(scrWidth-60);
				life = life -1; combo = 0;
				PlaySound(L"down.wav", NULL, SND_ASYNC|SND_FILENAME);
			}
			if (obj1.isReachedBottom(scrHeight) == 1 || obj1.isValid == false){
				obj1.randAppear(scrWidth-60);
			}
			showLife(life,scrHeight);
			if (obj2.isReachedBottom(scrHeight) == -1 ){
				obj2.randAppear(scrWidth-60);
				life = life -1; combo = 0;
				PlaySound(L"down.wav", NULL, SND_ASYNC|SND_FILENAME);
			}
			if (obj2.isReachedBottom(scrHeight) == 1 || obj2.isValid == false){
				obj2.randAppear(scrWidth-60);
			}
			showLife(life,scrHeight);
			if (obj3.isReachedBottom(scrHeight) == -1 ){
				obj3.randAppear(scrWidth-60);
				life = life -1; combo = 0;
				PlaySound(L"down.wav", NULL, SND_ASYNC|SND_FILENAME);
			}
			if (obj3.isReachedBottom(scrHeight) == 1 || obj3.isValid == false){
				obj3.randAppear(scrWidth-60);
			}
			showLife(life,scrHeight);
			if (obj4.isReachedBottom(scrHeight) == -1 ){
				obj4.randAppear(scrWidth-60);
				life = life -1; combo = 0;
				PlaySound(L"down.wav", NULL, SND_ASYNC|SND_FILENAME);
			}
			if (obj4.isReachedBottom(scrHeight) == 1 || obj4.isValid == false){
				obj4.randAppear(scrWidth-60);
			}
			showLife(life,scrHeight);
		DrawNum(scrWidth-120,10,combo,30,3);
		DrawNum(scrWidth-250,scrHeight-80,hiScore,30,6);
		DrawNum(10,10,score,30,6);
		if (score >= healthCount*recoverPoints){
			if (life < 3){life++;}
			healthCount++;
		}
		int inx = 0; int iny = 0;
		if ((m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x >= inx-10 && m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x <= inx+200
			&& m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y >= iny-10 && m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y <= iny+50)
			&& (m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x >= inx-10 && m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x <= inx+200
			&& m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y >= iny-10 && m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y <= iny+50))
		{
			PlaySound(L"C:\\hit.wav", NULL, SND_ASYNC|SND_FILENAME);
			gameStarted = false;
		}
		if (life == 0){
			if (score > hiScore){
				hiScore = score;
			}
			PlaySound(L"over.wav", NULL, SND_ASYNC|SND_FILENAME);
			gameOver = true;
			gamePaused = false;
			gameStarted = false;		
			
		}
	}else{
		int inx = scrWidth/2 - 300; int iny = scrHeight/8 - 30;
		D2D1_POINT_2F A1 = D2D1::Point2F(inx-10,iny-10);
		D2D1_POINT_2F A2 = D2D1::Point2F(inx+600,iny-10);
		D2D1_POINT_2F A3 = D2D1::Point2F(inx+600,iny+50);
		D2D1_POINT_2F A4 = D2D1::Point2F(inx-10,iny+50);
		m_pRenderTarget->DrawLine(A1, A2, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A2, A3, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A3, A4, m_pBrushBoneTracked, g_numTrackedBoneThickness);	
		m_pRenderTarget->DrawLine(A4, A1, m_pBrushBoneTracked, g_numTrackedBoneThickness);	

		if (gameOver == false){
			if (gamePaused==false){
				DrawStr(scrWidth/2 - 300,scrHeight/4 - 30,"ALIENS ARE ATTACKING",15);
				DrawStr(scrWidth/2 - 300,scrHeight/3 - 30,"OUR EARTH_ HELP US",15);
				DrawStr(scrWidth/2 - 300,scrHeight/8 - 30,"BOTH HAND HERE TO START",15);
			}else{
				DrawStr(scrWidth/2 - 300,scrHeight/4 - 30,"ALIENS ARE STILL ATTACKING",15);
				DrawStr(scrWidth/2 - 300,scrHeight/3 - 30,"OUR EARTH_ GAME PAUSED_",15);
				DrawStr(scrWidth/2 - 300,scrHeight/8 - 30,"BOTH HANDS HERE CONTINUE",15);
			}
			DrawStr(scrWidth/2 - 300,scrHeight/2 - 30,"YOU ARE A GIANT GUARDIAN",15);
			DrawStr(scrWidth/2 - 300,scrHeight/1.5 - 30,"HIT SQUARE TO WIN",15);
		}else{

			DrawStr(scrWidth/2 - 300,scrHeight/4 - 30,"ALIENS HAVE DESTROYED",15);
			DrawStr(scrWidth/2 - 300,scrHeight/3 - 30,"OUR EARTH_ SAD STORY",15);
			
			DrawStr(scrWidth/2 - 300,scrHeight/2 - 30,"BETTER LUCK NEXT TIME",15);
			DrawStr(scrWidth/2 - 300,scrHeight/1.5 - 30,"TOTAL SCORE",15);
			DrawNum(scrWidth/2 - 300,scrHeight/1.2 - 30,score,30,6);
			DrawStr(scrWidth/2 - 300,scrHeight/8 - 30,"BOTH HAND HERE TO START",15);
		}

		

		
		if ((m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x >= inx-10 && m_Points[NUI_SKELETON_POSITION_HAND_LEFT].x <= inx+600
			&& m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y >= iny-10 && m_Points[NUI_SKELETON_POSITION_HAND_LEFT].y <= iny+50)
			&& (m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x >= inx-10 && m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].x <= inx+600
			&& m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y >= iny-10 && m_Points[NUI_SKELETON_POSITION_HAND_RIGHT].y <= iny+50))
		{
			PlaySound(L"C:\\hit.wav", NULL, SND_ASYNC|SND_FILENAME);
			if (gameinited == false){
				//sf::Music music;
				//music.openFromFile("C:\\bgm.wav");
				//music.play();
				//gameinited = true;
			}
			gameOver = false;
			gameStarted = true;
			if (gamePaused == false){
				score = 0;
				combo = 0;
				life = 3;
				obj1.initialized = false;
			}
			gamePaused = true;
		}
	}

	//DrawSingleNum(60,60,1);
	//DrawSingleNum(110,110,2);
	//DrawSingleNum(160,160,3);
	//DrawSingleNum(210,210,4);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);









    // Right Arm
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

    // Left Leg
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

    // Right Leg
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);

    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[i], g_JointThickness, g_JointThickness );

        if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointInferred);
        }
        else if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }
    }
}

/// <summary>
/// Draws a bone line between two joints
/// </summary>
/// <param name="skel">skeleton to draw bones from</param>
/// <param name="joint0">joint to start drawing from</param>
/// <param name="joint1">joint to end drawing at</param>
void CSkeletonBasics::DrawBone(const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX joint0, NUI_SKELETON_POSITION_INDEX joint1)
{
    NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[joint0];
    NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[joint1];

    // If we can't find either of these joints, exit
    if (joint0State == NUI_SKELETON_POSITION_NOT_TRACKED || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
    {
        return;
    }

    // Don't draw if both points are inferred
    if (joint0State == NUI_SKELETON_POSITION_INFERRED && joint1State == NUI_SKELETON_POSITION_INFERRED)
    {
        return;
    }

    // We assume all drawn bones are inferred unless BOTH joints are tracked
    if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
    {
        m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneTracked, g_TrackedBoneThickness);
    }
    else
    {
        m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneInferred, g_InferredBoneThickness);
    }
}

/// <summary>
/// Converts a skeleton point to screen space
/// </summary>
/// <param name="skeletonPoint">skeleton point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F CSkeletonBasics::SkeletonToScreen(Vector4 skeletonPoint, int width, int height)
{
    LONG x, y;
    USHORT depth;

    // Calculate the skeleton's position on the screen
    // NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
    NuiTransformSkeletonToDepthImage(skeletonPoint, &x, &y, &depth);

    float screenPointX = static_cast<float>(x * width) / cScreenWidth;
    float screenPointY = static_cast<float>(y * height) / cScreenHeight;

    return D2D1::Point2F(screenPointX, screenPointY);
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CSkeletonBasics::EnsureDirect2DResources()
{
    HRESULT hr = S_OK;

    // If there isn't currently a render target, we need to create one
    if (NULL == m_pRenderTarget)
    {
        RECT rc;
        GetWindowRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );  

        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        D2D1_SIZE_U size = D2D1::SizeU( width, height );
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        rtProps.pixelFormat = D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

        // Create a Hwnd render target, in order to render to the window set in initialize
        hr = m_pD2DFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(GetDlgItem( m_hWnd, IDC_VIDEOVIEW), size),
            &m_pRenderTarget
            );
        if ( FAILED(hr) )
        {
            SetStatusMessage(L"Couldn't create Direct2D render target!");
            return hr;
        }

        //light green
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.27f, 0.75f, 0.27f), &m_pBrushJointTracked);

        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 1.0f), &m_pBrushJointInferred);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green, 1.0f), &m_pBrushBoneTracked);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 1.0f), &m_pBrushBoneInferred);
    }

    return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void CSkeletonBasics::DiscardDirect2DResources( )
{
    SafeRelease(m_pRenderTarget);

    SafeRelease(m_pBrushJointTracked);
    SafeRelease(m_pBrushJointInferred);
    SafeRelease(m_pBrushBoneTracked);
    SafeRelease(m_pBrushBoneInferred);
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
void CSkeletonBasics::SetStatusMessage(WCHAR * szMessage)
{
    SendDlgItemMessageW(m_hWnd, IDC_STATUS, WM_SETTEXT, 0, (LPARAM)szMessage);
}