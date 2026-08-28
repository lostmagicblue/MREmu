#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <vmio.h>

#ifdef ANDROID
#include "jni/Vibration.h"
#endif

#define MREMU_KEY_SEND (VM_KEY_QWERTY_MENU + 1)
#define MREMU_KEY_POWER (MREMU_KEY_SEND + 1)
#define MREMU_KEY_NONE (MREMU_KEY_POWER + 1)

#define MREMU_NEGATIVE_KEY_COUNT VM_KEY_BACK
#define MREMU_FULL_KEY_COUNT (MREMU_KEY_POWER + 1 + MREMU_NEGATIVE_KEY_COUNT)

// 共享几何常量：MREmu.cpp（窗口大小/清屏色）和 Keyboard.cpp（绘制/命中）用同一份，避免漂移。
// 布局自上而下：屏幕（满窗宽）→ 软键栏 → 6 行 × 3 列网格键盘，窗口即内容，无多余背景。
constexpr float SOFTBAR_H     = 26.f;  // 软键栏（菜单/返回）高度
constexpr int   GRID_NAV_ROWS = 2;     // 键盘导航行数（← ↑ → / C OK ↓）
constexpr int   GRID_NUM_ROWS = 4;     // 键盘数字行数（123/456/789/*0#）
constexpr float GRID_KH       = 38.f;  // 单行高度

class KeyboardControl {
public:
	enum key_source {
		Unknow,
		Keyboard,
		Mouse,
		ImGui,
        Touch0,
        Touch1,
        Touch2,
        Touch3,
        Touch4,
        Touch5,
        Touch6,
        Touch7,
        Touch8,
        Touch9
	};

	struct pkey_t {
		int key_code;
		sf::Clock tim;
		int8_t key_status = VM_KEY_EVENT_DOWN;
		key_source source = Unknow;

		pkey_t() = default;
		pkey_t(int key_code, key_source source);
	};

	std::vector<pkey_t> pkey;

#ifdef ANDROID
    Vibration vibration;
#endif

	void update();

	int find_key(int key_code);

	void press_key(int key_code, key_source source);
	void unpress_key(int key_code);
	void unpress_by_source(key_source source);
};

class Keyboard {
public:
	struct key_t {
		sf::Vector2f v[4];
		int key_code;
	};
	key_t keys_nav[5];

	KeyboardControl kc; // I don't remember why I separate this

	sf::RenderTexture frontend_layer_left; // numpad
	sf::RenderTexture frontend_layer_right; // dpad

	sf::Sprite sp_left;
	sf::Sprite sp_right;

	// 「实际点击有效网格」的布局参数（和 update_resize 里的 clamp/offset 一致，
	// 供 find_key_by_pos 命中检测使用，避免点击错位）。
	struct GridLayout {
		float kw = 0.f;        // 单格宽
		float kh = 0.f;        // 单格高
		int   cols = 3;        // 列数（3 固定）
		int   rows = 4;        // 行数（left=4, right=4 固定，right 画时只用到前3行）
		int   off_x = 0;       // 网格在 sp 内的 x 偏移（居中用）
		int   off_y = 0;       // 网格在 sp 内的 y 偏移（居中用）
	};
	GridLayout layout_left;
	GridLayout layout_right;

	// 软键栏（屏幕正下方一条横条）左右两半的命中区域，各对应一个软键
	sf::FloatRect softbar_l;
	sf::FloatRect softbar_r;

	sf::Sprite *screen;

	bool event(sf::Event& event);

	void update();

	void imgui_keyboard();

	void draw(sf::RenderTarget* rt);

	void draw_softbar(sf::RenderTarget* rt);

	void draw_press_key(sf::RenderTarget* rt, int key);

	int find_key_by_pos(int px, int py);

	void update_resize(int w, int h);
};