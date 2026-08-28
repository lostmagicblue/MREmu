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
	// 真机直板造型：窄边，窄高比，不做任何"巨大装饰"
	// 屏幕左右边距 24px、上边距 36px（顶 tiny 装饰）、下边距 150px（留给通话键 + D-pad 装饰 + 数字键盘）
	const float body_pad_x   = 24.f;
	const float body_pad_top = 36.f;
	const float body_pad_bot = 150.f;

	float body_x = screen_rect.left - body_pad_x;
	float body_y = screen_rect.top  - body_pad_top;
	float body_w = screen_rect.width  + body_pad_x * 2.f;
	float body_h = screen_rect.height + body_pad_top + body_pad_bot;

	// ====== 1. 机身主体（银灰金属色，像第二张真机） ======
	const float r = 22.f;
	sf::RectangleShape body(sf::Vector2f(body_w, body_h));
	body.setPosition(body_x, body_y);
	body.setFillColor(sf::Color(178, 180, 188));     // 银灰机身
	body.setOutlineColor(sf::Color(110, 112, 120));   // 深灰边线
	body.setOutlineThickness(2.f);
	win.draw(body);
	// 四角做圆角遮盖
	auto drawCorner = [&](float cx, float cy) {
		sf::CircleShape c(r, 28);
		c.setFillColor(sf::Color(178, 180, 188));
		c.setOutlineColor(sf::Color(110, 112, 120));
		c.setOutlineThickness(2.f);
		c.setPosition(cx - r, cy - r);
		win.draw(c);
	};
	drawCorner(body_x,          body_y);
	drawCorner(body_x + body_w, body_y);
	drawCorner(body_x,          body_y + body_h);
	drawCorner(body_x + body_w, body_y + body_h);

	// ====== 2. 屏幕黑框（就真的一块屏，黑边很细） ======
	sf::RectangleShape screen_frame(sf::Vector2f(screen_rect.width + 4.f, screen_rect.height + 4.f));
	screen_frame.setPosition(screen_rect.left - 2.f, screen_rect.top - 2.f);
	screen_frame.setFillColor(sf::Color::Black);
	screen_frame.setOutlineColor(sf::Color(60, 60, 70));
	screen_frame.setOutlineThickness(1.f);
	win.draw(screen_frame);

	// ====== 3. 顶部：细长听筒槽 + 微型前摄（去掉大"品牌字"，真机很简洁） ======
	sf::CircleShape ear(5.f, 32);
	ear.setScale(3.4f, 0.6f);                       // 扁长听筒
	ear.setFillColor(sf::Color(40, 40, 48));
	ear.setOutlineColor(sf::Color(90, 90, 100));
	ear.setOutlineThickness(0.8f);
	ear.setPosition(
		body_x + body_w / 2.f - ear.getScale().x * 5.f,
		body_y + body_pad_top / 2.f - ear.getScale().y * 5.f);
	win.draw(ear);

	sf::CircleShape cam(2.8f, 20);                    // tiny 前摄
	cam.setFillColor(sf::Color(30, 30, 40));
	cam.setOutlineColor(sf::Color(100, 100, 110));
	cam.setOutlineThickness(0.8f);
	cam.setPosition(body_x + body_w - 42.f, body_y + body_pad_top / 2.f - 2.8f);
	win.draw(cam);

	// ====== 4. 屏幕下方：[左通话绿键] + [中央 OK 圆] + [右挂机红键] 装饰小行 ======
	float nav_y = screen_rect.top + screen_rect.height + 22.f;
	// 中央 OK 圆
	sf::CircleShape ok_circle(14.f, 40);
	ok_circle.setFillColor(sf::Color(230, 230, 235));
	ok_circle.setOutlineColor(sf::Color(90, 90, 100));
	ok_circle.setOutlineThickness(1.4f);
	ok_circle.setPosition(body_x + body_w / 2.f - 14.f, nav_y);
	win.draw(ok_circle);
	sf::Text ok_txt;
	ok_txt.setString("OK");
	ok_txt.setFillColor(sf::Color(60, 60, 65));
	ok_txt.setCharacterSize(11);
	ok_txt.setPosition(body_x + body_w / 2.f - 10.f, nav_y + 8.f);
	win.draw(ok_txt);

	// 左软键 + 绿色通话键
	sf::CircleShape call(9.f, 28);
	call.setFillColor(sf::Color(60, 170, 80));
	call.setOutlineColor(sf::Color(40, 130, 60));
	call.setOutlineThickness(1.2f);
	call.setPosition(body_x + 38.f, nav_y + 5.f);
	win.draw(call);

	// 右软键 + 红色挂机键
	sf::CircleShape end(9.f, 28);
	end.setFillColor(sf::Color(200, 60, 60));
	end.setOutlineColor(sf::Color(160, 40, 40));
	end.setOutlineThickness(1.2f);
	end.setPosition(body_x + body_w - 38.f - 18.f, nav_y + 5.f);
	win.draw(end);

	// ====== 5. 底部：3 个小圆扬声器孔，很小一排 ======
	float hole_y = body_y + body_h - 18.f;
	for (int i = 0; i < 3; ++i) {
		sf::CircleShape hole(1.8f, 14);
		hole.setFillColor(sf::Color(40, 40, 48));
		hole.setPosition(body_x + body_w / 2.f - 20.f + i * 18.f, hole_y);
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
	// 单个手机造型窗口（窄长直板，小而紧凑，不再 560×860 那么巨大）
	// 日志输出全部走 cmd 控制台，简单直观
	sf::RenderWindow win_device(sf::VideoMode(
		std::max<unsigned>(420, (unsigned)(graphic.width * 2 + 48)),
		std::max<unsigned>(820, (unsigned)(graphic.height * 2 + 200))),
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
