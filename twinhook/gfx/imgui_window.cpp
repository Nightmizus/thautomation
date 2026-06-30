/* 
 * Most of the code here is taken from:
 * dear imgui: standalone example application for DirectX 9
 */
#include "imgui_window.h"

#include <imgui.h>
#include <examples/imgui_impl_dx9.h>
#include <examples/imgui_impl_win32.h>

#include <d3d9.h>
#undef DIRECTINPUT_VERSION 
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#include <atomic>
#include <thread>

#include <spdlog/spdlog.h>

static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static WNDCLASSEX wc;
static HWND hwnd;
static LPDIRECT3D9 pD3D;
static MSG msg;
static bool frame_active = false;
static std::thread ui_thread;
static std::atomic_bool ui_stop{ false };
static std::atomic_bool ui_started{ false };

static bool show_demo_window = true;
static bool show_another_window = false;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

static std::atomic_int stat_bullets{ 0 };
static std::atomic_int stat_enemies{ 0 };
static std::atomic_int stat_powerups{ 0 };
static std::atomic_int stat_lasers{ 0 };
static std::atomic_bool stat_bot_enabled{ false };
static std::atomic_bool stat_render_enabled{ false };
static std::atomic_bool request_toggle_bot{ false };
static std::atomic_bool request_toggle_debug{ false };

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			g_d3dpp.BackBufferWidth = LOWORD(lParam);
			g_d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
			if (SUCCEEDED(hr))
				ImGui_ImplDX9_CreateDeviceObjects();
			else
				SPDLOG_WARN("IMGUI external window reset failed: {}", hr);
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool imgui_window_create()
{
	SPDLOG_INFO("Initializing IMGUI external window and renderer");
	// Create application window
	wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Twinject Debugger"), NULL };
	RegisterClassEx(&wc);
	hwnd = CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_TOPMOST, _T("Twinject Debugger"), _T("Twinject Debugging Window"),
		WS_OVERLAPPEDWINDOW, 100, 100, 400, 800, NULL, NULL, wc.hInstance, NULL);
	if (!hwnd)
	{
		UnregisterClass(_T("Twinject Debugger"), wc.hInstance);
		return false;
	}

	// Initialize Direct3D
	if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
	{
		UnregisterClass(_T("Twinject Debugger"), wc.hInstance);
		return false;
	}
	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	//g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Present with vsync
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // Present without vsync, maximum unthrottled framerate

	// Create the D3DDevice
	if (pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
	{
		pD3D->Release();
		UnregisterClass(_T("Twinject Debugger"), wc.hInstance);
		return false;
	}

	// Setup Dear ImGui binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	// Setup style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	ZeroMemory(&msg, sizeof(msg));
	ShowWindow(hwnd, SW_SHOWNOACTIVATE);
	SetWindowPos(hwnd, HWND_TOPMOST, 100, 100, 400, 800, SWP_NOACTIVATE | SWP_SHOWWINDOW);
	UpdateWindow(hwnd);
	return true;
}

static void imgui_window_destroy()
{
	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	if (g_pd3dDevice)
	{
		g_pd3dDevice->Release();
		g_pd3dDevice = NULL;
	}
	if (pD3D)
	{
		pD3D->Release();
		pD3D = NULL;
	}
	if (hwnd)
	{
		DestroyWindow(hwnd);
		hwnd = NULL;
	}
	if (wc.hInstance)
		UnregisterClass(_T("Twinject Debugger"), wc.hInstance);
}

static bool imgui_window_begin_frame()
{
	frame_active = false;
	if (msg.message == WM_QUIT)
		return false;
	if (!hwnd || !IsWindow(hwnd) || !g_pd3dDevice)
		return false;

	RECT rect{};
	if (!GetClientRect(hwnd, &rect) || rect.right <= rect.left || rect.bottom <= rect.top)
		return false;

	HRESULT coop = g_pd3dDevice->TestCooperativeLevel();
	if (coop == D3DERR_DEVICELOST)
		return false;
	if (coop == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		if (FAILED(g_pd3dDevice->Reset(&g_d3dpp)))
			return false;
		ImGui_ImplDX9_CreateDeviceObjects();
	}

	// Poll and handle messages (inputs, window resize, etc.)
	// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
	// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
	// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
	// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
	while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		// TODO should we render right after dispatching a message?
		// return true;
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	POINT pos{};
	RECT client{};
	if (GetCursorPos(&pos) && ScreenToClient(hwnd, &pos) && GetClientRect(hwnd, &client) &&
		(PtInRect(&client, pos) || GetCapture() == hwnd))
	{
		io.MousePos = ImVec2((float)pos.x, (float)pos.y);
		io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	}
	else
	{
		io.MouseDown[0] = false;
		io.MouseDown[1] = false;
		io.MouseDown[2] = false;
	}
	ImGui::NewFrame();
	frame_active = true;
	return true;
}

static void imgui_window_build_ui()
{
	using namespace ImGui;
	Begin("twinject (netdex)");
	Text("b e p l #: %d %d %d %d",
		stat_bullets.load(),
		stat_enemies.load(),
		stat_powerups.load(),
		stat_lasers.load());
	Text("bot state: %s", stat_bot_enabled.load() ? "ENABLED" : "DISABLED");
	Text("viz state: %s", stat_render_enabled.load() ? "DETAILED" : "NONE");

	if (Button("Toggle Bot"))
		request_toggle_bot.store(true);
	SameLine();
	if (Button("Toggle Debug"))
		request_toggle_debug.store(true);
	Checkbox("Show IMGUI demo", &show_demo_window);
	End();

	if (show_demo_window)
		ShowDemoWindow();
}

static bool imgui_window_present()
{
	if (!frame_active || !g_pd3dDevice)
		return false;
	frame_active = false;

	// Rendering
	ImGui::EndFrame();
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*255.0f), (int)(clear_color.y*255.0f), (int)(clear_color.z*255.0f), (int)(clear_color.w*255.0f));
	g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
	if (g_pd3dDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		g_pd3dDevice->EndScene();
	}
	HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		g_pd3dDevice->Reset(&g_d3dpp);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
	return true;
}

static void imgui_window_loop()
{
	if (!imgui_window_create())
	{
		ui_started.store(false);
		return;
	}
	ui_started.store(true);

	while (!ui_stop.load())
	{
		if (imgui_window_begin_frame())
		{
			imgui_window_build_ui();
			imgui_window_present();
		}
		Sleep(16);
	}

	imgui_window_destroy();
}

bool imgui_window_init()
{
	if (ui_started.load())
		return true;

	ui_stop.store(false);
	ui_thread = std::thread(imgui_window_loop);
	return ui_thread.joinable();
}

bool imgui_window_preframe()
{
	return true;
}

bool imgui_window_frame_active()
{
	return false;
}

void imgui_window_update_state(int bullets, int enemies, int powerups, int lasers, bool botEnabled, bool renderEnabled)
{
	stat_bullets.store(bullets);
	stat_enemies.store(enemies);
	stat_powerups.store(powerups);
	stat_lasers.store(lasers);
	stat_bot_enabled.store(botEnabled);
	stat_render_enabled.store(renderEnabled);
}

bool imgui_window_consume_toggle_bot()
{
	return request_toggle_bot.exchange(false);
}

bool imgui_window_consume_toggle_debug()
{
	return request_toggle_debug.exchange(false);
}

bool imgui_window_render()
{
	return true;
}

bool imgui_window_cleanup()
{
	ui_stop.store(true);
	if (hwnd)
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	if (ui_thread.joinable())
		ui_thread.join();
	return true;
}
