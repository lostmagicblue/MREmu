#include "AppManager.h"
#include "Keyboard.h"
#include <vmio.h>
#include <vector>
#include <SFML/Graphics.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

KeyboardControl::pkey_t::pkey_t(int key_code, key_source source) {
	this->key_code = key_code;
	this->source = source;
}

void KeyboardControl::update() {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].tim.getElapsedTime().asMilliseconds() >= 500) {
			pkey[i].tim.restart();
			if (pkey[i].key_status < VM_KEY_EVENT_REPEAT)
				pkey[i].key_status++;
		}
}

int KeyboardControl::find_key(int key_code) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].key_code == key_code)
			return i;
	return -1;
}

void KeyboardControl::press_key(int key_code, key_source source) {
	if (key_code == MREMU_KEY_NONE)
		return;

	int i = find_key(key_code);
	if (i == -1) {
#ifdef ANDROID
		if (Touch0 <= source && source <= Touch9) {
			vibration.vibrate(sf::milliseconds(50));
		}
#endif

		pkey.push_back(pkey_t(key_code, source));
		add_keyboard_event(VM_KEY_EVENT_DOWN, key_code);
	}
}

void KeyboardControl::unpress_key(int key_code) {
	int i = find_key(key_code);
	if (i != -1) {
		pkey.erase(pkey.begin() + i);
		add_keyboard_event(VM_KEY_EVENT_UP, key_code);
	}
}

void KeyboardControl::unpress_by_source(key_source source) {
	for (int i = 0; i < pkey.size(); ++i)
		if (pkey[i].source == source) {
#ifdef ANDROID
			if (Touch0 <= source && source <= Touch9) {
				vibration.vibrate(sf::milliseconds(40));
			}
#endif

			add_keyboard_event(VM_KEY_EVENT_UP, pkey[i].key_code);
			pkey.erase(pkey.begin() + i);
			--i;
		}
}

struct Keys {
	char name[20] = "";
	int code = 0;
};
const Keys keys_imgui[3 * 7] =
{
	{"Left S",VM_KEY_LEFT_SOFTKEY},
	{"UP",VM_KEY_UP},
	{"Right S",VM_KEY_RIGHT_SOFTKEY},
	{"LEFT",VM_KEY_LEFT},
	{"OK",VM_KEY_OK},
	{"RIGHT",VM_KEY_RIGHT},
	{"C",VM_KEY_CLEAR},
	{"Down",VM_KEY_DOWN},
	{"Back",VM_KEY_BACK},
	{"1.,",VM_KEY_NUM1},
	{"2abc",VM_KEY_NUM2},
	{"3def",VM_KEY_NUM3},
	{"4ghi",VM_KEY_NUM4},
	{"5jkl",VM_KEY_NUM5},
	{"6mno",VM_KEY_NUM6},
	{"7pqrs",VM_KEY_NUM7},
	{"8tuv",VM_KEY_NUM8},
	{"9wxyz",VM_KEY_NUM9},
	{"*",VM_KEY_STAR},
	{"0",VM_KEY_NUM0},
	{"#",VM_KEY_POUND},
};

const std::map<sf::Keyboard::Key, int> key_to_key =
{
	{sf::Keyboard::Up, VM_KEY_UP},
	{sf::Keyboard::Down, VM_KEY_DOWN},
	{sf::Keyboard::Left, VM_KEY_LEFT},
	{sf::Keyboard::Right, VM_KEY_RIGHT},
	{sf::Keyboard::Slash, VM_KEY_LEFT_SOFTKEY},
	{sf::Keyboard::RShift, VM_KEY_RIGHT_SOFTKEY},
	{sf::Keyboard::Enter, VM_KEY_OK},
	{sf::Keyboard::BackSpace, VM_KEY_CLEAR},
	{sf::Keyboard::Escape, VM_KEY_BACK},
	{sf::Keyboard::Numpad7, VM_KEY_NUM1},
	{sf::Keyboard::Numpad8, VM_KEY_NUM2},
	{sf::Keyboard::Numpad9, VM_KEY_NUM3},
	{sf::Keyboard::Numpad4, VM_KEY_NUM4},
	{sf::Keyboard::Numpad5, VM_KEY_NUM5},
	{sf::Keyboard::Numpad6, VM_KEY_NUM6},
	{sf::Keyboard::Numpad1, VM_KEY_NUM7},
	{sf::Keyboard::Numpad2, VM_KEY_NUM8},
	{sf::Keyboard::Numpad3, VM_KEY_NUM9},
	{sf::Keyboard::Divide, VM_KEY_STAR},
	{sf::Keyboard::Numpad0, VM_KEY_NUM0},
	{sf::Keyboard::Multiply, VM_KEY_POUND},
};

bool Keyboard::event(sf::Event& event) {
	switch (event.type) {
	case sf::Event::KeyPressed:
	case sf::Event::KeyReleased: {
		const auto& el = key_to_key.find(event.key.code);

		if (el != key_to_key.end()) {
			if (event.type == sf::Event::KeyPressed)
				kc.press_key(el->second, KeyboardControl::Keyboard);
			else
				kc.unpress_key(el->second);
		}
		return true;
	}
	case sf::Event::MouseButtonPressed:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			kc.press_key(find_key_by_pos(event.mouseButton.x, event.mouseButton.y), kc.Mouse);
		return true;
	case sf::Event::MouseButtonReleased:
		if (event.mouseButton.button == sf::Mouse::Button::Left)
			kc.unpress_by_source(kc.Mouse);
		return true;
	case sf::Event::TouchBegan:
		kc.press_key(find_key_by_pos(event.touch.x, event.touch.y),
			(KeyboardControl::key_source)(kc.Touch0 + event.touch.finger));
		return true;
	case sf::Event::TouchEnded:
		kc.unpress_by_source((KeyboardControl::key_source)(kc.Touch0 + event.touch.finger));
		return true;
	}
	return false;
}

void Keyboard::imgui_keyboard() {
	ImVec2 v = { 60,20 };
	ImGui::Begin("KeyBoard");
	for (int i = 0; i < 3 * 7; ++i) {
		if (i % 3 != 0)
			ImGui::SameLine();

		bool presed = kc.find_key(keys_imgui[i].code) != -1;

		if (presed) {
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(160, 75, 160));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(160, 75, 160));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(160, 75, 160));
		}

		if (ImGui::Button(keys_imgui[i].name, v))
			kc.unpress_key(keys_imgui[i].code);
		if (ImGui::IsItemClicked())
			kc.press_key(keys_imgui[i].code, KeyboardControl::ImGui);

		if (presed)
			ImGui::PopStyleColor(3);
	}
	ImGui::End();
}

void Keyboard::draw(sf::RenderTarget* rt) {
	// 先画键盘本体（不透明层），再画按下高亮（否则高亮会被层盖住看不见）
	rt->draw(sp_left);
	rt->draw(sp_right);

	for (int i = 0; i < kc.pkey.size(); ++i)
		draw_press_key(rt, kc.pkey[i].key_code);
}

const int key_map_left[4][3] =
{
	{VM_KEY_NUM1, VM_KEY_NUM2, VM_KEY_NUM3},
	{VM_KEY_NUM4, VM_KEY_NUM5, VM_KEY_NUM6},
	{VM_KEY_NUM7, VM_KEY_NUM8, VM_KEY_NUM9},
	{VM_KEY_STAR, VM_KEY_NUM0, VM_KEY_POUND}
};

const int key_map_right[2][3] =
{
	{	 VM_KEY_LEFT,	 VM_KEY_UP,	   VM_KEY_RIGHT},
	{VM_KEY_CLEAR,	  VM_KEY_OK,	  VM_KEY_DOWN}
};

void Keyboard::draw_press_key(sf::RenderTarget* rt, int key) {
	sf::FloatRect rect;
	bool found = false;

	// —— 软键栏两半（菜单 = 左软键，返回 = 右软键）——
	if (key == VM_KEY_LEFT_SOFTKEY) {
		rect = softbar_l;
		found = true;
	}
	else if (key == VM_KEY_RIGHT_SOFTKEY) {
		rect = softbar_r;
		found = true;
	}

	// —— 数字区（4×3）——
	if (!found) {
		int x = -1, y = 0;
		if (VM_KEY_NUM1 <= key && key <= VM_KEY_NUM9) {
			x = (key - VM_KEY_NUM1) % 3;
			y = (key - VM_KEY_NUM1) / 3;
		}
		else
			switch (key) {
			case VM_KEY_STAR:
				x = 0, y = 3;
				break;
			case VM_KEY_NUM0:
				x = 1, y = 3;
				break;
			case VM_KEY_POUND:
				x = 2, y = 3;
				break;
			}

		if (x != -1) {
			rect = sf::FloatRect(
				sp_left.getPosition().x + x * layout_left.kw,
				sp_left.getPosition().y + y * layout_left.kh,
				layout_left.kw, layout_left.kh);
			found = true;
		}
	}

	// —— 导航区（2×3）——
	if (!found) {
		for (int i = 0; i < GRID_NAV_ROWS; ++i)
			for (int j = 0; j < 3; ++j)
				if (key_map_right[i][j] == key) {
					rect = sf::FloatRect(
						sp_right.getPosition().x + j * layout_right.kw,
						sp_right.getPosition().y + i * layout_right.kh,
						layout_right.kw, layout_right.kh);
					found = true;
					break;
				}
	}

	if (!found)
		return;

	// 半透明白高亮：蓝底格子上像按下的反光
	sf::Vertex v[4] = {
		sf::Vertex(sf::Vector2f(rect.left,               rect.top),                sf::Color(255, 255, 255, 80)),
		sf::Vertex(sf::Vector2f(rect.left + rect.width,  rect.top),                sf::Color(255, 255, 255, 80)),
		sf::Vertex(sf::Vector2f(rect.left + rect.width,  rect.top + rect.height),  sf::Color(255, 255, 255, 80)),
		sf::Vertex(sf::Vector2f(rect.left,               rect.top + rect.height),  sf::Color(255, 255, 255, 80)),
	};
	rt->draw(v, 4, sf::TriangleFan);
}

static bool in_box(int x, int y, int bx, int by, int bw, int bh) {
	return x >= bx && y >= by && x < bx + bw && y < by + bh;
}

static bool in_box(int x, int y, sf::Sprite sp) {
	return in_box(x, y, sp.getPosition().x, sp.getPosition().y, sp.getTextureRect().width, sp.getTextureRect().height);
}

int Keyboard::find_key_by_pos(int px, int py) {
	// 软键栏两半（菜单/返回）— 屏幕正下方那条横条
	if (softbar_l.contains((float)px, (float)py))
		return VM_KEY_LEFT_SOFTKEY;
	if (softbar_r.contains((float)px, (float)py))
		return VM_KEY_RIGHT_SOFTKEY;

	if (in_box(px, py, sp_left)) {
		int x = (int)sp_left.getPosition().x + layout_left.off_x;
		int y = (int)sp_left.getPosition().y + layout_left.off_y;

		float kw = layout_left.kw;
		float kh = layout_left.kh;

		int kpx = (int)((float)(px - x) / kw);
		int kpy = (int)((float)(py - y) / kh);
		if (kpx < 0 || kpx >= layout_left.cols) return MREMU_KEY_NONE;
		if (kpy < 0 || kpy >= layout_left.rows) return MREMU_KEY_NONE;

		return key_map_left[kpy][kpx];
	}
	if (in_box(px, py, sp_right)) {
		int x = (int)sp_right.getPosition().x + layout_right.off_x;
		int y = (int)sp_right.getPosition().y + layout_right.off_y;

		float kw = layout_right.kw;
		float kh = layout_right.kh;

		int kpx = (int)((float)(px - x) / kw);
		int kpy = (int)((float)(py - y) / kh);
		if (kpx < 0 || kpx >= layout_right.cols) return MREMU_KEY_NONE;
		if (kpy < 0 || kpy >= layout_right.rows) return MREMU_KEY_NONE;

		return key_map_right[kpy][kpx];
	}

	return MREMU_KEY_NONE;
}

static sf::Vector2i size_by_aspect_ratio(int bw, int bh, float ratio) {
	int h = bh;
	int w = (float)(h) * ratio;

	if (w > bw) {
		w = bw;
		h = (float)(w) / ratio;
	}

	return { w, h };
}

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

const char16_t* keys_marks_left[4][3] = {
	{u"1", u"2", u"3"},
	{u"4", u"5", u"6"},
	{u"7", u"8", u"9"},
	{u"*", u"0", u"#"},
};

const char16_t* keys_marks_right[2][3] = {
	{u"\u2190", u"\u2191", u"\u2192"},   // ← ↑ →
	{u"C",      u"OK",   u"\u2193"},    // C  OK  ↓
};

// 键盘格子的外观（参照真机 VXP 截图：蓝底 + 白字 + 细白格线，一整块对齐）
static const sf::Color KB_CELL(26, 156, 224);   // 格子蓝
static const sf::Color KB_LINE(235, 243, 250);  // 格线浅白

// 在 layer 上画一个 rows×3 的网格块（格子内缩 1px 露出底色 → 均匀细格线）
static void draw_key_grid(sf::RenderTexture& layer, int rows,
                          const char16_t* marks[][3]) {
	float kw = (float)layer.getSize().x / 3.f;
	float kh = (float)layer.getSize().y / rows;

	for (int iy = 0; iy < rows; ++iy)
		for (int ix = 0; ix < 3; ++ix) {
			sf::RectangleShape cell(sf::Vector2f(kw - 2.f, kh - 2.f));
			cell.setPosition(ix * kw + 1.f, iy * kh + 1.f);
			cell.setFillColor(KB_CELL);
			layer.draw(cell);

			auto tex = u16text_to_texture(marks[iy][ix], sf::Color::White);
			sf::Sprite sp(tex);
			float s = std::min((kh - 14.f) / std::max(1.f, (float)tex.getSize().y),
			                   (kw * 0.6f) / std::max(1.f, (float)tex.getSize().x));
			if (s > 1.6f) s = 1.6f;
			if (s < 1.f) s = 1.f;
			sp.setScale(s, s);
			sp.setPosition(ix * kw + (kw - (float)tex.getSize().x * s) / 2.f,
			               iy * kh + (kh - (float)tex.getSize().y * s) / 2.f);
			layer.draw(sp);
		}
}

void Keyboard::update_resize(int win_w, int win_h) {
	int screen_x = (int)screen->getPosition().x;
	int screen_y = (int)screen->getPosition().y;
	(void)screen_x;
	int screen_h = (int)(screen->getScale().y * screen->getTextureRect().height);

	// ===== 布局（参照真机 VXP 截图，自上而下，全部满窗宽、零留白）=====
	// [屏幕] [软键栏 菜单|返回] [导航 2×3] [数字 4×3]
	float sb_y = (float)(screen_y + screen_h);
	softbar_l = sf::FloatRect(0.f,         sb_y, (float)win_w / 2.f, SOFTBAR_H);
	softbar_r = sf::FloatRect((float)win_w / 2.f, sb_y, (float)win_w / 2.f, SOFTBAR_H);

	int   grid_top = screen_y + screen_h + (int)SOFTBAR_H;
	int   grid_h   = std::max(1, win_h - grid_top);
	float kw       = (float)win_w / 3.f;
	float kh       = (float)grid_h / (GRID_NAV_ROWS + GRID_NUM_ROWS);

	// —— 导航 2×3（在上）——
	{
		int h = (int)(kh * GRID_NAV_ROWS);

		layout_right.kw = kw;  layout_right.kh = kh;
		layout_right.cols = 3; layout_right.rows = GRID_NAV_ROWS;
		layout_right.off_x = 0; layout_right.off_y = 0;

		frontend_layer_right.create(win_w, h);
		frontend_layer_right.clear(KB_LINE);
		draw_key_grid(frontend_layer_right, GRID_NAV_ROWS, keys_marks_right);
		frontend_layer_right.display();

		sp_right = sf::Sprite(frontend_layer_right.getTexture());
		sp_right.setPosition(0.f, (float)grid_top);
	}

	// —— 数字 4×3（在导航下面）——
	{
		int h = (int)(kh * GRID_NUM_ROWS);

		layout_left.kw = kw;  layout_left.kh = kh;
		layout_left.cols = 3; layout_left.rows = GRID_NUM_ROWS;
		layout_left.off_x = 0; layout_left.off_y = 0;

		frontend_layer_left.create(win_w, h);
		frontend_layer_left.clear(KB_LINE);
		draw_key_grid(frontend_layer_left, GRID_NUM_ROWS, keys_marks_left);
		frontend_layer_left.display();

		sp_left = sf::Sprite(frontend_layer_left.getTexture());
		sp_left.setPosition(0.f, (float)grid_top + kh * GRID_NAV_ROWS);
	}
}

void Keyboard::draw_softbar(sf::RenderTarget* rt) {
	// 软键栏：屏幕正下方一条深蓝横条，左「菜单」= 左软键，右「返回」= 右软键（两半都可点击）
	float w = softbar_l.width + softbar_r.width;
	float y = softbar_l.top;

	sf::RectangleShape bar(sf::Vector2f(w, SOFTBAR_H));
	bar.setPosition(0.f, y);
	bar.setFillColor(sf::Color(33, 88, 148));
	rt->draw(bar);

	auto tex_l = u16text_to_texture(u"菜单", sf::Color::White);
	auto tex_r = u16text_to_texture(u"返回", sf::Color::White);
	sf::Sprite sl(tex_l), sr(tex_r);
	sl.setPosition(10.f, y + (SOFTBAR_H - (float)tex_l.getSize().y) / 2.f);
	sr.setPosition(w - (float)tex_r.getSize().x - 10.f,
	               y + (SOFTBAR_H - (float)tex_r.getSize().y) / 2.f);
	rt->draw(sl);
	rt->draw(sr);
}