#include <iostream>
#include <thread>

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Memory.h"
#include "Cpu.h"
#include "GDB.h"
#include "Bridge.h"
#include "App.h"
#include "AppManager.h"
#include "Keyboard.h"
#include "Touch.h"
#include "Log.h"

#include "MREngine/Graphic.h"
#include "MREngine/IO.h"
#include "MREngine/SIM.h"
#include "MREngine/CharSet.h"
#include <cmdparser.hpp>

#include "NativeApps/Menu/AppSelector.h"

sf::Clock global_clock;

bool work = true;

std::string error_message = "";
bool show_error = false;

AppManager* g_appManager = 0;

#ifdef ANDROID
#include <spdlog/sinks/android_sink.h>

std::atomic<int> storage_permission_state{0};

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_notifyPermissionState(JNIEnv *env, jobject thiz, jboolean granted) {
    storage_permission_state = granted ? 1 : -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile(JNIEnv *env, jobject thiz, jstring j_path) {
    const char *path_cstr = env->GetStringUTFChars(j_path, nullptr);

    while(!g_appManager)
        sf::sleep(sf::seconds(0.2));

    g_appManager->add_app_for_launch(path_cstr, false);

    env->ReleaseStringUTFChars(path_cstr, path_cstr);
}
#endif


void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

		GDB::update();
		appManager.update(delta_ms);

		sf::sleep(sf::milliseconds(1000 / 120));
	}
}

// ================ 手机外壳绘制（win_device 背景）================
// 画一个圆角矩形 + 听筒 + 屏幕框（屏幕在里面，sprite 我们按 screen_sp 的位置自己放）
static void draw_phone_frame(sf::RenderWindow& win, const sf::FloatRect& screen_rect) {
	// 国产直板功能机：机身就是整个窗口，背景色已经是机身银灰（外面 win.clear() 保证没黑底）。
	// 这里只负责画：深灰边线 + 四角圆角遮盖 + 屏幕黑框 + 顶部听筒/前摄 + 屏幕下方功能行装饰 + 底部扬声器小孔。
	float win_w = (float)win.getSize().x;
	float win_h = (float)win.getSize().y;

	const float r = 26.f;   // 四角圆角半径

	// ===== 1. 机身边线（让用户看见手机轮廓，虽然背景色就是机身色） =====
	sf::RectangleShape body(sf::Vector2f(win_w, win_h));
	body.setPosition(0.f, 0.f);
	body.setFillColor(sf::Color(178, 180, 188));
	body.setOutlineColor(sf::Color(110, 112, 120));
	body.setOutlineThickness(2.f);
	win.draw(body);

	auto drawCorner = [&](float cx, float cy) {
		sf::CircleShape c(r, 30);
		c.setFillColor(sf::Color(178, 180, 188));
		c.setOutlineColor(sf::Color(110, 112, 120));
		c.setOutlineThickness(2.f);
		c.setPosition(cx - r, cy - r);
		win.draw(c);
	};
	drawCorner(0.f,    0.f   );
	drawCorner(win_w,  0.f   );
	drawCorner(0.f,    win_h );
	drawCorner(win_w,  win_h );

	// ===== 2. 屏幕黑框（1px 深灰边） =====
	sf::RectangleShape screen_frame(sf::Vector2f(screen_rect.width + 4.f, screen_rect.height + 4.f));
	screen_frame.setPosition(screen_rect.left - 2.f, screen_rect.top - 2.f);
	screen_frame.setFillColor(sf::Color::Black);
	screen_frame.setOutlineColor(sf::Color(60, 60, 70));
	screen_frame.setOutlineThickness(1.f);
	win.draw(screen_frame);

	// ===== 3. 顶部：细长听筒槽 + 微型前摄（金立长虹那种非常简洁的顶） =====
	sf::CircleShape ear(4.5f, 30);
	ear.setScale(3.2f, 0.55f);
	ear.setFillColor(sf::Color(40, 40, 48));
	ear.setOutlineColor(sf::Color(90, 90, 100));
	ear.setOutlineThickness(0.8f);
	ear.setPosition(
		win_w / 2.f - ear.getScale().x * 4.5f,
		screen_rect.top / 2.f - ear.getScale().y * 4.5f);
	win.draw(ear);

	sf::CircleShape cam(2.4f, 18);
	cam.setFillColor(sf::Color(30, 30, 40));
	cam.setOutlineColor(sf::Color(100, 100, 110));
	cam.setOutlineThickness(0.6f);
	cam.setPosition(win_w - 28.f, screen_rect.top / 2.f - 2.4f);
	win.draw(cam);

	// ===== 4. 屏幕下方「国产功能键区」装饰（和 Keyboard RIGHT 的 D-pad 位置对齐；装饰和功能键两套都在这一整块区域）
	// 只画软键 + 通话绿 + OK 圆盘（内嵌 D-pad 四向三角）+ 挂机红
	float nav_top = screen_rect.top + screen_rect.height + 14.f;
	float cx      = win_w / 2.f;

	// 左软键 + 通话键（绿）
	sf::CircleShape call(8.5f, 24);
	call.setFillColor(sf::Color(60, 170, 80));
	call.setOutlineColor(sf::Color(40, 130, 60));
	call.setOutlineThickness(1.2f);
	call.setPosition(22.f, nav_top + 14.f);
	win.draw(call);

	// 右软键 + 挂机键（红）
	sf::CircleShape end(8.5f, 24);
	end.setFillColor(sf::Color(200, 60, 60));
	end.setOutlineColor(sf::Color(160, 40, 40));
	end.setOutlineThickness(1.2f);
	end.setPosition(win_w - 22.f - 17.f, nav_top + 14.f);
	win.draw(end);

	// 中央 OK 圆盘（外面金属银大圈 + 内白圆 + 4 个方向三角 = 国产机经典 D-pad 造型）
	sf::CircleShape ok_ring(24.f, 48);
	ok_ring.setFillColor(sf::Color(200, 200, 208));
	ok_ring.setOutlineColor(sf::Color(90, 90, 100));
	ok_ring.setOutlineThickness(1.5f);
	ok_ring.setPosition(cx - 24.f, nav_top);
	win.draw(ok_ring);

	sf::CircleShape ok_inner(14.f, 36);
	ok_inner.setFillColor(sf::Color(230, 230, 238));
	ok_inner.setOutlineColor(sf::Color(130, 130, 140));
	ok_inner.setOutlineThickness(1.f);
	ok_inner.setPosition(cx - 14.f, nav_top + 10.f);
	win.draw(ok_inner);
	sf::Text okt;
	okt.setString("OK");
	okt.setFillColor(sf::Color(60, 60, 65));
	okt.setCharacterSize(10);
	okt.setPosition(cx - 9.f, nav_top + 16.f);
	win.draw(okt);

	// D-pad 四个方向小三角（点缀装饰，提示位置）
	auto tri = [&](float px, float py, float rotDeg, sf::Color c) {
		sf::ConvexShape t(3);
		t.setPoint(0, sf::Vector2f( 0.f, -5.f));
		t.setPoint(1, sf::Vector2f( 5.f,  4.f));
		t.setPoint(2, sf::Vector2f(-5.f,  4.f));
		t.setFillColor(c);
		t.setPosition(px, py);
		t.setRotation(rotDeg);
		win.draw(t);
	};
	sf::Color dk = sf::Color(110, 112, 120);
	tri(cx,         nav_top +  6.f, 0.f,   dk);  // ↑
	tri(cx + 22.f,  nav_top + 28.f, 90.f,  dk);  // →
	tri(cx,         nav_top + 50.f, 180.f, dk);  // ↓
	tri(cx - 22.f,  nav_top + 28.f, 270.f, dk);  // ←

	// ===== 5. 底部：扬声器 5 个小圆孔一排 =====
	float hole_y = win_h - 16.f;
	for (int i = 0; i < 5; ++i) {
		sf::CircleShape hole(1.6f, 14);
		hole.setFillColor(sf::Color(40, 40, 48));
		hole.setPosition(cx - 32.f + i * 16.f, hole_y);
		win.draw(hole);
	}
}

int main(int argc, char** argv) {
    std::string app_path = "";
    bool path_is_local = false;

	Log::set_module("Main");
	spdlog::set_level(spdlog::level::debug);

#ifndef ANDROID
	// cmd 控制台日志：简化 pattern，一行一个 [时间] [级别] 内容
	// 缺 API / warn / error 全在 cmd 里输出，不开额外 ImGui 日志面板
	// 用 spdlog 公共 API 全局设置 pattern（自动作用到 default logger 的所有 sink），
	// 避免直接访问 sinks 容器里 spdlog::sinks::sink 的内部成员 — 因为 common.h 里 sink 只有
	// 前向声明，直接 s->set_pattern() 需要完整定义，会造成 C2027/C2039。
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

	cli::Parser parser(argc, argv);
	{
		parser.set_optional<std::string>("", "", "", "Path to vxp");
		parser.set_optional<bool>("l", "path_is_local", false, "Set to run from local filesystem");
		parser.set_optional<bool>("g", "gdb", false, "Set to run gdb server");
		parser.set_optional<int>("p", "gdb_port", 1234, "Port for gdb server");
	}
	parser.run_and_exit_if_error();
	app_path = parser.get<std::string>("");
	path_is_local = parser.get<bool>("l");

	if (app_path.size())
		spdlog::info("Vxp path from args: {} {}", app_path, path_is_local ? "(local path)" : "");

	GDB::gdb_mode = parser.get<bool>("g");
	GDB::gdb_port = parser.get<int>("p");

	fs::current_path(fs::path(argv[0]).parent_path());
#else
    auto android_logger = spdlog::android_logger_mt("android_logger", "MREmu");

    spdlog::set_default_logger(android_logger);
#endif

    AppManager appManager;
    g_appManager = &appManager;

	if(GDB::gdb_mode)
		GDB::wait();

	Memory::init(32 * 1024 * 1024);
	Cpu::init();
	Bridge::init();

	MREngine::SIM::init();
	MREngine::System::init();
	MREngine::CharSet::init();
	MREngine::AppAudio::init();
	MREngine::Graphic graphic;

#ifndef ANDROID
	// 国产直板功能机造型：窄长机身，窗口大小 ≈ 手机壳本身大小
	// 布局：顶部 tiny 听筒 + 屏幕（占大头） + 中央功能行（通话 / OK 圆 / 挂机 + D-pad 装饰）+ 底部 4×3 数字键盘
	// 背景颜色 = 机身颜色（用户看不到黑底，整个画面就是一台手机）
	sf::RenderWindow win_device(sf::VideoMode(
		std::max<unsigned>(360, (unsigned)(graphic.width * 1.5f + 36)),   // 240*1.5 + 左右壳 18*2 = 宽 396
		std::max<unsigned>(780, (unsigned)(graphic.height * 2.f + 250))), // 320*2=640 屏 + 功能 80 + 数字 200 + 顶底 60 = 920 封顶
		"MREmu");

	// ImGui 绑定到 win_device（仅用来画数字键盘按钮 + VXP Error 弹窗）
	ImGui::SFML::Init(win_device);
	win_device.setFramerateLimit(60);
#else
	sf::RenderWindow win_device(sf::VideoMode::getDesktopMode(), "MREmu");
	win_device.setFramerateLimit(60);

	while (win_device.isOpen()) {
		sf::Event event;
		while (win_device.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				win_device.close();
			}
		}

		int perm_state = storage_permission_state.load();

		if (perm_state == 1)
			break;
		else if (perm_state == 0)
			win_device.clear(sf::Color::Black);
		else if (perm_state == -1)
			win_device.clear(sf::Color::Red);

		win_device.display();
	}
#endif

	MREngine::IO::init();

	if (GDB::gdb_mode)
		GDB::cpu_state = GDB::Stop;

	std::thread second_thread(mre_main, &appManager);

	Keyboard keyboard;
	Touch touch;

	if (app_path.size()) {
		if (fs::exists(app_path) || path_is_local) {
			appManager.add_app_for_launch(app_path, path_is_local);
		} else {
			error_message = "VXP file does not exist:\n" + app_path;
			show_error = true;
		}
	}
	else
		appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);


	int scale = 1;
	sf::Sprite screen_sp(graphic.screen_tex);
	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	auto update_screen_size = [&] {
		// 屏幕按整数比缩放，但最大 clamp 到 1.5（240×320 ×1.5 = 360×480）—
		// 屏幕不撑得巨大，手机整体保持窄直板。
		int scale_x = win_device.getSize().x / graphic.width;
		int scale_y = win_device.getSize().y / (graphic.height + graphic.height / 2);
		scale = std::min(scale_x, scale_y);
		if (scale < 1) scale = 1;
		if (scale > 2) scale = 2;   // 最大值：240×320 ×2 = 480×640，再大按键会丑

		screen_sp.setScale((float)scale, (float)scale);
		// 屏幕在手机中间稍微靠上（对应外壳的顶部小装饰）
		const float target_top_pad = 36.f;
		float tx = (float)(win_device.getSize().x - graphic.width * scale) / 2.f;
		float ty = std::max(target_top_pad, 20.f);
		screen_sp.setPosition(tx, ty);

		keyboard.update_resize((int)win_device.getSize().x, (int)win_device.getSize().y);
	};

	update_screen_size();

	sf::Clock fps;
	(void)fps;

	sf::Clock deltaClock;
	sf::Event event;

	while (win_device.isOpen()) {
		while (win_device.pollEvent(event)) {
#ifndef ANDROID
			ImGui::SFML::ProcessEvent(event);
#endif
			keyboard.event(event);
			touch.sf_event(event);
			switch (event.type) {
			case sf::Event::Closed:
				win_device.close();
				break;
            case sf::Event::Resized:
                win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				update_screen_size();
                break;
			}
		}

#ifndef ANDROID
		ImGui::SFML::Update(win_device, deltaClock.restart());

		// VXP Error Modal（只在启动 vxp 不存在等极少情况弹一次，其他任何 debug 信息都走 cmd）
		if (show_error) {
			ImGui::OpenPopup("VXP Error");
			show_error = false;
		}
		if (ImGui::BeginPopupModal("VXP Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("%s", error_message.c_str());
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
#endif

		graphic.update_screen();

		// ========== 设备窗口：手机外壳 + 屏幕 + 键盘（ImGui 画在同一个窗口上）==========
		win_device.clear(sf::Color(178, 180, 188));  // 机身银灰色 = 背景色，窗口就是整个手机壳
		sf::FloatRect screen_rect(screen_sp.getPosition().x,
								  screen_sp.getPosition().y,
								  screen_sp.getScale().x * graphic.width,
								  screen_sp.getScale().y * graphic.height);
#ifndef ANDROID
		draw_phone_frame(win_device, screen_rect);
#endif
		{
			screen_sp.setTexture(graphic.screen_tex, true);
			win_device.draw(screen_sp);
		}
#ifndef ANDROID
		keyboard.draw(&win_device);

		// ImGui 内容：仅 keyboard.draw() 内部用到的按钮 + 上面的 VXP Error 弹窗
		ImGui::SFML::Render(win_device);
#endif

		win_device.display();
	}

	work = false;
	second_thread.join();

#ifndef ANDROID
	ImGui::SFML::Shutdown();
#endif
	return 0;
}
