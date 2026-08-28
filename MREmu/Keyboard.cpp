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

const int key_map_right[3][3] =
{
	{VM_KEY_LEFT_SOFTKEY, VM_KEY_UP,  VM_KEY_RIGHT_SOFTKEY},  // LeftS ↑ RightS
	{VM_KEY_LEFT,         VM_KEY_OK,  VM_KEY_RIGHT},          // ←  OK  →
	{MREMU_KEY_NONE,      VM_KEY_DOWN, MREMU_KEY_NONE}        // 空   ↓   空（占位，不画）
};

void Keyboard::draw_press_key(sf::RenderTarget* rt, int key) {
	sf::Vector2f sp_pos;
	float kw = 0.f, kh = 0.f;
	int x = -1, y = 0;

	// —— 数字区（4×3）——
	if (VM_KEY_NUM1 <= key && key <= VM_KEY_NUM9) {
		x = (key - VM_KEY_NUM1) % 3;
		y = (key - VM_KEY_NUM1) / 3;
		sp_pos = sp_left.getPosition();
		kw = layout_left.kw;
		kh = layout_left.kh;
	}
	else
		switch (key) {
		case VM_KEY_STAR:
			x = 0, y = 3;
			sp_pos = sp_left.getPosition();
			kw = layout_left.kw;
			kh = layout_left.kh;
			break;
		case VM_KEY_NUM0:
			x = 1, y = 3;
			sp_pos = sp_left.getPosition();
			kw = layout_left.kw;
			kh = layout_left.kh;
			break;
		case VM_KEY_POUND:
			x = 2, y = 3;
			sp_pos = sp_left.getPosition();
			kw = layout_left.kw;
			kh = layout_left.kh;
			break;
		}

	// —— 导航区（3×3：LeftS ↑ RightS / ← OK → / 空 ↓ 空）——
	if (x == -1) {
		for (int i = 0; i < GRID_NAV_ROWS; ++i)
			for (int j = 0; j < 3; ++j)
				if (key_map_right[i][j] == key) {
					x = j, y = i;
					sp_pos = sp_right.getPosition();
					kw = layout_right.kw;
					kh = layout_right.kh;
					break;
				}
	}

	if (x == -1)
		return;

	// 按下效果：键面（避开间隙）叠一层半透明深色，对应 HTML 的 :active 内阴影
	float bx = sp_pos.x + x * kw + KEY_GAP / 2.f;
	float by = sp_pos.y + y * kh + KEY_GAP / 2.f;
	float bw = kw - KEY_GAP;
	float bh = kh - KEY_GAP;

	sf::Vertex v[4] = {
		sf::Vertex(sf::Vector2f(bx,      by),      sf::Color(0, 0, 0, 45)),
		sf::Vertex(sf::Vector2f(bx + bw, by),      sf::Color(0, 0, 0, 45)),
		sf::Vertex(sf::Vector2f(bx + bw, by + bh), sf::Color(0, 0, 0, 45)),
		sf::Vertex(sf::Vector2f(bx,      by + bh), sf::Color(0, 0, 0, 45)),
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

const char16_t* keys_marks_right[3][3] = {
	{u"菜单",   u"\u2191", u"返回"},    // 左选择键 / ↑ / 右选择键
	{u"\u2190", u"OK",    u"\u2192"},  // ←  OK  →
	{u"",       u"\u2193", u""},       // 空占位 / ↓ / 空占位（不画）
};

// 白色圆角按键（对照 HTML 样式稿：白底 + 浅灰描边 + 4px 圆角 + 底部浅阴影 + 深灰字）
static void draw_rounded_box(sf::RenderTexture& rt, float x, float y, float w, float h,
                             sf::Color fill, sf::Color line) {
	sf::RectangleShape body(sf::Vector2f(w, h));
	body.setPosition(x, y);
	body.setFillColor(fill);
	if (line.a) {
		body.setOutlineColor(line);
		body.setOutlineThickness(1.f);
	}
	rt.draw(body);

	sf::CircleShape c(4.f, 12);
	c.setFillColor(fill);
	if (line.a) {
		c.setOutlineColor(line);
		c.setOutlineThickness(1.f);
	}
	auto corner = [&](float cx, float cy) {
		c.setPosition(cx - 4.f, cy - 4.f);
		rt.draw(c);
	};
	corner(x, y);
	corner(x + w, y);
	corner(x, y + h);
	corner(x + w, y + h);
}

// 画一个 rows×3 的按键块；key_map 里 MREMU_KEY_NONE 的格子是空占位，透出机身灰底
static void draw_key_grid(sf::RenderTexture& layer, int rows, float kh_real,
                          const char16_t* marks[][3], const int key_map[][3]) {
	float kw = (float)layer.getSize().x / 3.f;

	for (int iy = 0; iy < rows; ++iy)
		for (int ix = 0; ix < 3; ++ix) {
			if (key_map[iy][ix] == MREMU_KEY_NONE)
				continue;

			float bx = ix * kw + KEY_GAP / 2.f;
			float by = iy * (kh_real + KEY_GAP) + KEY_GAP / 2.f;
			float bw = kw - KEY_GAP;
			float bh = kh_real;

			// 底部浅阴影（下移 1px 的深色圆角块）
			draw_rounded_box(layer, bx, by + 1.f, bw, bh, sf::Color(0, 0, 0, 22), sf::Color::Transparent);
			// 键体：白底 + 浅灰描边
			draw_rounded_box(layer, bx, by, bw, bh, sf::Color::White, sf::Color(208, 208, 208));

			// 键面文字：深灰 #333，居中，按格子尺寸自适应
			auto tex = u16text_to_texture(marks[iy][ix], sf::Color(51, 51, 51));
			sf::Sprite sp(tex);
			float s = std::min((bh - 14.f) / std::max(1.f, (float)tex.getSize().y),
			                   (bw * 0.7f) / std::max(1.f, (float)tex.getSize().x));
			if (s > 1.6f) s = 1.6f;
			if (s < 1.f) s = 1.f;
			sp.setScale(s, s);
			sp.setPosition(bx + (bw - (float)tex.getSize().x * s) / 2.f,
			               by + (bh - (float)tex.getSize().y * s) / 2.f);
			layer.draw(sp);
		}
}

void Keyboard::update_resize(int win_w, int win_h) {
	int screen_y = (int)screen->getPosition().y;
	int screen_h = (int)(screen->getScale().y * screen->getTextureRect().height);

	// ===== 布局（对照 HTML 样式稿）：浅灰机身留白 + 黑屏 + 导航 3×3 + 数字 4×3，白色圆角键 =====
	float col_w = (float)win_w - 2.f * BODY_PAD;
	float top   = (float)(screen_y + screen_h) + BODY_GAP;
	float nav_h = GRID_NAV_ROWS * NAV_KH + (GRID_NAV_ROWS - 1) * KEY_GAP;

	// —— 导航 3×3（LeftS ↑ RightS / ← OK → / 空 ↓ 空）——
	{
		layout_right.kw = col_w / 3.f;
		layout_right.kh = NAV_KH + KEY_GAP;   // 命中格子含下方间隙，区域连贯好点
		layout_right.cols = 3;                layout_right.rows = GRID_NAV_ROWS;
		layout_right.off_x = 0;               layout_right.off_y = 0;

		frontend_layer_right.create((int)col_w, (int)nav_h);
		frontend_layer_right.clear(sf::Color::Transparent);   // 键缝透出浅灰机身底
		draw_key_grid(frontend_layer_right, GRID_NAV_ROWS, NAV_KH, keys_marks_right, key_map_right);
		frontend_layer_right.display();

		sp_right = sf::Sprite(frontend_layer_right.getTexture());
		sp_right.setPosition(BODY_PAD, top);
	}

	// —— 数字 4×3 ——
	{
		float top2  = top + nav_h + BODY_GAP;
		float num_h = GRID_NUM_ROWS * NUM_KH + (GRID_NUM_ROWS - 1) * KEY_GAP;

		layout_left.kw = col_w / 3.f;
		layout_left.kh = NUM_KH + KEY_GAP;
		layout_left.cols = 3;                 layout_left.rows = GRID_NUM_ROWS;
		layout_left.off_x = 0;                layout_left.off_y = 0;

		frontend_layer_left.create((int)col_w, (int)num_h);
		frontend_layer_left.clear(sf::Color::Transparent);
		draw_key_grid(frontend_layer_left, GRID_NUM_ROWS, NUM_KH, keys_marks_left, key_map_left);
		frontend_layer_left.display();

		sp_left = sf::Sprite(frontend_layer_left.getTexture());
		sp_left.setPosition(BODY_PAD, top2);
	}
}