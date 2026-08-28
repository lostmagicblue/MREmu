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
	// —— 仿诺基亚/国产直板功能机视觉规范（参数化，单点改动）——
	const float R_CORNER    = 20.f;    // 机身边缘圆角（小，精致感）
	const float BODY_PAD_X  = screen_rect.left;           // 屏幕左边距
	const float BODY_PAD_TOP= screen_rect.top / 2.f;      // 顶栏品牌字高度的一半基准

	float win_w = (float)win.getSize().x;
	float win_h = (float)win.getSize().y;
	float cx    = win_w / 2.f;

	// =================================================================
	// 1. 机身：银白主色 + 深灰边 + 小圆角（不是大圆弧，真机长这样）
	// =================================================================
	sf::RectangleShape body(sf::Vector2f(win_w, win_h));
	body.setPosition(0.f, 0.f);
	body.setFillColor(sf::Color(238, 240, 245));   // 银白机身
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
	drawCorner(0.f,   0.f);
	drawCorner(win_w, 0.f);
	drawCorner(0.f,   win_h);
	drawCorner(win_w, win_h);

	// =================================================================
	// 2. 顶部：小品牌字（类似 NOKIA 在顶左） + 听筒（顶中偏右） + 前摄（右上）
	// =================================================================
	sf::Text brand;
	brand.setString("MREmu");
	brand.setFillColor(sf::Color(60, 60, 70));
	brand.setCharacterSize(12);
	brand.setPosition(14.f, BODY_PAD_TOP - 4.f);
	win.draw(brand);

	sf::CircleShape ear(4.f, 28);
	ear.setScale(2.8f, 0.5f);
	ear.setFillColor(sf::Color(50, 50, 58));
	ear.setOutlineColor(sf::Color(100, 100, 110));
	ear.setOutlineThickness(0.6f);
	ear.setPosition(cx - ear.getScale().x * 4.f + 18.f, BODY_PAD_TOP - 4.f * ear.getScale().y);
	win.draw(ear);

	sf::CircleShape cam(1.8f, 14);
	cam.setFillColor(sf::Color(30, 30, 40));
	cam.setPosition(win_w - 22.f, BODY_PAD_TOP - 1.8f);
	win.draw(cam);

	// =================================================================
	// 3. 屏幕：双层边框（外层亮银金属边 + 内层黑边），像真机屏幕凹进去
	// =================================================================
	sf::RectangleShape frame_outer(sf::Vector2f(screen_rect.width + 12.f, screen_rect.height + 12.f));
	frame_outer.setPosition(screen_rect.left - 6.f, screen_rect.top - 6.f);
	frame_outer.setFillColor(sf::Color(200, 204, 212));
	frame_outer.setOutlineColor(sf::Color(120, 122, 130));
	frame_outer.setOutlineThickness(1.f);
	win.draw(frame_outer);

	sf::RectangleShape frame_inner(sf::Vector2f(screen_rect.width + 4.f, screen_rect.height + 4.f));
	frame_inner.setPosition(screen_rect.left - 2.f, screen_rect.top - 2.f);
	frame_inner.setFillColor(sf::Color::Black);
	win.draw(frame_inner);

	// =================================================================
	// 4. 屏幕下方：一行软键标签文字（「选项」「功能表」「电话簿」）
	// =================================================================
	float soft_y = screen_rect.top + screen_rect.height + 10.f;
	sf::Text soft_l, soft_c, soft_r;
	soft_l.setString(u8"选项");
	soft_c.setString(u8"功能表");
	soft_r.setString(u8"电话簿");
	soft_l.setCharacterSize(12);
	soft_c.setCharacterSize(12);
	soft_r.setCharacterSize(12);
	soft_l.setFillColor(sf::Color(60, 60, 70));
	soft_c.setFillColor(sf::Color(60, 60, 70));
	soft_r.setFillColor(sf::Color(60, 60, 70));
	soft_l.setPosition(18.f,                     soft_y);
	soft_r.setPosition(win_w - 64.f,             soft_y);
	soft_c.setPosition(cx - soft_c.getLocalBounds().width / 2.f, soft_y);
	win.draw(soft_l);
	win.draw(soft_c);
	win.draw(soft_r);

	// =================================================================
	// 5. D-pad + OK 主按键（中央白色圆角方形 OK + 四向凸起块）
	// =================================================================
	float dpad_top = soft_y + 28.f;
	const float OK_SIZE = 46.f;   // 中间 OK 方块大小
	const float ARM_H   = 30.f;   // 上下/左右凸起条尺寸

	// —— 上下左右 4 个凸起条（浅灰，像按下去的方向键）
	sf::Color arm = sf::Color(216, 220, 228);
	sf::Color armLine = sf::Color(160, 162, 170);

	sf::RectangleShape arm_up(sf::Vector2f(ARM_H, ARM_H * 0.55f));
	arm_up.setFillColor(arm);
	arm_up.setOutlineColor(armLine);
	arm_up.setOutlineThickness(1.f);
	arm_up.setPosition(cx - ARM_H / 2.f, dpad_top);
	win.draw(arm_up);

	sf::RectangleShape arm_dn(sf::Vector2f(ARM_H, ARM_H * 0.55f));
	arm_dn.setFillColor(arm);
	arm_dn.setOutlineColor(armLine);
	arm_dn.setOutlineThickness(1.f);
	arm_dn.setPosition(cx - ARM_H / 2.f, dpad_top + OK_SIZE - ARM_H * 0.55f + 10.f);
	win.draw(arm_dn);

	sf::RectangleShape arm_lf(sf::Vector2f(ARM_H * 0.55f, ARM_H));
	arm_lf.setFillColor(arm);
	arm_lf.setOutlineColor(armLine);
	arm_lf.setOutlineThickness(1.f);
	arm_lf.setPosition(cx - OK_SIZE / 2.f - ARM_H * 0.55f + 2.f, dpad_top + (OK_SIZE - ARM_H) / 2.f + 5.f);
	win.draw(arm_lf);

	sf::RectangleShape arm_rt(sf::Vector2f(ARM_H * 0.55f, ARM_H));
	arm_rt.setFillColor(arm);
	arm_rt.setOutlineColor(armLine);
	arm_rt.setOutlineThickness(1.f);
	arm_rt.setPosition(cx + OK_SIZE / 2.f - 2.f, dpad_top + (OK_SIZE - ARM_H) / 2.f + 5.f);
	win.draw(arm_rt);

	// —— 中央白色圆角方形 OK 键（最上层盖住四条凸起）
	sf::RectangleShape ok_btn(sf::Vector2f(OK_SIZE, OK_SIZE));
	// 画圆角方形：用 rect + 4 个小圆角覆盖四角
	ok_btn.setPosition(cx - OK_SIZE / 2.f, dpad_top + (OK_SIZE - OK_SIZE) / 2.f + 5.f);
	ok_btn.setFillColor(sf::Color(248, 248, 252));
	ok_btn.setOutlineColor(sf::Color(140, 142, 150));
	ok_btn.setOutlineThickness(1.f);
	win.draw(ok_btn);

	float ok_x = ok_btn.getPosition().x;
	float ok_y = ok_btn.getPosition().y;
	{
		float rc = 8.f;
		sf::CircleShape cc(rc, 22);
		cc.setFillColor(sf::Color(248, 248, 252));
		cc.setOutlineColor(sf::Color(140, 142, 150));
		cc.setOutlineThickness(1.f);
		auto okC = [&](float a, float b) {
			cc.setPosition(a - rc, b - rc);
			win.draw(cc);
		};
		okC(ok_x,            ok_y);
		okC(ok_x + OK_SIZE,   ok_y);
		okC(ok_x,            ok_y + OK_SIZE);
		okC(ok_x + OK_SIZE,   ok_y + OK_SIZE);
	}

	// —— OK 文字
	sf::Text okt;
	okt.setString("OK");
	okt.setFillColor(sf::Color(80, 80, 90));
	okt.setCharacterSize(14);
	okt.setPosition(cx - 12.f, ok_y + OK_SIZE / 2.f - 12.f);
	win.draw(okt);

	// =================================================================
	// 6. 通话（绿）/ 挂机（红）小方按钮，在 D-pad 左右两侧
	// =================================================================
	const float KB_SIZE = 22.f;
	sf::RectangleShape call(sf::Vector2f(KB_SIZE + 4.f, KB_SIZE));
	call.setFillColor(sf::Color(80, 180, 90));
	call.setOutlineColor(sf::Color(60, 140, 70));
	call.setOutlineThickness(1.2f);
	call.setPosition(16.f, dpad_top + (OK_SIZE - KB_SIZE) / 2.f + 5.f);
	win.draw(call);

	sf::RectangleShape end_b(sf::Vector2f(KB_SIZE + 4.f, KB_SIZE));
	end_b.setFillColor(sf::Color(210, 70, 70));
	end_b.setOutlineColor(sf::Color(170, 50, 50));
	end_b.setOutlineThickness(1.2f);
	end_b.setPosition(win_w - 16.f - KB_SIZE - 4.f, dpad_top + (OK_SIZE - KB_SIZE) / 2.f + 5.f);
	win.draw(end_b);

	// =================================================================
	// 7. 底部：扬声器一排 5 小孔
	// =================================================================
	float hole_y = win_h - 14.f;
	for (int i = 0; i < 5; ++i) {
		sf::CircleShape hole(1.4f, 12);
		hole.setFillColor(sf::Color(60, 60, 70));
		hole.setPosition(cx - 30.f + i * 15.f, hole_y);
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
