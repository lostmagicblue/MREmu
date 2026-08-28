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

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

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

// 只负责画外壳背景，完全不画按键！按键由 Keyboard 完整负责
static void draw_phone_frame(sf::RenderWindow& win, const sf::FloatRect& screen_rect) {
    const float R_CORNER = 18.f;
    float win_w = (float)win.getSize().x;
    float win_h = (float)win.getSize().y;
    float cx = win_w / 2.f;

    sf::RectangleShape body(sf::Vector2f(win_w, win_h));
    body.setPosition(0.f, 0.f);
    body.setFillColor(sf::Color(238, 240, 245));
    body.setOutlineColor(sf::Color(140, 142, 150));
    body.setOutlineThickness(2.f);
    win.draw(body);

    auto drawCorner = [&](float cx2, float cy2) {
        sf::CircleShape c(R_CORNER, 32);
        c.setFillColor(sf::Color(238, 240, 245));
        c.setOutlineColor(sf::Color(140, 142, 150));
        c.setOutlineThickness(2.f);
        c.setPosition(cx2 - R_CORNER, cy2 - R_CORNER);
        win.draw(c);
    };
    drawCorner(0.f, 0.f); drawCorner(win_w, 0.f);
    drawCorner(0.f, win_h); drawCorner(win_w, win_h);

    // 顶部品牌和听筒
    auto tex_brand = u16text_to_texture(u"MREmu", sf::Color(60, 60, 70));
    sf::Sprite sp_brand(tex_brand);
    sp_brand.setPosition(14.f, screen_rect.top / 2.f - 4.f);
    win.draw(sp_brand);

    sf::CircleShape ear(3.f, 28);
    ear.setScale(2.5f, 0.5f);
    ear.setFillColor(sf::Color(50, 50, 58));
    ear.setPosition(cx - ear.getScale().x * 3.f + 14.f, screen_rect.top / 2.f - 3.f * ear.getScale().y);
    win.draw(ear);

    // 屏幕框
    sf::RectangleShape frame_outer(sf::Vector2f(screen_rect.width + 10.f, screen_rect.height + 10.f));
    frame_outer.setPosition(screen_rect.left - 5.f, screen_rect.top - 5.f);
    frame_outer.setFillColor(sf::Color(200, 204, 212));
    win.draw(frame_outer);

    sf::RectangleShape frame_inner(sf::Vector2f(screen_rect.width + 2.f, screen_rect.height + 2.f));
    frame_inner.setPosition(screen_rect.left - 1.f, screen_rect.top - 1.f);
    frame_inner.setFillColor(sf::Color::Black);
    win.draw(frame_inner);

    // 软键标签
    float soft_y = screen_rect.top + screen_rect.height + 8.f;
    auto tex_l = u16text_to_texture(u"选项", sf::Color(60, 60, 70));
    auto tex_c = u16text_to_texture(u"功能表", sf::Color(60, 60, 70));
    auto tex_r = u16text_to_texture(u"电话簿", sf::Color(60, 60, 70));
    sf::Sprite sp_l(tex_l), sp_c(tex_c), sp_r(tex_r);
    sp_l.setPosition(18.f, soft_y);
    sp_r.setPosition(win_w - (float)tex_r.getSize().x - 14.f, soft_y);
    sp_c.setPosition(cx - (float)tex_c.getSize().x / 2.f, soft_y);
    win.draw(sp_l); win.draw(sp_c); win.draw(sp_r);
}

int main(int argc, char** argv) {
    std::string app_path = "";
    bool path_is_local = false;
    Log::set_module("Main");
    spdlog::set_level(spdlog::level::debug);

#ifndef ANDROID
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    cli::Parser parser(argc, argv);
    parser.set_optional<std::string>("", "", "", "Path to vxp");
    parser.set_optional<bool>("l", "path_is_local", false, "Set to run from local filesystem");
    parser.set_optional<bool>("g", "gdb", false, "Set to run gdb server");
    parser.set_optional<int>("p", "gdb_port", 1234, "Port for gdb server");
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
    if(GDB::gdb_mode) GDB::wait();

    Memory::init(32 * 1024 * 1024);
    Cpu::init();
    Bridge::init();
    MREngine::SIM::init();
    MREngine::System::init();
    MREngine::CharSet::init();
    MREngine::AppAudio::init();
    MREngine::Graphic graphic;

#ifndef ANDROID
    sf::RenderWindow win_device(sf::VideoMode(
        std::max<unsigned>(360, (unsigned)(graphic.width * 1.5f + 30)),
        std::max<unsigned>(700, (unsigned)(graphic.height * 1.5f + 260))),
        "MREmu");
    ImGui::SFML::Init(win_device);
    win_device.setFramerateLimit(60);
#else
    sf::RenderWindow win_device(sf::VideoMode::getDesktopMode(), "MREmu");
    win_device.setFramerateLimit(60);
    while (win_device.isOpen()) {
        sf::Event event;
        while (win_device.pollEvent(event)) {
            if (event.type == sf::Event::Closed) win_device.close();
        }
        int perm_state = storage_permission_state.load();
        if (perm_state == 1) break;
        else if (perm_state == 0) win_device.clear(sf::Color::Black);
        else if (perm_state == -1) win_device.clear(sf::Color::Red);
        win_device.display();
    }
#endif

    MREngine::IO::init();
    if (GDB::gdb_mode) GDB::cpu_state = GDB::Stop;
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
    } else
        appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);

    int scale = 1;
    sf::Sprite screen_sp(graphic.screen_tex);
    touch.screen = &screen_sp;
    keyboard.screen = &screen_sp;

    auto update_screen_size = [&] {
        int scale_x = win_device.getSize().x / graphic.width;
        int scale_y = win_device.getSize().y / (graphic.height + graphic.height / 2);
        scale = std::min(scale_x, scale_y);
        if (scale < 1) scale = 1;
        if (scale > 2) scale = 2;

        screen_sp.setScale((float)scale, (float)scale);
        const float target_top_pad = 20.f;
        float tx = (float)(win_device.getSize().x - graphic.width * scale) / 2.f;
        float ty = std::max(target_top_pad, 20.f);
        screen_sp.setPosition(tx, ty);

        keyboard.update_resize((int)win_device.getSize().x, (int)win_device.getSize().y);
    };
    update_screen_size();

    sf::Clock deltaClock;
    sf::Event event;
    while (win_device.isOpen()) {
        while (win_device.pollEvent(event)) {
#ifndef ANDROID
            ImGui::SFML::ProcessEvent(event);
#endif
            keyboard.event(event);
            touch.sf_event(event);
            if (event.type == sf::Event::Closed) win_device.close();
            else if (event.type == sf::Event::Resized) {
                win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
                update_screen_size();
            }
        }

#ifndef ANDROID
        ImGui::SFML::Update(win_device, deltaClock.restart());
        if (show_error) {
            ImGui::OpenPopup("VXP Error");
            show_error = false;
        }
        if (ImGui::BeginPopupModal("VXP Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", error_message.c_str());
            if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
#endif

        graphic.update_screen();
        win_device.clear(sf::Color(178, 180, 188));
        sf::FloatRect screen_rect(screen_sp.getPosition().x, screen_sp.getPosition().y,
                                  screen_sp.getScale().x * graphic.width, screen_sp.getScale().y * graphic.height);
#ifndef ANDROID
        draw_phone_frame(win_device, screen_rect);
#endif
        {
            screen_sp.setTexture(graphic.screen_tex, true);
            win_device.draw(screen_sp);
        }
#ifndef ANDROID
        keyboard.draw(&win_device);
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
