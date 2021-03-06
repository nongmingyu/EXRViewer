// ViewEXR.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "EXRViewer.h"
#include "resource.h"
#include "panel.h"
#include "viewimage.h"
#include <commdlg.h>

// Include the v6 common controls in the manifest
#pragma comment(linker, \
	"\"/manifestdependency:type='Win32' "\
	"name='Microsoft.Windows.Common-Controls' "\
	"version='6.0.0.0' "\
	"processorArchitecture='*' "\
	"publicKeyToken='6595b64144ccf1df' "\
	"language='*'\"")

#define MAX_LOADSTRING 100

void OnCreate(HWND hWnd);
void OnDestroy();
void OnSize(HWND hWnd, int width, int height);

BOOL FetchOpenFileName(HWND hWnd, PWSTR pszOutputName, PWSTR pszTitle = nullptr);

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
MenuMan *g_pMenuMan = nullptr;
CtlPanel *g_pPanel = nullptr;
ImgPanel *g_pImgWnd = nullptr;
ImageViewer *g_pViewer = nullptr;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	hInst = hInstance;
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VIEWEXR, szWindowClass, MAX_LOADSTRING);

	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VIEWEXR));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_VIEWEXR);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_VIEWEXR));
	RegisterClassExW(&wcex);

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VIEWEXR));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
		OnCreate(hWnd);
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
			case IDM_OPEN:
			{
				WCHAR filename[MAX_PATH] = {};
				FetchOpenFileName(hWnd, filename);
				g_pViewer->OpenImage(filename);
			}
				break;
			case IDM_CLOSE:
				g_pViewer->CloseImage();
				break;
			case IDM_ACTUALSIZE:
			{
				bool bActual = !(g_pViewer->GetOption(OPT_ACTUALSIZE));
				g_pViewer->SetOption(OPT_ACTUALSIZE, bActual? 1: 0);
				g_pMenuMan->CheckMainItem(IDM_ACTUALSIZE, bActual);
			}
				break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

	case WM_SIZE:
		OnSize(hWnd, LOWORD(lParam), HIWORD(lParam));
		break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
		OnDestroy();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void OnCreate(HWND hWnd)
{
	g_pMenuMan = new MenuMan(hWnd);
	g_pPanel = new CtlPanel(0, 0, 0, 0, hWnd, hInst);
	g_pImgWnd = new ImgPanel(0, 0, 0, 0, hWnd, hInst);
	g_pViewer = new ImageViewer(g_pImgWnd->GetHWND());
	g_pViewer->SetMenuMan(g_pMenuMan);
	g_pViewer->SetPanel(g_pPanel);
	g_pPanel->SetViewer(g_pViewer);
	g_pImgWnd->SetViewer(g_pViewer);

	g_pMenuMan->CheckMainItem(IDM_ACTUALSIZE, g_pViewer->GetOption(OPT_ACTUALSIZE) ? true : false);
}

void OnSize(HWND hWnd, int width, int height)
{
	if (width && height)
	{
		//int right_w = g_pPanel->GetWidth();
		//int left_w = width - right_w;
		//g_pPanel->Move(left_w, 0, right_w, height, true);
		//g_pImgWnd->Move(0, 0, left_w, height, true);

		int bottom_h = g_pPanel->GetHeight();
		int top_h = height - bottom_h;
		g_pImgWnd->Move(0, 0, width, top_h, true);
		g_pPanel->Move(0, top_h, width, bottom_h, true);

		g_pViewer->UpdateDisplayRect();
	}
}

void OnDestroy()
{
	SAFEDELETE(g_pMenuMan);
	SAFEDELETE(g_pViewer);
	SAFEDELETE(g_pPanel);
	SAFEDELETE(g_pImgWnd);
}

BOOL FetchOpenFileName(HWND hWnd, PWSTR pszOutputName, PWSTR pszTitle /*= nullptr*/)
{
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEW);
	ofn.hInstance = hInst;
	ofn.hwndOwner = hWnd;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFile = pszOutputName;
	ofn.lpstrTitle = pszTitle;
	ofn.lpstrInitialDir = nullptr;
	ofn.Flags = OFN_READONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

	ofn.lpstrFile[0] = L'\0';

	if (!ofn.lpstrTitle)
	{
		ofn.lpstrTitle = L"Open an EXR file";
	}
	ofn.lpstrFilter = L"EXR Files (*.exr)\0*.exr\0" \
			L"All Files (*.*)\0*.*\0\0";
	ofn.lpstrDefExt = L"exr";

	return GetOpenFileNameW(&ofn);
}
