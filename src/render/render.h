#pragma once
#include <memory>
#include <vector>
#include <string>

#include <d3d11.h>

#include "../../ext/imgui/vendors/Dear ImGui/imgui.h"
#include "../../ext/imgui/vendors/Dear ImGui/backends/imgui_impl_dx11.h"
#include "../../ext/imgui/vendors/Dear ImGui/backends/imgui_impl_win32.h"

struct detail_t {
	HWND window = nullptr;
	WNDCLASSEX window_class = {};
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* device_context = nullptr;
	ID3D11RenderTargetView* render_target_view = nullptr;
	IDXGISwapChain* swap_chain = nullptr;
};

// Menu pages enum
enum ImPages_ : int
{
	ImPage_Aimbot,
	ImPage_Visuals,
	ImPage_Filters,
	ImPage_Movement,
	ImPage_Settings,
	ImPages_COUNT
};

// Bone/hitpart selection enum (matching demo exactly)
enum eBones : int
{
	Bone_Head,
	Bone_LeftArm,
	Bone_RightArm,
	Bone_Belly,
	Bone_LeftLeg,
	Bone_RightLeg,
	Bones_COUNT
};

class render_t {
public:
	render_t();
	~render_t();

	bool running = false;

	void start_render();
	void render_menu();
	void render_visuals();
	void end_render();

	bool create_device();
	bool create_window();
	bool create_imgui();

	std::unique_ptr<detail_t> detail = std::make_unique<detail_t>();

private:
	void destroy_device();
	void destroy_window();
	void destroy_imgui();

	// Menu system variables
	int m_iCurrentPage = 0;
	std::vector<const char*> m_Tabs;
	bool m_bSelectedBones[Bones_COUNT] = { false }; // Bone selection state

	// Helper functions for custom styling
	void apply_custom_style();
	void render_menu_background(ImVec2 pos, ImVec2 size, float header_height, float sidebar_width);
	void render_sidebar();
	void render_page_content(float group_width);

	// Custom widget replacements
	bool radio_button_gradient(const char* label, int* v, int v_button, ImVec2 size_arg);
	void begin_child_styled(const char* str_id, ImVec2 size);
	void end_child_styled();

	// Keybind picker widget
	bool keybind_button(const char* label, int* key, int* mode, float width);

	// Checkbox with hover effect
	bool checkbox_hover(const char* label, bool* v);
};

inline std::unique_ptr<render_t> render = std::make_unique<render_t>();