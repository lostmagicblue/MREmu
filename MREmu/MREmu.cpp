#include <iostream>
#include <thread>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

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

    env->ReleaseStringUTFChars(j_path, path_cstr);
}
#endif

// Windows 底层硬件级异常过滤器 (如 Segment Fault / Access Violation)
#ifdef _WIN32
LONG WINAPI WindowsCrashFilter(EXCEPTION_POINTERS* pExceptionInfo) {
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    PVOID addr = pExceptionInfo->ExceptionRecord->ExceptionAddress;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "[CRASH CAUGHT] System Exception Detected!" << std::endl;
    std::cout << "  Exception Code: 0x" << std::hex << code << std::dec << std::endl;
    std::cout << "  Fault Address:  0x" << std::hex << (uintptr_t)addr << std::dec << std::endl;
    std::cout << "  Reason: MRE VXP code triggered invalid memory access / emulator fault." << std::endl;
    std::cout << "==================================================\n" << std::endl;

    // 尝试安全退出当前崩溃的 VXP 实例，拉起内置 AppSelector 菜单
    if (g_appManager) {
        try {
            g_appManager->add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);
        } catch (...) {}
    }

    // 拦截崩溃，阻止系统杀死控制台进程，让 CPU 模拟线程安全终止/重置
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

#ifdef _WIN32
        // 包裹 Windows 原生 SEH 结构化异常
        __try {
            GDB::update();
            appManager.update(delta_ms);
        }
        __except (WindowsCrashFilter(GetExceptionInformation())) {
            spdlog::error("[CRASH INTERCEPTED] Recovering core execution loop...");
        }
#else
		try {
			GDB::update();
			appManager.update(delta_ms);
		}
		catch (const std::exception& e) {
			spdlog::error("[CRASH PREVENTED] Exception: {}", e.what());
			std::cout << "[ERROR] Exception caught: " << e.what() << std::endl;
		}
		catch (...) {
			spdlog::error("[CRASH PREVENTED] Unknown crash in MRE core execution.");
		}
#endif

		sf::sleep(sf::milliseconds(1000 / 120));
	}
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // 在程序最顶层注册全局硬件崩溃捕获器，防止 CMD 被直接关闭
    SetUnhandledExceptionFilter(WindowsCrashFilter);
#endif

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

	sf::RenderWindow win_device(sf::VideoMode(graphic.width, graphic.height + 208), "MREmu Device");
	win_device.setFramerateLimit(60);

#ifdef ANDROID
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
			try {
				appManager.add_app_for_launch(app_path, path_is_local);
			} catch (...) {
				std::cout << "[ERROR] Exception launch VXP: " << app_path << std::endl;
				appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);
			}
		} else {
			std::cout << "[ERROR] VXP file does not exist: " << app_path << std::endl;
			appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);
		}
	}
	else {
		appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);
	}

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

	while (win_device.isOpen()) {
		while (win_device.pollEvent(event)) {
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

#ifdef _WIN32
        __try {
            graphic.update_screen();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::cout << "[ERROR] Screen draw memory error." << std::endl;
        }
#else
		try {
			graphic.update_screen();
		} catch (...) {}
#endif

		{
			screen_sp.setTexture(graphic.screen_tex, true);
			win_device.draw(screen_sp);
		}

		keyboard.draw(&win_device);

		win_device.display();
		win_device.clear(sf::Color::Black);
	}

	work = false;
	second_thread.join();

	return 0;
}
