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
	for (int i = 0; i < kc.pkey.size(); ++i)
		draw_press_key(rt, kc.pkey[i].key_code);

	rt->draw(sp_left);
	rt->draw(sp_right);
}

const int key_map_left[4][3] =
{
	{VM_KEY_NUM1, VM_KEY_NUM2, VM_KEY_NUM3},
	{VM_KEY_NUM4, VM_KEY_NUM5, VM_KEY_NUM6},
	{VM_KEY_NUM7, VM_KEY_NUM8, VM_KEY_NUM9},
	{VM_KEY_STAR, VM_KEY_NUM0, VM_KEY_POUND}
};

const int key_map_right[4][3] =
{
	{VM_KEY_LEFT_SOFTKEY,	   VM_KEY_UP, VM_KEY_RIGHT_SOFTKEY},
	{		 VM_KEY_LEFT,	   VM_KEY_OK,		  VM_KEY_RIGHT},
	{		VM_KEY_CLEAR,	 VM_KEY_DOWN,		   VM_KEY_BACK},
	{		VM_KEY_CLEAR, MREMU_KEY_NONE,		   VM_KEY_BACK}
};

void Keyboard::draw_press_key(sf::RenderTarget* rt, int key) {
	sf::Color c(160, 75, 160);

	int x = -1, y = 0;
	sf::Vector2f offset_vec;
	float kw, kh;

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
		offset_vec = sf::Vector2f(
			sp_left.getPosition().x + (float)layout_left.off_x,
			sp_left.getPosition().y + (float)layout_left.off_y);
		kw = layout_left.kw;
		kh = layout_left.kh;
	}
	else {
		offset_vec = sf::Vector2f(
			sp_right.getPosition().x + (float)layout_right.off_x,
			sp_right.getPosition().y + (float)layout_right.off_y);
		kw = layout_right.kw;
		kh = layout_right.kh;

		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				if (key_map_right[i][j] == key) {
					x = j, y = i;

					if (!(std::abs(j - 1) == 1 && std::abs(i - 1)) == 1)
						offset_vec.y += kh / 2.f;
					else if (i == 2)
						offset_vec.y += kh;

					break;
				}
	}

	sf::Vertex v[4];

	if (x != -1) {
		v[0].position = sf::Vector2f(x * kw, y * kh);
		v[1].position = sf::Vector2f((x + 1) * kw, y * kh);
		v[2].position = sf::Vector2f((x + 1) * kw, (y + 1) * kh);
		v[3].position = sf::Vector2f(x * kw, (y + 1) * kh);

		for (int j = 0; j < 4; ++j) {
			v[j].position += offset_vec;
			v[j].color = c;
		}
		rt->draw(v, 4, sf::TriangleFan);
	}
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

		if ((kpx == 0 || kpx == 2) && (kpy == 0 || kpy == 3))
			return key_map_right[kpy][kpx];

		{
			float local_y = (float)(py - y);
			kpy = (int)std::floor((local_y - kh / 2.f) / kh);

			if (kpy >= 0 && kpy < 3 && !(std::abs(kpx - 1) == 1 && std::abs(kpy - 1) == 1))
				return key_map_right[kpy][kpx];
		}
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
	{u"\u2014", u"\u2B06", u"\u2014"},
	{u"\u2B05", u"OK", u"\u2B95"},
	{u"\u232B", u"\u2B07", u"\u21A9"},
};

void Keyboard::update_resize(int win_w, int win_h) {
	int screen_x = (int)screen->getPosition().x;
	int screen_y = (int)screen->getPosition().y;
	int screen_w = (int)(screen->getScale().x * screen->getTextureRect().width);
	int screen_h = (int)(screen->getScale().y * screen->getTextureRect().height);

	// ===== 国产直板功能机：D-pad(3×4 上方) + 数字(3×4 下方) 上下竖排 =====
	sf::IntRect left, right;
	{
		// 键盘整体在屏幕下方，距离屏幕底部留 10px
		int kb_top   = screen_y + screen_h + 10;
		int kb_left  = (win_w - screen_w) / 2;
		int kb_w     = screen_w;
		int kb_h     = std::max(1, win_h - kb_top - 14);
		int kb_right = kb_left + kb_w;
		(void)kb_right;

		// 上下两块：RIGHT（D-pad）3×4 在上，LEFT（数字）3×4 在下
		int half_h = kb_h / 2;
		right = { kb_left, kb_top,          kb_w, half_h };
		left  = { kb_left, kb_top + half_h, kb_w, kb_h - half_h };
	}

	// —— 键格尺寸上限 ——
	const float MAX_KW   = 92.f;
	const float MAX_KH_L = 48.f;
	const float MAX_KH_R = 44.f;

	// 数字 12 键副字母标签
	static const char16_t* SUBLABEL[4][3] = {
		{u"",     u"abc",  u"def"},
		{u"ghi",  u"jkl",  u"mno"},
		{u"pqrs", u"tuv",  u"wxyz"},
		{u"+",    u"",     u"#"}
	};

	{
		int w = left.width, h = left.height;

		const float GAP = 3.f;          // 减小间隙
		const float BTN_RC = 4.f;       // 减小圆角

		float kw = std::min(MAX_KW,   ((float)w - 2.f * GAP) / 3.f - GAP);
		float kh = std::min(MAX_KH_L, ((float)h - 2.f * GAP) / 4.f - GAP);
		if (kw < 28.f) kw = 28.f;
		if (kh < 36.f) kh = 36.f;
		int draw_w = (int)(3.f * kw + 4.f * GAP);
		int draw_h = (int)(4.f * kh + 5.f * GAP);
		int off_x  = (w - draw_w) / 2;
		int off_y  = (h - draw_h) / 2;
		layout_left.kw    = kw + GAP;
		layout_left.kh    = kh + GAP;
		layout_left.cols  = 3;
		layout_left.rows  = 4;
		layout_left.off_x = off_x;
		layout_left.off_y = off_y;

		frontend_layer_left.create(w, h);
		frontend_layer_left.clear(sf::Color::Transparent);

		sp_left = sf::Sprite(frontend_layer_left.getTexture());
		sp_left.setPosition((float)left.left, (float)left.top);

		for (int iy = 0; iy < 4; ++iy)
			for (int ix = 0; ix < 3; ++ix) {
				float bx = (float)off_x + GAP + (float)ix * (kw + GAP);
				float by = (float)off_y + GAP + (float)iy * (kh + GAP);

				sf::RectangleShape btn(sf::Vector2f(kw, kh));
				btn.setPosition(bx, by);
				btn.setFillColor(sf::Color(250, 250, 252));
				btn.setOutlineColor(sf::Color(170, 172, 180));
				btn.setOutlineThickness(1.f);
				frontend_layer_left.draw(btn);

				sf::CircleShape cr(BTN_RC, 20);
				cr.setFillColor(sf::Color(250, 250, 252));
				cr.setOutlineColor(sf::Color(170, 172, 180));
				cr.setOutlineThickness(1.f);
				auto drawRC = [&](float cx, float cy) {
					cr.setPosition(cx - BTN_RC, cy - BTN_RC);
					frontend_layer_left.draw(cr);
				};
				drawRC(bx,         by);
				drawRC(bx + kw,    by);
				drawRC(bx,         by + kh);
				drawRC(bx + kw,    by + kh);

				auto tex_main = u16text_to_texture(keys_marks_left[iy][ix], sf::Color(50, 50, 58));
				sf::Sprite sp_main(tex_main);
				float scale_main = std::min((kh - 14.f) / std::max(1.f, (float)tex_main.getSize().y),
				                           (kw - 8.f) / std::max(1.f, (float)tex_main.getSize().x));
				if (scale_main < 1.f) scale_main = 1.f;
				if (scale_main > 1.8f) scale_main = 1.8f; // 限制最大，避免字太大
				sp_main.setScale(scale_main, scale_main);
				sp_main.setOrigin(
					(float)sp_main.getTextureRect().width / 2.f,
					(float)sp_main.getTextureRect().height / 2.f);
				sp_main.setPosition(bx + kw / 2.f - scale_main,
				                    by + kh / 2.f - 4.f * scale_main);
				frontend_layer_left.draw(sp_main);

				const char16_t* sub = SUBLABEL[iy][ix];
				if (sub && sub[0] != u'\0') {
					auto tex_sub = u16text_to_texture(sub, sf::Color(120, 122, 130));
					sf::Sprite sp_sub(tex_sub);
					float scale_sub = 1.f;
					float sw = (float)tex_sub.getSize().x * scale_sub;
					float sh = (float)tex_sub.getSize().y * scale_sub;
					sp_sub.setScale(scale_sub, scale_sub);
					sp_sub.setPosition(bx + kw - sw - 4.f,
					                   by + kh - sh - 2.f);
					frontend_layer_left.draw(sp_sub);
				}
			}

		frontend_layer_left.display();
	}

	{
		int w = right.width, h = right.height;

		float kw = std::min(MAX_KW,   (float)(w - 1) / 3.f);
		float kh = std::min(MAX_KH_R, (float)(h - 1) / 4.f);
		int draw_w = (int)(kw * 3.f + 1.f);
		int draw_h = (int)(kh * 4.f + 1.f);
		int off_x  = (w - draw_w) / 2;
		int off_y  = (h - draw_h) / 2;
		layout_right.kw    = kw;
		layout_right.kh    = kh;
		layout_right.cols  = 3;
		layout_right.rows  = 4;
		layout_right.off_x = off_x;
		layout_right.off_y = off_y;

		frontend_layer_right.create(w, h);
		frontend_layer_right.clear(sf::Color::Transparent);
		sp_right = sf::Sprite(frontend_layer_right.getTexture());
		sp_right.setPosition((float)right.left, (float)right.top);
		frontend_layer_right.display();
	}
}
