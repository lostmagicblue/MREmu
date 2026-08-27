#include "UiLogSink.h"
#include <spdlog/logger.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <SFML/System.hpp>  // sf::Clock

namespace {
    static sf::Clock* get_start_clock() {
        static sf::Clock s_clock; // 首次构造即开始计时
        return &s_clock;
    }
    struct InitClock { InitClock() { (void)get_start_clock(); } };
    static InitClock s_init;
}

std::mutex&     UiLogSink::mtx()      { static std::mutex m; return m; }
std::deque<UiLogEntry>& UiLogSink::ring() { static std::deque<UiLogEntry> r; return r; }
UiLogSink*&     UiLogSink::instance() { static UiLogSink* p = nullptr; return p; }

void UiLogSink::install() {
    std::lock_guard<std::mutex> lk(mtx());
    if (instance()) return; // 已经装过了
    auto sink = std::make_shared<UiLogSink>();
    instance() = sink.get();

    // 格式化：我们自己保存 module/msg/time 字段，所以就用默认 formatter 再拼回来也行
    // 但为了简洁，我们在 sink_it_ 里用 raw 信息自己组装
    spdlog::default_logger()->sinks().push_back(sink);
}

std::vector<UiLogEntry> UiLogSink::snapshot() {
    std::lock_guard<std::mutex> lk(mtx());
    auto& r = ring();
    return std::vector<UiLogEntry>(r.begin(), r.end());
}

void UiLogSink::clear() {
    std::lock_guard<std::mutex> lk(mtx());
    ring().clear();
}

void UiLogSink::sink_it_(const spdlog::details::log_msg& msg) {
    // payload 是用户真正写的 msg（带模块名时是 [xx] [level] content）
    // spdlog::formatter 把 level、logger_name、时间戳填进 msg。
    // 为了简洁，我们只取 payload 原文和 level，再从 pattern 中的 [Module] 解析模块。
    spdlog::memory_buf_t formatted;
    spdlog::formatter_ptr formatter_ptr; // 不格式化，直接用 payload
    (void)formatter_ptr;

    UiLogEntry e;
    e.ts_ms  = (uint64_t)get_start_clock()->getElapsedTime().asMilliseconds();
    e.level  = (int)msg.level;

    // payload 里的开头有 [Mod]，我们手工拆
    fmt::string_view pay = msg.payload;
    std::string text(pay.data(), pay.size());

    // 模式: "[Main] [xxx] actual message" —— 取第一个方括号为模块
    if (!text.empty() && text[0] == '[') {
        size_t close1 = text.find(']');
        if (close1 != std::string::npos) {
            e.module = text.substr(1, close1 - 1);
            // 跳过 [Mod] 后面的空格
            size_t p = close1 + 1;
            while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
            // 再接一个 [Level]（可选）
            if (p < text.size() && text[p] == '[') {
                size_t close2 = text.find(']', p);
                if (close2 != std::string::npos) {
                    p = close2 + 1;
                    while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
                }
            }
            e.msg = text.substr(p);
        } else {
            e.msg = std::move(text);
        }
    } else {
        e.msg = std::move(text);
    }

    std::lock_guard<std::mutex> lk(mtx());
    auto& r = ring();
    r.push_back(std::move(e));
    while (r.size() > MAX_LINES) r.pop_front();
}
