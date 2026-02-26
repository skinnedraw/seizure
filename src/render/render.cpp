#define IMGUI_DEFINE_MATH_OPERATORS
#include <render/render.h>

#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <string>
#include <mutex>

#include <settings.h>
#include <features/esp/esp.h>
#include <keybind/keybind.h>
#include <Windows.h>
#include <memory/memory.h>
#include <sdk/offsets.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>

// Include ImGui addons for custom widgets
#include "../../ext/imgui/vendors/Dear ImGui/addons/imgui_addons.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
    {
        return true;
    }

    switch (msg)
    {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

render_t::render_t()
{
    detail = std::make_unique<detail_t>();

    // Initialize tab names
    m_Tabs.push_back("Aimbot");
    m_Tabs.push_back("Silent Aim");
    m_Tabs.push_back("Visuals");
    m_Tabs.push_back("Filters");
    m_Tabs.push_back("Movement");
    m_Tabs.push_back("Desync");
    m_Tabs.push_back("Settings");
}

render_t::~render_t()
{
    destroy_imgui();
    destroy_window();
    destroy_device();
}

bool render_t::create_window()
{
    detail->window_class.cbSize = sizeof(detail->window_class);
    detail->window_class.style = CS_CLASSDC;
    detail->window_class.lpszClassName = "T4";
    detail->window_class.hInstance = GetModuleHandleA(0);
    detail->window_class.lpfnWndProc = wnd_proc;

    RegisterClassExA(&detail->window_class);

    detail->window = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        detail->window_class.lpszClassName,
        "T4",
        WS_POPUP,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        0,
        0,
        detail->window_class.hInstance,
        0
    );

    if (!detail->window)
    {
        return false;
    }

    SetLayeredWindowAttributes(detail->window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

    RECT client_area{};
    RECT window_area{};

    GetClientRect(detail->window, &client_area);
    GetWindowRect(detail->window, &window_area);

    POINT diff{};
    ClientToScreen(detail->window, &diff);

    MARGINS margins
    {
        window_area.left + (diff.x - window_area.left),
        window_area.top + (diff.y - window_area.top),
        window_area.right,
        window_area.bottom,
    };

    DwmExtendFrameIntoClientArea(detail->window, &margins);

    ShowWindow(detail->window, SW_SHOW);
    UpdateWindow(detail->window);

    return true;
}

bool render_t::create_device()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};

    swap_chain_desc.BufferCount = 1;

    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    swap_chain_desc.OutputWindow = detail->window;

    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Windowed = 1;

    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    swap_chain_desc.SampleDesc.Count = 2;
    swap_chain_desc.SampleDesc.Quality = 0;

    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_FEATURE_LEVEL feature_level;
    D3D_FEATURE_LEVEL feature_level_list[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_level_list,
        2,
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &detail->swap_chain,
        &detail->device,
        &feature_level,
        &detail->device_context
    );

    if (result == DXGI_ERROR_UNSUPPORTED)
    {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            feature_level_list,
            2,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &detail->swap_chain,
            &detail->device,
            &feature_level,
            &detail->device_context
        );
    }

    if (result != S_OK)
    {
        MessageBoxA(nullptr, "This software can not run on your computer.", "Critical Problem", MB_ICONERROR | MB_OK);
    }

    ID3D11Texture2D* back_buffer{ nullptr };
    detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));

    if (back_buffer)
    {
        detail->device->CreateRenderTargetView(back_buffer, nullptr, &detail->render_target_view);
        back_buffer->Release();

        return true;
    }

    return false;
}

void render_t::apply_custom_style()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Custom Styles from CGui
    style.WindowRounding = 3;
    style.ChildRounding = 2;
    style.FrameRounding = 0;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 0;

    style.ButtonTextAlign = { 0.5f, 0.5f };
    style.WindowTitleAlign = { 0.5f, 0.5f };
    style.FramePadding = { 6.0f, 6.0f };
    style.WindowPadding = { 10.0f, 10.0f };
    style.ItemSpacing = { 10.0f, 6.0f };
    style.ItemInnerSpacing = { style.WindowPadding.x, 2 };

    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;

    style.ScrollbarSize = 7.f;
    style.GrabMinSize = 1.f;
    style.DisabledAlpha = 0.5f;

    style.WindowMinSize = ImVec2(400, 450);

    // Custom Colors from CGui
    style.Colors[ImGuiCol_WindowBg] = ImAdd::HexToColorVec4(0x0c0c0c, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImAdd::HexToColorVec4(0x121212, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImAdd::HexToColorVec4(0x0d0d0d, 1.00f);

    style.Colors[ImGuiCol_Text] = ImAdd::HexToColorVec4(0xb4b4b4, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImAdd::HexToColorVec4(0x8c8c8c, 1.00f);

    style.Colors[ImGuiCol_SliderGrab] = ImAdd::HexToColorVec4(0x5e7aa6, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImAdd::HexToColorVec4(0x3e5780, 1.00f);

    style.Colors[ImGuiCol_SeparatorHovered] = style.Colors[ImGuiCol_SliderGrab];
    style.Colors[ImGuiCol_SeparatorActive] = style.Colors[ImGuiCol_SliderGrabActive];

    style.Colors[ImGuiCol_BorderShadow] = ImAdd::HexToColorVec4(0x000000, 1.00f);
    style.Colors[ImGuiCol_Border] = ImAdd::HexToColorVec4(0x333333, 1.00f);
    style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];

    style.Colors[ImGuiCol_Button] = ImAdd::HexToColorVec4(0x2a2a2a, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImAdd::HexToColorVec4(0x3f3f3f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImAdd::HexToColorVec4(0x191919, 1.00f);

    style.Colors[ImGuiCol_FrameBg] = style.Colors[ImGuiCol_Button];
    style.Colors[ImGuiCol_FrameBgHovered] = style.Colors[ImGuiCol_ButtonHovered];
    style.Colors[ImGuiCol_FrameBgActive] = style.Colors[ImGuiCol_ButtonActive];

    style.Colors[ImGuiCol_Header] = ImAdd::HexToColorVec4(0x2a2a2a, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImAdd::HexToColorVec4(0x3f3f3f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImAdd::HexToColorVec4(0x191919, 1.00f);

    style.Colors[ImGuiCol_ScrollbarBg] = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_ScrollbarGrab] = ImAdd::HexToColorVec4(0x414141, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = style.Colors[ImGuiCol_ScrollbarGrab];
    style.Colors[ImGuiCol_ScrollbarGrabActive] = style.Colors[ImGuiCol_ScrollbarGrab];

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4();
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4();
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4();
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4();
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4();
}

bool render_t::create_imgui()
{
    using namespace ImGui;
    CreateContext();

    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    GImGui->NavDisableHighlight = true; // Disable Highlighting

    // Apply custom style FIRST before scaling
    apply_custom_style();

    // Scale everything
    style.ScaleAllSizes(main_scale);

    // Setup Font - Use the same font config as the demo
    ImFontConfig cfg;
    cfg.GlyphOffset = ImVec2(0, 1);

    // Use default font for now (you can add custom fonts later)
    io.Fonts->AddFontDefault(&cfg);

    // Add Tahoma as the ESP font
    char windows_path[MAX_PATH];
    GetWindowsDirectoryA(windows_path, MAX_PATH);
    std::string tahoma_path = std::string(windows_path) + "\\Fonts\\tahoma.ttf";
    Visualize.font = io.Fonts->AddFontFromFileTTF(tahoma_path.c_str(), 13.0f * main_scale);
    if (!Visualize.font)
    {
        Visualize.font = io.Fonts->AddFontDefault();
    }

    if (!ImGui_ImplWin32_Init(detail->window))
    {
        return false;
    }

    if (!detail->device || !detail->device_context)
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(detail->device, detail->device_context))
    {
        return false;
    }

    return true;
}

void render_t::destroy_device()
{
    if (detail->render_target_view) detail->render_target_view->Release();
    if (detail->swap_chain) detail->swap_chain->Release();
    if (detail->device_context) detail->device_context->Release();
    if (detail->device) detail->device->Release();
}

void render_t::destroy_window()
{
    DestroyWindow(detail->window);
    UnregisterClassA(detail->window_class.lpszClassName, detail->window_class.hInstance);
}

void render_t::destroy_imgui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void render_t::start_render()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (GetAsyncKeyState(settings::settings::menu_key) & 1)
    {
        running = !running;

        if (running)
        {
            SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
        }
        else
        {
            SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
    }
}

void render_t::end_render()
{
    ImGui::Render();

    float clear_color[4]{ 0, 0, 0, 0 };
    detail->device_context->OMSetRenderTargets(1, &detail->render_target_view, nullptr);
    detail->device_context->ClearRenderTargetView(detail->render_target_view, clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    detail->swap_chain->Present(0, 0);
}

bool render_t::radio_button_gradient(const char* label, int* v, int v_button, ImVec2 size_arg)
{
    return ImAdd::RadioFrameGradient(label, v, v_button, size_arg);
}

void render_t::begin_child_styled(const char* str_id, ImVec2 size)
{
    ImAdd::BeginChild(str_id, size);
}

void render_t::end_child_styled()
{
    ImAdd::EndChild();
}

// Checkbox wrapper with hover effect
bool render_t::checkbox_hover(const char* label, bool* v)
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Store cursor position for hover detection
    ImVec2 cursor_start = ImGui::GetCursorScreenPos();

    // Calculate the full size of checkbox + label
    float checkbox_size = ImGui::GetFrameHeight();
    ImVec2 label_size = ImGui::CalcTextSize(label);
    ImVec2 total_size = ImVec2(checkbox_size + style.ItemInnerSpacing.x + label_size.x, checkbox_size);

    // Check if mouse is hovering over the checkbox area
    ImVec2 mouse_pos = ImGui::GetMousePos();
    bool is_hovered = (mouse_pos.x >= cursor_start.x && mouse_pos.x <= cursor_start.x + total_size.x &&
        mouse_pos.y >= cursor_start.y && mouse_pos.y <= cursor_start.y + total_size.y);

    // Push colors based on hover state
    if (is_hovered)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    }

    // Draw the checkbox
    bool result = ImAdd::CheckBox(label, v);

    ImGui::PopStyleColor(4);

    return result;
}

// Custom keybind picker that works with keybind class and settings
bool render_t::keybind_button(const char* label, int* key, int* mode, float width)
{
    ImGuiStyle& style = ImGui::GetStyle();
    bool value_changed = false;

    // Generate unique IDs
    ImGui::PushID(label);

    // Track if we're waiting for key input
    static bool waiting_for_key = false;
    static const char* active_label = nullptr;

    // Display text based on current key and state
    std::string key_text = "[None]";
    if (waiting_for_key && active_label == label)
    {
        key_text = "[...]";
    }
    else if (*key > 0 && *key < 256)
    {
        key_text = "[";
        key_text += key_namess[*key];
        key_text += "]";
    }

    // Calculate text size and position it on the right with padding
    ImVec2 text_size = ImGui::CalcTextSize(key_text.c_str());
    float window_width = ImGui::GetWindowWidth();

    // Position cursor to align text to the right with padding
    ImGui::SetCursorPosX(window_width - text_size.x - style.WindowPadding.x);

    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

    // Make the text clickable
    ImGui::InvisibleButton("##keybind_button", text_size);
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    bool hovered = ImGui::IsItemHovered();

    // Draw the text with color based on hover state
    ImU32 text_color = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImGui::GetWindowDrawList()->AddText(cursor_pos, text_color, key_text.c_str());

    // Handle left click - key binding
    if (clicked)
    {
        waiting_for_key = true;
        active_label = label;
    }

    // If we're waiting for this keybind
    if (waiting_for_key && active_label == label)
    {
        // Check for any key press
        for (int i = 0; i < 256; i++)
        {
            // Skip mouse buttons for cleaner UX
            if (i == VK_LBUTTON || i == VK_RBUTTON)
                continue;

            if (GetAsyncKeyState(i) & 0x8000)
            {
                // ESC or DELETE clears the bind
                if (i == VK_ESCAPE || i == VK_DELETE)
                {
                    *key = 0;
                }
                else
                {
                    *key = i;
                }

                waiting_for_key = false;
                active_label = nullptr;
                value_changed = true;
                break;
            }
        }
    }

    // Handle right click - mode selection popup
    if (right_clicked)
    {
        ImGui::OpenPopup("keybind_mode_popup");
    }

    // Popup for mode selection
    if (ImGui::BeginPopup("keybind_mode_popup"))
    {
        // Note: The keybind enum names are backwards - TOGGLE=0 acts as hold, HOLD=1 acts as toggle
        if (ImGui::Selectable("Hold", *mode == keybind::TOGGLE))
        {
            *mode = keybind::TOGGLE; // This is 0, but acts as hold
            value_changed = true;
        }

        if (ImGui::Selectable("Toggle", *mode == keybind::HOLD))
        {
            *mode = keybind::HOLD; // This is 1, but acts as toggle
            value_changed = true;
        }

        if (ImGui::Selectable("Always", *mode == keybind::ALWAYS))
        {
            *mode = keybind::ALWAYS;
            value_changed = true;
        }

        ImGui::Separator();

        if (ImGui::Selectable("Clear"))
        {
            *key = 0;
            value_changed = true;
        }

        ImGui::EndPopup();
    }

    // Show mode indicator next to button
    const char* mode_text = "";
    switch (*mode)
    {
    case keybind::TOGGLE: mode_text = "[hold]"; break; // enum is backwards
    case keybind::HOLD: mode_text = "[toggle]"; break; // enum is backwards
    case keybind::ALWAYS: mode_text = "[always]"; break;
    }

    if (*mode != keybind::TOGGLE) // Only show if not default (hold)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", mode_text);
    }

    ImGui::PopID();
    return value_changed;
}

void render_t::render_menu_background(ImVec2 pos, ImVec2 size, float header_height, float sidebar_width)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    float fGlowAlpha = 0.14f;

    // Draw header
    pDrawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + header_height),
        ImGui::GetColorU32(ImGuiCol_ChildBg), style.WindowRounding, ImDrawFlags_RoundCornersTop);

    // Draw main content area
    pDrawList->AddRectFilled(ImVec2(pos.x, pos.y + header_height), ImVec2(pos.x + size.x, pos.y + size.y),
        ImGui::GetColorU32(ImGuiCol_WindowBg), style.WindowRounding, ImDrawFlags_RoundCornersBottomRight);

    // Draw sidebar
    pDrawList->AddRectFilled(ImVec2(pos.x, pos.y + header_height), ImVec2(pos.x + sidebar_width, pos.y + size.y),
        ImGui::GetColorU32(ImGuiCol_ChildBg), style.WindowRounding, ImDrawFlags_RoundCornersBottomLeft);

    // Draw header gradient
    pDrawList->AddRectFilledMultiColor(
        ImVec2(pos.x + style.WindowBorderSize, pos.y + style.WindowBorderSize + style.WindowRounding),
        ImVec2(pos.x + size.x - style.WindowBorderSize, pos.y + header_height),
        ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.0f),
        ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.0f),
        ImGui::GetColorU32(ImGuiCol_SliderGrab, fGlowAlpha),
        ImGui::GetColorU32(ImGuiCol_SliderGrab, fGlowAlpha)
    );

    // Draw borders
    if (style.WindowBorderSize > 0)
    {
        pDrawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding);
        pDrawList->AddLine(ImVec2(pos.x + style.WindowBorderSize, pos.y + header_height),
            ImVec2(pos.x + size.x - style.WindowBorderSize, pos.y + header_height),
            ImGui::GetColorU32(ImGuiCol_SliderGrab));
        pDrawList->AddLine(ImVec2(pos.x + sidebar_width, pos.y + header_height + style.WindowBorderSize),
            ImVec2(pos.x + sidebar_width, pos.y + size.y - style.WindowBorderSize),
            ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
    }

    // Draw title
    pDrawList->AddText(ImVec2(pos.x + style.WindowPadding.x, pos.y + style.WindowPadding.y),
        ImGui::GetColorU32(ImGuiCol_Text), "seizure");
    ImVec2 title_size = ImGui::CalcTextSize("seizure");
    pDrawList->AddText(ImVec2(pos.x + style.WindowPadding.x + title_size.x + 1, pos.y + style.WindowPadding.y),
        ImGui::GetColorU32(ImGuiCol_SliderGrab), " nightly");
}

void render_t::render_sidebar()
{
    ImGuiStyle& style = ImGui::GetStyle();

    for (int i = 0; i < m_Tabs.size(); i++)
    {
        radio_button_gradient(m_Tabs[i], &m_iCurrentPage, i, ImVec2(-0.1f, 0));
    }
}

void render_t::render_page_content(float group_width)
{
    ImGuiStyle& style = ImGui::GetStyle();
    float HeaderHeight = ImGui::GetFontSize() + style.WindowPadding.y * 2; // Needed for bone selector

    if (m_iCurrentPage == ImPage_Aimbot)
    {
        ImGui::BeginGroup(); // Left column
        {
            begin_child_styled("General", ImVec2(group_width, ImGui::GetFontSize() * 4.5f + style.ItemSpacing.y * 3 + style.WindowPadding.y * 2));
            {
                checkbox_hover("Enable", &settings::aimbot::enabled);
                ImGui::SameLine();
                keybind_button("AimbotKey", &settings::aimbot::keybind, &settings::aimbot::keybind_mode, 0);
                checkbox_hover("Sticky Aim", &settings::aimbot::sticky_aim);
            }
            end_child_styled();

            begin_child_styled("Configs", ImVec2(group_width, ImGui::GetFontSize() * 8.5f + style.ItemSpacing.y * 3 + style.ItemInnerSpacing.y * 4 + style.WindowPadding.y * 2));
            {
                const char* aimbot_types[] = { "Camera", "Mouse" };

                ImGui::PushItemWidth(ImGui::GetWindowWidth() - style.WindowPadding.x * 2);
                {
                    ImAdd::Combo("Type", &settings::aimbot::aim_type, aimbot_types, IM_ARRAYSIZE(aimbot_types));
                    ImAdd::SliderFloat("Field of View", &settings::aimbot::fov, 1.0f, 1000.0f, "%.1f");
                    ImAdd::SliderFloat("Smoothness", &settings::aimbot::smoothing, 0.1f, 1.0f, "%.2f");
                }
                ImGui::PopItemWidth();
            }
            end_child_styled();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup(); // Right column
        {
            begin_child_styled("Overlay", ImVec2(group_width, ImGui::GetFontSize() * 1.5f + style.ItemSpacing.y + style.WindowPadding.y * 2));
            {
                checkbox_hover("FOV Circle", &settings::aimbot::show_fov);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##FOV Circle Color", settings::aimbot::fov_color);
            }
            end_child_styled();

            // Bone/Hitpart Selector - Fixed scaling
            begin_child_styled("Target Bones", ImVec2(0, 0));
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                ImGui::BeginChild("InternalFrame", ImVec2(), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                {
                    // Get available space
                    float available_width = ImGui::GetWindowWidth();
                    float available_height = ImGui::GetWindowHeight();

                    // Calculate ideal sizes based on width
                    float ideal_ArmLeg_Width = available_width / 6.0f;
                    float ideal_ArmLeg_Height = ideal_ArmLeg_Width * 2.5f;
                    float ideal_Head_Size = (ideal_ArmLeg_Width / 3.0f) * 4.0f;

                    // Calculate total height needed for the layout
                    float total_height_needed = HeaderHeight + ideal_Head_Size + ideal_ArmLeg_Height * 2;

                    // Add padding to both top and bottom
                    float vertical_padding = style.WindowPadding.y * 2;

                    // Scale down if needed to fit height with padding
                    if (total_height_needed > available_height - vertical_padding)
                    {
                        float scale_factor = (available_height - vertical_padding) / total_height_needed;
                        ideal_ArmLeg_Width *= scale_factor;
                        ideal_ArmLeg_Height *= scale_factor;
                        ideal_Head_Size *= scale_factor;
                    }

                    // Final sizes (cast to int for pixel alignment)
                    float ArmLeg_Width = (float)(int)ideal_ArmLeg_Width;
                    float ArmLeg_Height = (float)(int)ideal_ArmLeg_Height;
                    float Head_Size = (float)(int)ideal_Head_Size;

                    // Calculate actual total height (HeaderHeight is for spacing above head)
                    float actual_total_height = HeaderHeight + Head_Size + ArmLeg_Height * 2;

                    // Center vertically with padding
                    ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImVec2(0, (available_height - actual_total_height) / 2));
                    ImGui::BeginGroup();
                    {
                        // Add spacing above the head (this is what HeaderHeight is for)
                        ImGui::Dummy(ImVec2(0, HeaderHeight));

                        ImGui::SetCursorPosX((float)(int)(ImGui::GetWindowWidth() / 2 - Head_Size / 2));
                        if (ImAdd::SelectableFrame(":)Head", m_bSelectedBones[Bone_Head], ImVec2(Head_Size, Head_Size)))
                        {
                            m_bSelectedBones[Bone_Head] = !m_bSelectedBones[Bone_Head];
                        }

                        ImGui::SetCursorPosX((float)(int)(ImGui::GetWindowWidth() / 2 - ArmLeg_Width * 2));
                        ImGui::BeginGroup();
                        {
                            if (ImAdd::SelectableFrame("LeftArm", m_bSelectedBones[Bone_LeftArm], ImVec2(ArmLeg_Width, ArmLeg_Height)))
                            {
                                m_bSelectedBones[Bone_LeftArm] = !m_bSelectedBones[Bone_LeftArm];
                            }
                            ImGui::SameLine();
                            if (ImAdd::SelectableFrame("Belly", m_bSelectedBones[Bone_Belly], ImVec2(ArmLeg_Width * 2, ArmLeg_Height)))
                            {
                                m_bSelectedBones[Bone_Belly] = !m_bSelectedBones[Bone_Belly];
                            }
                            ImGui::SameLine();
                            if (ImAdd::SelectableFrame("RightArm", m_bSelectedBones[Bone_RightArm], ImVec2(ArmLeg_Width, ArmLeg_Height)))
                            {
                                m_bSelectedBones[Bone_RightArm] = !m_bSelectedBones[Bone_RightArm];
                            }
                        }
                        ImGui::EndGroup();

                        ImGui::SetCursorPosX((float)(int)(ImGui::GetWindowWidth() / 2 - ArmLeg_Width));
                        ImGui::BeginGroup();
                        {
                            if (ImAdd::SelectableFrame("LeftLeg", m_bSelectedBones[Bone_LeftLeg], ImVec2(ArmLeg_Width, ArmLeg_Height)))
                            {
                                m_bSelectedBones[Bone_LeftLeg] = !m_bSelectedBones[Bone_LeftLeg];
                            }
                            ImGui::SameLine();
                            if (ImAdd::SelectableFrame("RightLeg", m_bSelectedBones[Bone_RightLeg], ImVec2(ArmLeg_Width, ArmLeg_Height)))
                            {
                                m_bSelectedBones[Bone_RightLeg] = !m_bSelectedBones[Bone_RightLeg];
                            }
                        }
                        ImGui::EndGroup();
                    }
                    ImGui::EndGroup();
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_SilentAim)
    {
        ImGui::BeginGroup(); // Left column
        {
            begin_child_styled("General", ImVec2(group_width, ImGui::GetFontSize() * 4.5f + style.ItemSpacing.y * 3 + style.WindowPadding.y * 2));
            {
                checkbox_hover("Enable", &settings::silentaim::enabled);
                ImGui::SameLine();
                keybind_button("SilentAimKey", &settings::silentaim::keybind, &settings::silentaim::keybind_mode, 0);

                checkbox_hover("Sticky Aim", &settings::silentaim::sticky_aim);
            }
            end_child_styled();

            begin_child_styled("Configs", ImVec2(group_width, ImGui::GetFontSize() * 6.0f + style.ItemSpacing.y * 2 + style.ItemInnerSpacing.y * 3 + style.WindowPadding.y * 2));
            {
                ImGui::PushItemWidth(ImGui::GetWindowWidth() - style.WindowPadding.x * 2);
                {
                    ImAdd::SliderFloat("Field of View", &settings::silentaim::fov, 10.0f, 500.0f, "%.1f");

                    checkbox_hover("Use Prediction", &settings::silentaim::use_prediction);

                    if (settings::silentaim::use_prediction)
                    {
                        ImAdd::SliderFloat("Prediction Time", &settings::silentaim::prediction_time, 0.01f, 0.2f, "%.3f");
                    }
                }
                ImGui::PopItemWidth();
            }
            end_child_styled();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup(); // Right column
        {
            begin_child_styled("Overlay", ImVec2(group_width, ImGui::GetFontSize() * 3.0f + style.ItemSpacing.y * 2 + style.WindowPadding.y * 2));
            {
                checkbox_hover("FOV Circle", &settings::silentaim::show_fov);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##Silent FOV Circle Color", settings::silentaim::fov_color);

                checkbox_hover("Show Tracer", &settings::silentaim::show_tracer);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##Tracer Color", settings::silentaim::tracer_color);
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_Visuals)
    {
        ImGui::BeginGroup();
        {
            begin_child_styled("ESP", ImVec2(group_width, 0));
            {
                checkbox_hover("Draw Box", &settings::visuals::box);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##box_color", settings::visuals::box_color);

                checkbox_hover("Draw Name", &settings::visuals::name);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##name_color", settings::visuals::name_color);

                checkbox_hover("Distance", &settings::visuals::distance);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##distance_color", settings::visuals::distance_color);

                checkbox_hover("Tool", &settings::visuals::tool);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
                ImAdd::ColorEdit4("##tool_color", settings::visuals::tool_color);

                checkbox_hover("Local Player", &settings::visuals::localplayer);
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_Filters)
    {
        ImGui::BeginGroup();
        {
            begin_child_styled("Filters", ImVec2(group_width, 0));
            {
                ImGui::TextDisabled("Filter options will go here");
                // Add your filter settings here
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_Movement)
    {
        ImGui::BeginGroup();
        {
            begin_child_styled("Movement", ImVec2(group_width, ImGui::GetFontSize() * 4.5f + style.ItemSpacing.y * 3 + style.WindowPadding.y * 2));
            {
                checkbox_hover("Walkspeed", &settings::walkspeed::enabled);
                ImGui::SameLine();
                keybind_button("WalkspeedKey", &settings::walkspeed::keybind, &settings::walkspeed::keybind_mode, 0);

                ImGui::Spacing();

                ImGui::PushItemWidth(ImGui::GetWindowWidth() - style.WindowPadding.x * 2);
                ImAdd::SliderFloat("Speed", &settings::walkspeed::speed, 1.0f, 1000.0f, "%.1f");
                ImGui::PopItemWidth();

                checkbox_hover("Noclip", &settings::noclip::enabled);
                ImGui::SameLine();
                keybind_button("NoclipKey", &settings::noclip::keybind, &settings::noclip::keybind_mode, 0);
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_Desync)
    {
        ImGui::BeginGroup();
        {
            begin_child_styled("FFlags", ImVec2(group_width, ImGui::GetFontSize() * 3 + style.ItemSpacing.y + style.WindowPadding.y * 2));
            {
                checkbox_hover("Freeze pos", &settings::freezepos::enabled);
                ImGui::SameLine();
                keybind_button("freezepos", &settings::freezepos::keybind, &settings::freezepos::keybind_mode, 0);

                ImGui::Spacing();

                checkbox_hover("Void Hide", &settings::voidhide::enabled);
                ImGui::SameLine();
                keybind_button("VoidHideKey", &settings::voidhide::keybind, &settings::voidhide::keybind_mode, 0);
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
    else if (m_iCurrentPage == ImPage_Settings)
    {
        ImGui::BeginGroup();
        {
            begin_child_styled("Application", ImVec2(group_width, ImGui::GetFontSize() * 6.0f + style.ItemSpacing.y * 3 + style.WindowPadding.y * 2));
            {
                // Menu keybind selector
                ImGui::Text("Menu Key");
                ImGui::SameLine();
                static int dummy_mode = 0; // Menu key doesn't need modes, just the key
                keybind_button("MenuKey", &settings::settings::menu_key, &dummy_mode, 0);

                if (checkbox_hover("Hide Console", &settings::settings::hide_console))
                {
                    HWND console_window = GetConsoleWindow();
                    if (console_window)
                    {
                        ShowWindow(console_window, settings::settings::hide_console ? SW_HIDE : SW_SHOW);
                    }
                }

                ImGui::Spacing();

                float button_width = ImGui::GetWindowWidth() - style.WindowPadding.x * 2;
                float button_height = ImGui::GetFontSize() * 2;

                if (ImAdd::Button("Rescan", ImVec2(button_width, button_height)))
                {
                    std::uint64_t fake_datamodel = memory->read<std::uint64_t>(memory->get_module_address() + OFF(FakeDataModel, Pointer));
                    game::datamodel = rbx::instance_t(memory->read<std::uint64_t>(fake_datamodel + OFF(FakeDataModel, RealDataModel)));
                    game::visengine = { memory->read<std::uint64_t>(memory->get_module_address() + OFF(VisualEngine, Pointer)) };
                    game::workspace = { game::datamodel.find_first_child_by_class("Workspace") };
                    game::players = { game::datamodel.find_first_child_by_class("Players") };
                    game::local_player = { memory->read<std::uint64_t>(game::players.address + OFF(Player, LocalPlayer)) };

                    {
                        std::lock_guard<std::mutex> lock(cache::mtx);
                        cache::cached_players.clear();
                        cache::cached_local_player = {};
                    }
                }

                ImGui::Spacing();

                if (ImAdd::Button("Unload", ImVec2(button_width, button_height)))
                {
                    settings::settings::should_unload = true;
                }
            }
            end_child_styled();
        }
        ImGui::EndGroup();
    }
}

void render_t::render_menu()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImVec2 MenuSize = ImVec2(625, 450);
    float HeaderHeight = ImGui::GetFontSize() + style.WindowPadding.y * 2;
    float SideBarWidth = 120;

    ImGui::SetNextWindowPos(io.DisplaySize / 2 - MenuSize / 2, ImGuiCond_Once);
    ImGui::SetNextWindowSize(MenuSize, ImGuiCond_Once);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("seizure nightly", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);

    // Render custom background
    {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        render_menu_background(pos, size, HeaderHeight, SideBarWidth);
    }

    // Render sidebar
    {
        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImVec2(0, HeaderHeight));
        ImGui::BeginChild("SideBar", ImVec2(SideBarWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
        render_sidebar();
        ImGui::EndChild();
    }

    // Render main content
    {
        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImVec2(SideBarWidth + style.WindowBorderSize, HeaderHeight));
        ImGui::BeginChild("Main", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
        {
            float fGroupWidth = (ImGui::GetWindowWidth() - style.ItemSpacing.x - style.WindowPadding.x * 2) / 2;
            render_page_content(fGroupWidth);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

void render_t::render_visuals()
{
    esp::run();

    if (settings::aimbot::show_fov)
    {
        HWND rblxWnd = FindWindowA(nullptr, "Roblox");
        if (rblxWnd)
        {
            ImVec2 center{};

            // Dynamic FOV center based on aimbot type
            if (settings::aimbot::aim_type == 0)  // Camera mode - center of screen
            {
                RECT windowRect;
                GetClientRect(rblxWnd, &windowRect);
                center = ImVec2(
                    static_cast<float>((windowRect.right - windowRect.left) / 2),
                    static_cast<float>((windowRect.bottom - windowRect.top) / 2)
                );
            }
            else if (settings::aimbot::aim_type == 1)  // Mouse mode - cursor position
            {
                POINT cursor_point;
                GetCursorPos(&cursor_point);
                ScreenToClient(rblxWnd, &cursor_point);
                center = ImVec2(static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y));
            }

            float radius = settings::aimbot::fov;

            ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(
                settings::aimbot::fov_color[0],
                settings::aimbot::fov_color[1],
                settings::aimbot::fov_color[2],
                settings::aimbot::fov_color[3]
            ));

            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            draw->AddCircle(center, radius, color, 0, 2.0f);
        }
    }

    if (settings::silentaim::show_fov)
    {
        HWND rblxWnd = FindWindowA(nullptr, "Roblox");
        if (rblxWnd)
        {
            // Silent aim FOV is always centered on mouse cursor
            POINT cursor_point;
            GetCursorPos(&cursor_point);
            ScreenToClient(rblxWnd, &cursor_point);
            ImVec2 center = ImVec2(static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y));

            float radius = settings::silentaim::fov;

            ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(
                settings::silentaim::fov_color[0],
                settings::silentaim::fov_color[1],
                settings::silentaim::fov_color[2],
                settings::silentaim::fov_color[3]
            ));

            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            draw->AddCircle(center, radius, color, 0, 2.0f);
        }
    }

    // Silent Aim Tracer
    if (settings::silentaim::show_tracer && rbx::silentaim::has_target)
    {
        HWND rblxWnd = FindWindowA(nullptr, "Roblox");
        if (rblxWnd)
        {
            // Get mouse position
            POINT cursor_point;
            GetCursorPos(&cursor_point);
            ScreenToClient(rblxWnd, &cursor_point);
            ImVec2 cursor_pos = ImVec2(static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y));

            // Get target screen position
            math::vector2 dims = game::visengine.get_dimensions();
            math::matrix4 view = game::visengine.get_viewmatrix();
            math::vector2 target_screen;

            if (game::visengine.world_to_screen(rbx::silentaim::current_target_position, target_screen, dims, view))
            {
                ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    settings::silentaim::tracer_color[0],
                    settings::silentaim::tracer_color[1],
                    settings::silentaim::tracer_color[2],
                    settings::silentaim::tracer_color[3]
                ));

                ImDrawList* draw = ImGui::GetBackgroundDrawList();
                draw->AddLine(cursor_pos, ImVec2(target_screen.x, target_screen.y), color, 2.0f);
            }
        }
    }
}