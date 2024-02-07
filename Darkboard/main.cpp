#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <WICTextureLoader.h>
#include <dwmapi.h>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>

#include "resource.h"
#include <iostream>

// Define resource IDs from Darkboard.rc
#define IDB_PNG1 101
#define IDB_PNG2 102

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Define a struct for Notes
struct Note
{
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
		ar& noteID;
		ar& title;
		ar& body;
		ar& isPinned;
		ar& isEditing;
		ar& isDeleted;
		ar& newlyCreated;
		ar& x;
		ar& y;
	}

    int noteID;
    char title[32];
    char body[256];
    bool isPinned = false;
    bool isEditing = false;
    bool isDeleted = false;
    bool newlyCreated = false;

    // Coordinates for note position on window
    float x = 0.f;
    float y = 0.f;
};

// Global var for note editing state
/*
bool isEditing = false;
Note* editingNote = nullptr;
*/

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Darkboard", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Darkboard", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Change title bar color to Windows dark mode
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_EnablePowerSavingMode; // Enable Power Saving Mode

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.18f);

    ID3D11ShaderResourceView* pinTexture = nullptr;
    ID3D11ShaderResourceView* closeTexture = nullptr;

    // Load textures from Darkboard.rc 
    HRSRC closeButton = FindResource(nullptr, MAKEINTRESOURCE(IDB_PNG1), L"PNG");
    HRSRC pinButton = FindResource(nullptr, MAKEINTRESOURCE(IDB_PNG2), L"PNG");

    HGLOBAL closeResource = LoadResource(nullptr, closeButton);
    HGLOBAL pinResource = LoadResource(nullptr, pinButton);

    void* closeData = LockResource(closeResource);
    void* pinData = LockResource(pinResource);

    DWORD closeSize = SizeofResource(nullptr, closeButton);
    DWORD pinSize = SizeofResource(nullptr, pinButton);


    // Load your textures using DirectXTK's WICTextureLoader
    HRESULT hr = DirectX::CreateWICTextureFromMemory(g_pd3dDevice, (const uint8_t*)pinData, pinSize, nullptr, &pinTexture);
    if (FAILED(hr)) 
    {
    }

    hr = DirectX::CreateWICTextureFromMemory(g_pd3dDevice, (const uint8_t*)closeData, closeSize, nullptr, &closeTexture);
    if (FAILED(hr)) 
    {
    }

    // Disable .ini file
    ImGui::GetIO().IniFilename = nullptr;

    // Initialize a vector to store notes
    std::vector<Note> notes;

    // Try to deserialize from notes.dat file
    std::ifstream ifs("notes.dat");
    if (ifs.is_open())
    {
        while (true)
        {
			Note note;
            try
            {
				boost::archive::text_iarchive ia(ifs);
				ia >> note;
				notes.push_back(note);
			}
            catch (boost::archive::archive_exception& e)
            {
				break;
			}
		}
    }

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    bool done = false;
    bool editingNote = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        ImGui_ImplWin32_WaitForEvent(hwnd);
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // User interaction text - we show this before the user has created any notes to guide them to double-click to create a note
        // Text is shown in the center of the window, being slightly lighter than the background
        // Have it only show the text - no background, no borders
        if (notes.empty())
        {
            ImVec2 windowCenter = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(windowCenter, ImGuiCond_None, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(250, 100));
			ImGui::Begin("##help", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Double-click to create a note");
			ImGui::End();
		}

        // Detect if the mouse was double-clicked on the background (to create a new note)
        if (ImGui::IsMouseDoubleClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive())
        {
            // Loop through note IDs (short int), check for closest available that is not in the Notes vector
            for (short int i = 0; i < 256; i++)
            {
				bool found = false;
                for (auto& note : notes)
                {
                    if (note.noteID == i)
                    {
						found = true;
						break;
					}
				}

                if (!found && !editingNote)
                {
                    // Create a new note with the ID
                    Note note;
                    note.noteID = i;
                    strcpy_s(note.title, "New note");
                    // Note body - vector size
                    auto vectorSize = notes.size();
                    strcpy_s(note.body, "Body");
                    note.newlyCreated = true;

                    // Initial note coordinates correspond to mouse position
                    note.x = ImGui::GetMousePos().x;
                    note.y = ImGui::GetMousePos().y;

                    notes.push_back(note);
					break;
				}
			}
		}

        for(auto& note : notes)
        {
            if (note.isDeleted)
            {
                continue;
            }

            ImGui::SetNextWindowPos(ImVec2(note.x, note.y), ImGuiCond_Once);

            // Set the window flags based on the pinned state
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar;

            if (note.isPinned)
            {
                windowFlags |= ImGuiWindowFlags_NoMove;
            }

            ImGui::SetNextWindowSize(ImVec2(250, 200));
            ImGui::Begin(std::to_string(note.noteID).c_str(), 0, windowFlags);

            // Create a child window for the custom title bar
            ImGui::BeginChild("TitleBar", ImVec2(ImGui::GetWindowWidth(), 20));

            // Add the title
            if (note.newlyCreated)
            {
                editingNote = true;
                // Set the focus to the title input box
                ImGui::InputText("##title", note.title, IM_ARRAYSIZE(note.title), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

                // Check if we are done editing the title
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
                {
                    note.newlyCreated = false;
                    editingNote = false;
                }
            }
            else
            {
                ImGui::Text("%s", note.title);
            }

            ImGui::SameLine();

            ImGuiStyle& style = ImGui::GetStyle();
            ImVec2 buttonSize(10.f, 10.f);
            float widthNeeded = buttonSize.x + style.ItemSpacing.x + buttonSize.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - widthNeeded*2 - 5);


            // Pin button with a pin status
            if (ImGui::ImageButton((ImTextureID)pinTexture, buttonSize)) 
            {
                note.isPinned = !note.isPinned;
            }

            // Close button that is to the right of the pin button
            ImGui::SameLine();
            if (ImGui::ImageButton((ImTextureID)closeTexture, buttonSize) && !editingNote) 
            {
                note.isDeleted = true;
            }

            ImGui::EndChild();

            if (note.isEditing) 
            {
                // When editing, use InputTextMultiline
                ImVec2 size = ImGui::GetContentRegionAvail(); // Get the size of the available space
                ImGui::InputTextMultiline("##edit", note.body, IM_ARRAYSIZE(note.body), size, ImGuiInputTextFlags_AutoSelectAll);

                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) || 
                (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()))
                {
                    // If the Enter key was pressed, or if the mouse was clicked outside the widget, stop editing
                    std::replace(note.body, note.body + strlen(note.body), '\n', ' ');
                    note.isEditing = false;
                }
            }
            else 
            {
                // When not editing, use TextWrapped
                ImGui::TextWrapped("%s", note.body);
                if (ImGui::IsItemClicked()) 
                {
                    // If the text is clicked, start editing
                    note.isEditing = true;
                }
            }

            // Save the note position
            note.x = ImGui::GetWindowPos().x;
            note.y = ImGui::GetWindowPos().y;

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    std::ofstream ofs("notes.dat");

    for (auto& note : notes)
    {

        if (!note.isDeleted)
        {
            // Serialize the vector and write to file
            boost::archive::text_oarchive oa(ofs);
			oa << note;
		}
	}

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
