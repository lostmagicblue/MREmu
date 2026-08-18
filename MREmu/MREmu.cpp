#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

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

namespace fs = std::filesystem;

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

    env->ReleaseStringUTFChars(j_path, path_cstr);
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

int main(int argc, char** argv) {
    std::string app_path = "";
    bool path_is_local = false;

	Log::set_module("Main");
	spdlog::set_level(spdlog::level::debug);

#ifndef ANDROID
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
	sf::RenderWindow win_debug(sf::VideoMode(340, 720), "MREmu Phone Container");
	sf::RenderWindow win_device(sf::VideoMode(graphic.width, graphic.height + 208), "MREmu Device");
	ImGui::SFML::Init(win_debug);
	win_debug.setFramerateLimit(60);
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
		screen_sp.setPosition((win_device.getSize().x - graphic.width * scale) / 2, 0);

		keyboard.update_resize(win_device.getSize().x, win_device.getSize().y);
	};

	update_screen_size();

	sf::Clock fps;
	sf::Clock deltaClock;
	sf::Event event;

	// 自动扫描本地 mre 文件夹中的 .vxp 文件
	std::vector<std::string> mre_files;
	auto rescan_mre_dir = [&]() {
		mre_files.clear();
		if (fs::exists("mre") && fs::is_directory("mre")) {
			for (const auto& entry : fs::directory_iterator("mre")) {
				if (entry.path().extension() == ".vxp") {
					mre_files.push_back(entry.path().string());
				}
			}
		}
	};
	rescan_mre_dir();

	while (win_device.isOpen()
#ifndef ANDROID
        && win_debug.isOpen()
#endif
    ) {
#ifndef ANDROID
		while (win_debug.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);
			switch (event.type) {
			case sf::Event::Closed:
				win_debug.close();
				win_device.close();
				break;
			case sf::Event::Resized:
				win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				break;
			}
		}
#endif

		while (win_device.pollEvent(event)) {
			keyboard.event(event);
			touch.sf_event(event);
			switch (event.type) {
			case sf::Event::Closed:
				win_device.close();
#ifndef ANDROID
				win_debug.close();
#endif
				break;
            case sf::Event::Resized:
                win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				update_screen_size();
                break;
			}
		}

#ifndef ANDROID
		ImGui::SFML::Update(win_debug, deltaClock.restart());

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

#ifndef ANDROID
		// ------------------ 美化后的单窗口整合 UI ------------------
		ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(330, 710), ImGuiCond_Always);

		ImGui::Begin("MRE Feature Phone", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

		// 1. 文件读取控制区
		if (ImGui::CollapsingHeader("MRE Storage (mre/)", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (mre_files.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No .vxp files found in 'mre/'");
				if (ImGui::Button("Rescan Folder")) {
					rescan_mre_dir();
				}
			} else {
				static int selected_idx = 0;
				std::vector<const char*> items;
				for (const auto& file : mre_files) items.push_back(file.c_str());

				ImGui::PushItemWidth(200);
				ImGui::Combo("##vxp_select", &selected_idx, items.data(), items.size());
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Launch")) {
					appManager.add_app_for_launch(mre_files[selected_idx], true);
				}
			}
		}

		ImGui::Separator();

		// 2. 屏幕预览区
		ImGui::Text("Display (240x320)");
		graphic.imgui_screen();

		ImGui::Separator();

		// 3. 可折叠按键区
		static bool show_keypad = true;
		ImGui::Checkbox("Show Keypad", &show_keypad);

		if (show_keypad) {
			keyboard.imgui_keyboard();
		}

		ImGui::End();
#endif

		{
			screen_sp.setTexture(graphic.screen_tex, true);
			win_device.draw(screen_sp);
		}

		keyboard.draw(&win_device);

#ifndef ANDROID
		ImGui::SFML::Render(win_debug);
		win_debug.display();
		win_debug.clear(sf::Color(30, 30, 30));
#endif

		win_device.display();
		win_device.clear(sf::Color::Black);
	}

	work = false;
	second_thread.join();

#ifndef ANDROID
	ImGui::SFML::Shutdown();
#endif
	return 0;
}
