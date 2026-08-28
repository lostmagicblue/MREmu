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
	// 外壳尺寸：屏幕四周留边
	float body_pad_x = 40.f;
	float body_pad_top = 70.f;
	float body_pad_bot = 180.f;

	float body_x = screen_rect.left - body_pad_x;
	float body_y = screen_rect.top  - body_pad_top;
	float body_w = screen_rect.width  + body_pad_x * 2.f;
	float body_h = screen_rect.height + body_pad_top + body_pad_bot;

	// 1. 主体圆角矩形（深色）
	const float r = 34.f;
	sf::RectangleShape body(sf::Vector2f(body_w, body_h));
	body.setPosition(body_x, body_y);
	body.setFillColor(sf::Color(30, 30, 36));
	body.setOutlineColor(sf::Color(80, 80, 90));
	body.setOutlineThickness(2.f);
	// SFML RectangleShape 没有圆角，我们自己用四个角落的圆 + 主体近似
	win.draw(body);
	// 4 个角落的黑色圆角遮盖（让它看起来更圆滑）
	auto drawCorner = [&](float cx, float cy) {
		sf::CircleShape c(r, 32);
		c.setFillColor(sf::Color(30, 30, 36));
		c.setOutlineColor(sf::Color(80, 80, 90));
		c.setOutlineThickness(2.f);
		c.setPosition(cx - r, cy - r);
		win.draw(c);
	};
	drawCorner(body_x,              body_y);
	drawCorner(body_x + body_w,     body_y);
	drawCorner(body_x,              body_y + body_h);
	drawCorner(body_x + body_w,     body_y + body_h);
	// 切出一个屏幕矩形（在屏幕四周画黑边）
	sf::RectangleShape screen_frame(sf::Vector2f(screen_rect.width + 6.f, screen_rect.height + 6.f));
	screen_frame.setPosition(screen_rect.left - 3.f, screen_rect.top - 3.f);
	screen_frame.setFillColor(sf::Color::Black);
	screen_frame.setOutlineColor(sf::Color(110, 110, 120));
	screen_frame.setOutlineThickness(1.f);
	win.draw(screen_frame);

	// 2. 顶部听筒槽（横向椭圆）
	sf::CircleShape ear(7.f, 36);
	ear.setScale(3.f, 0.8f);
	ear.setFillColor(sf::Color(15, 15, 20));
	ear.setOutlineColor(sf::Color(70, 70, 80));
	ear.setOutlineThickness(1.f);
	ear.setPosition(body_x + body_w / 2.f - ear.getScale().x * 7.f,
		            body_y + body_pad_top / 2.f - ear.getScale().y * 7.f);
	win.draw(ear);

	// 3. 前置摄像头小圆圈
	sf::CircleShape cam(5.f, 30);
	cam.setFillColor(sf::Color(15, 15, 20));
	cam.setOutlineColor(sf::Color(80, 80, 90));
	cam.setOutlineThickness(1.2f);
	cam.setPosition(body_x + body_w - 80.f, body_y + body_pad_top / 2.f - 5.f);
	win.draw(cam);

	// 4. "MREmu" 标签在顶部
	sf::Text brand;
	// 不指定字体，使用 SFML 默认，找不到会不画
	brand.setString("MREmu  v0.1");
	brand.setFillColor(sf::Color(180, 180, 190));
	brand.setCharacterSize(14);
	brand.setPosition(body_x + 20.f, body_y + 18.f);
	win.draw(brand);

	// 5. 屏幕下方的 Android 三大键图标（只是装饰，按键输入还是靠 KeyboardControl）
	float soft_y = screen_rect.top + screen_rect.height + 40.f;
	sf::Font* sans = nullptr; // 不画中文
	(void)sans;

	// back ◀
	sf::ConvexShape tri;
	tri.setPointCount(3);
	tri.setPoint(0, sf::Vector2f( 0.f, 14.f));
	tri.setPoint(1, sf::Vector2f(14.f, 0.f));
	tri.setPoint(2, sf::Vector2f(14.f, 28.f));
	tri.setFillColor(sf::Color(210, 210, 220));
	tri.setPosition(body_x + body_w / 2.f - 100.f - 7.f, soft_y);
	win.draw(tri);

	// home ●
	sf::CircleShape home(13.f, 40);
	home.setFillColor(sf::Color::Transparent);
	home.setOutlineColor(sf::Color(210, 210, 220));
	home.setOutlineThickness(2.f);
	home.setPosition(body_x + body_w / 2.f - 13.f, soft_y);
	win.draw(home);

	// menu/recents ▢
	sf::RectangleShape rec(sf::Vector2f(22.f, 22.f));
	rec.setFillColor(sf::Color::Transparent);
	rec.setOutlineColor(sf::Color(210, 210, 220));
	rec.setOutlineThickness(2.f);
	rec.setPosition(body_x + body_w / 2.f + 80.f, soft_y + 2.f);
	win.draw(rec);

	// 6. 底部扬声器 5 个小孔
	float hole_y = body_y + body_h - 30.f;
	for (int i = 0; i < 5; ++i) {
		sf::CircleShape hole(2.5f, 16);
		hole.setFillColor(sf::Color(15, 15, 20));
		hole.setPosition(body_x + body_w / 2.f - 40.f + i * 18.f, hole_y);
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
	{
		auto& sinks = spdlog::default_logger()->sinks();
		for (auto& s : sinks)
			s->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
	}

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
	// 单个手机造型窗口（足够放下 240x320 屏 + 键盘），不再开 Debug & Log 大窗口
	// （日志输出全部走 cmd 控制台，简单直观）
	sf::RenderWindow win_device(sf::VideoMode(
		std::max<unsigned>(560, (unsigned)(graphic.width * 2 + 120)),
		std::max<unsigned>(860, (unsigned)(graphic.height * 2 + 250))),
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
		int scale_x = win_device.getSize().x / graphic.width;
		int scale_y = win_device.getSize().y / (graphic.height + graphic.height / 2);

		scale = std::min(scale_x, scale_y);
		if (scale < 1)
			scale = 1;

		screen_sp.setScale(scale, scale);
		// 屏幕在手机中间稍微靠上（对应外壳的 top pad）
		float target_top_pad = 70.f;
		float tx = (win_device.getSize().x - graphic.width * scale) / 2.f;
		float ty = std::max(target_top_pad, 20.f);
		screen_sp.setPosition(tx, ty);

		keyboard.update_resize(win_device.getSize().x, win_device.getSize().y);
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
		win_device.clear(sf::Color(18, 18, 22));
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
