#include "UiLogSink.h"
#include <spdlog/spdlog.h>
#include <spdlog/details/log_msg.h>
#include <SFML/System.hpp>  // sf::Clock

namespace {
    static sf::Clock* get_start_clock() {
        static sf::Clock s_clock; // 首次构造即开始计时
        return &s_clock;
    }
    struct InitClock { InitClock() { (void)get_start_clock(); } };
    static InitClock s_init;
}

std::mutex&             UiLogSink::mtx()       { static std::mutex m; return m; }
std::deque<UiLogEntry>& UiLogSink::ring()      { static std::deque<UiLogEntry> r; return r; }
UiLogSink*&             UiLogSink::instance()  { static UiLogSink* p = nullptr; return p; }

void UiLogSink::install() {
    std::lock_guard<std::mutex> lk(mtx());
    if (instance()) return; // 已经装过了
    auto sink = std::make_shared<UiLogSink>();
    instance() = sink.get();
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

// spdlog::sinks::sink 接口：真正写日志
// 直接继承最基类 sink，log() / flush() 两个函数名在所有 spdlog 版本里都稳定
void UiLogSink::log(const spdlog::details::log_msg& msg) {
    UiLogEntry e;
    e.ts_ms = (uint64_t)get_start_clock()->getElapsedTime().asMilliseconds();
    e.level = (int)msg.level;

    // payload 一般形如: "[模块名] [级别] 实际消息"
    // 也可能: "[模块名] 实际消息"
    std::string text(msg.payload.data(), msg.payload.size());

    if (!text.empty() && text.front() == '[') {
        size_t end1 = text.find(']');
        if (end1 != std::string::npos) {
            e.module = text.substr(1, end1 - 1);
            size_t p = end1 + 1;
            while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
            if (p < text.size() && text[p] == '[') {
                size_t end2 = text.find(']', p);
                if (end2 != std::string::npos) {
                    p = end2 + 1;
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

    while (!e.msg.empty() && (e.msg.back() == '\n' || e.msg.back() == '\r'))
        e.msg.pop_back();

    std::lock_guard<std::mutex> lk(mtx());
    auto& r = ring();
    r.push_back(std::move(e));
    while (r.size() > MAX_LINES) r.pop_front();
}

// 环形缓存不需要 flush，空实现即可
void UiLogSink::flush() {}
