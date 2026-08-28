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

// 旧「手机外壳」装饰绘制已整体删除：现在窗口 = 屏幕 + 软键栏 + 键盘网格，
// 三块内容铺满整个窗口，没有任何机身背景/听筒/摄像头/通话挂机装饰。
// 软键栏（菜单/返回）的绘制在 Keyboard::draw_softbar()，几何常量在 Keyboard.h。

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
	// 窗口 = 内容本身，紧凑无留白：屏幕（满窗宽）→ 软键栏（菜单/返回）→ 6×3 网格键盘。
	// 几何常量在 Keyboard.h（SOFTBAR_H / GRID_NAV_ROWS / GRID_NUM_ROWS / GRID_KH），
	// MREmu.cpp 和 Keyboard.cpp 用同一份，窗口尺寸和内部布局永远对得上。
	sf::RenderWindow win_device(sf::VideoMode(
		(unsigned)(graphic.width * 1.5f),
		(unsigned)(graphic.height * 1.5f + SOFTBAR_H + (GRID_NAV_ROWS + GRID_NUM_ROWS) * GRID_KH)),
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


	sf::Sprite screen_sp(graphic.screen_tex);
	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	auto update_screen_size = [&] {
		// 屏幕永远占满窗口宽度、顶在 (0,0)：窗口多宽，屏幕就多宽（下面接软键栏和键盘）
		float sx = (float)win_device.getSize().x / (float)graphic.width;
		screen_sp.setScale(sx, sx);
		screen_sp.setPosition(0.f, 0.f);

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

		// ========== 设备窗口：屏幕 + 软键栏 + 键盘网格（三块铺满，ImGui 只用于错误弹窗）==========
		win_device.clear(sf::Color(33, 88, 148));  // 软键栏深蓝（窗口尺寸刚好铺满时几乎看不到）
		{
			screen_sp.setTexture(graphic.screen_tex, true);
			win_device.draw(screen_sp);
		}
#ifndef ANDROID
		keyboard.draw_softbar(&win_device);
		keyboard.draw(&win_device);

		// ImGui 内容：仅 VXP Error 弹窗
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
