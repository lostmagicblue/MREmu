#pragma once

// 一个简易的 spdlog ringbuffer sink，用于在 ImGui 里展示运行日志。
// 用法：在 main() 开头调用 UiLogSink::install()，之后的 spdlog::info/warn/error
//       都会写入固定大小的环形缓冲区，并带时间戳和日志级别颜色。
//
// 注意：直接继承 spdlog::sinks::sink（最基类），而不是 base_sink<Mutex>。
//       这样绕开了不同 spdlog 版本对 sink_it_/_sink_it/flush_/_flush 命名的差异，
//       保证从 0.x 到 1.x 的任意 spdlog 版本都能顺利编译链接。

#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>
#include <spdlog/sinks/sink.h>

struct UiLogEntry {
    uint64_t ts_ms = 0;        // ms since app start
    int level = 0;             // spdlog::level::level_enum 的整数
    std::string module;        // [Main] / [Bridge] ... 模块名
    std::string msg;           // 实际日志内容
};

class UiLogSink : public spdlog::sinks::sink {
public:
    static constexpr size_t MAX_LINES = 2000;

    // 调用一次即可：把一个 UiLogSink 加到 spdlog 的默认 logger。
    static void install();

    // 获取全部条目（返回拷贝），线程安全
    static std::vector<UiLogEntry> snapshot();

    // 清空历史
    static void clear();

    // —— spdlog::sinks::sink 接口 ——
    void log(const spdlog::details::log_msg& msg) override;
    void flush() override;

private:
    static std::mutex&             mtx();
    static std::deque<UiLogEntry>& ring();
    static UiLogSink*&             instance();
};
