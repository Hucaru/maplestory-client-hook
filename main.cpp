#include <windows.h>
#include <stdio.h>
#include <Include\d3d8.h> // Set the include path like this as it conflicts with windows 10 sdk unless using visual studio and manually adjusting include orders
#include "detours.h"

extern "C" __declspec(dllexport) void dummyExport()
{
    printf("You are calling the dummy export\n");
    return;
}

#define WINDOWNAME "Valhalla v28"
#define CONFIGNAME "valhalla.ini"
#define CREATEDEVICE_VTI 15

PDWORD IDirect3D8_vtable = NULL;
D3DDISPLAYMODE display_mode;
HWND window = NULL;

struct config
{
    BOOL debug_print;
    BOOL windowed;
    int width;
    int height;
    BOOL screen_centre;
};

config settings = {};

void parse_config()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    for (int i = MAX_PATH - 1; i >= 0; --i)
    {
        if (path[i] == '\\')
        {
            ++i;
            for (int j = 0; j < strlen(CONFIGNAME); ++j)
            {
                path[i + j] = CONFIGNAME[j];
            }

            path[i + strlen(CONFIGNAME)] = '\0';

            break;
        }
    }

    settings.debug_print = GetPrivateProfileInt("Debug", "print", 0, path);

    settings.windowed = GetPrivateProfileInt("Display", "window_mode", 0, path);
    settings.width = GetPrivateProfileInt("Display", "resolution_x", 800, path);
    settings.height = GetPrivateProfileInt("Display", "resolution_y", 700, path);
    settings.screen_centre = GetPrivateProfileInt("Display", "screen_centre", 0, path);
}

typedef HWND(WINAPI* create_window_ex_ptr)(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
create_window_ex_ptr CreateWindowEx_original;

typedef LONG (WINAPI* set_window_long_a_ptr)(HWND hWnd, int nIndex, LONG dwNewLong);
set_window_long_a_ptr SetWindowLongA_original;

typedef BOOL (WINAPI* set_window_pos_ptr)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
set_window_pos_ptr SetWindowPos_original;

typedef HRESULT (WINAPI* CreateDevice_ptr)(IDirect3D8* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface);
CreateDevice_ptr CreateDevice_original;

HWND WINAPI CreateWindowEx_hook(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    printf("Creating window with name: %s\n", lpClassName);

    if (!strcmp((char*)lpClassName, "NexonADBallon") || !strcmp((char*)lpClassName, "NexonADBrowser"))
    {
        return NULL;
    }

    if (!strcmp((char*)lpClassName, "MapleStoryClass"))
    {
        if (settings.windowed)
        {
            dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        }

        window = CreateWindowEx_original(dwExStyle, lpClassName, (LPCTSTR)WINDOWNAME, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

        return window;
    }

    return CreateWindowEx_original(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

BOOL WINAPI SetWindowPos_hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    printf("SetWindowPos\n");
    if (settings.windowed && hWnd == window)
    {

        cx = settings.width;
        cy = settings.height;

        if (settings.screen_centre)
        {
            X = (GetSystemMetrics(SM_CXSCREEN) / 2) - (settings.width / 2);
            Y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (settings.height / 2);
        }
        
        uFlags = SWP_DRAWFRAME | SWP_FRAMECHANGED | SWP_SHOWWINDOW;
        hWndInsertAfter = HWND_NOTOPMOST;
    }
    
    return SetWindowPos_original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

LONG WINAPI SetWindowLongA_hook(HWND hWnd, int nIndex, LONG dwNewLong)
{
    printf("SetWindowLongA: %i, 0x%.8X\n", nIndex, dwNewLong);
    if (settings.windowed && hWnd == window)
    {
        dwNewLong = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    
    return SetWindowLongA_original(hWnd, nIndex, dwNewLong);;
}

HRESULT WINAPI CreateDevice_hook(IDirect3D8* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
    if (settings.windowed)
    {
        pPresentationParameters->Windowed = TRUE;
        pPresentationParameters->SwapEffect = D3DSWAPEFFECT::D3DSWAPEFFECT_DISCARD; // D3DSWAPEFFECT_FLIP for fullscreen
        pPresentationParameters->FullScreen_RefreshRateInHz = 0; // 60 for fullscreen

        
        pPresentationParameters->BackBufferFormat = display_mode.Format;
        pPresentationParameters->BackBufferWidth = 0;
        pPresentationParameters->BackBufferHeight = 0;
    }

    HRESULT result = CreateDevice_original(Direct3D_Object, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    DWORD protectFlag;
    if (VirtualProtect(&IDirect3D8_vtable[CREATEDEVICE_VTI], sizeof(DWORD), PAGE_READWRITE, &protectFlag))
	{
		*(DWORD*)&IDirect3D8_vtable[CREATEDEVICE_VTI] = (DWORD)CreateDevice_original;

		if (!VirtualProtect(&IDirect3D8_vtable[CREATEDEVICE_VTI], sizeof(DWORD), protectFlag, &protectFlag))
		{
			return D3DERR_INVALIDCALL;
		}
	} else {
		return D3DERR_INVALIDCALL;
	}

    // clean up vtable

    if (result == D3D_OK)
    {
        printf("d3d create ok\n");
        // Here we can hook new vtable IDirect3D8_vtable = (DWORD*)*(DWORD*)*ppReturnedDeviceInterface; if we are interested in other methods
    }

    return result;
}

BOOL apply_hook(__inout PVOID* ppvTarget, __in PVOID pvDetour)
{
	if (DetourTransactionBegin() != NO_ERROR)
    {
        printf("Failed to being detour\n");
        return FALSE;
    }

	if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
    {
        printf("Detour thread update failed\n");
        return FALSE;
    }

	if (DetourAttach(ppvTarget, pvDetour) != NO_ERROR)
    {
        printf("Failed detour attach\n");
        return FALSE;
    }

    if (DetourTransactionCommit() != NO_ERROR)
    {
        printf("Failed detour transaction commit\n");
        DetourTransactionAbort();
        return FALSE;
    }
    
	return TRUE;
}

BOOL hook()
{
    CreateWindowEx_original = &CreateWindowExA;
    if (!apply_hook((PVOID*)&CreateWindowEx_original, (PVOID)CreateWindowEx_hook))
    {
        printf("CreateWindowEx hook failed\n");
        return FALSE;
    }
    else
    {
        printf("CreateWindowEx hooked\n");
    }

    SetWindowLongA_original = &SetWindowLongA;
    if (!apply_hook((PVOID*)&SetWindowLongA_original, (PVOID)SetWindowLongA_hook))
    {
        printf("SetWindowLongA hook failed\n");
        return FALSE;
    }
    else
    {
        printf("SetWindowLongA hooked\n");
    }

    SetWindowPos_original = &SetWindowPos;
    if (!apply_hook((PVOID*)&SetWindowPos_original, (PVOID)SetWindowPos_hook))
    {
        printf("SetWindowPos hook failed\n");
        return FALSE;
    }
    else
    {
        printf("SetWindowPos hooked\n");
    }

    IDirect3D8* d3d_device = Direct3DCreate8(D3D_SDK_VERSION);

    if (!d3d_device)
    {
        printf("Failed to create d3d device\n");
        return FALSE;
    }

    IDirect3D8_vtable = (DWORD*)*(DWORD*)d3d_device;
    d3d_device->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &display_mode);
    d3d_device->Release();

    DWORD protectFlag;
	if (VirtualProtect(&IDirect3D8_vtable[CREATEDEVICE_VTI], sizeof(DWORD), PAGE_READWRITE, &protectFlag))
	{
		*(DWORD*)&CreateDevice_original = IDirect3D8_vtable[CREATEDEVICE_VTI];
		*(DWORD*)&IDirect3D8_vtable[CREATEDEVICE_VTI] = (DWORD)CreateDevice_hook;

		if (!VirtualProtect(&IDirect3D8_vtable[CREATEDEVICE_VTI], sizeof(DWORD), protectFlag, &protectFlag))
        {
            printf("VirtualProtect apply on CreateDevice vtable entry failed\n");    
            return FALSE;
        }

        printf("CreateDevice hooked\n");
    }
    else
    {
        printf("VirtualProtect remove on CreateDevice vtable entry failed\n");
        return FALSE;
    }
    

    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    parse_config();

    if (settings.debug_print)
    {
        AllocConsole();
        AttachConsole(GetCurrentProcessId());

        freopen("CON", "w", stdout);

        char title[128];
        sprintf_s(title, "Attached to: %i", GetCurrentProcessId());
        SetConsoleTitleA(title);
    }

    DisableThreadLibraryCalls(hinstDLL);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            return hook();
    }

    return TRUE;
}
   