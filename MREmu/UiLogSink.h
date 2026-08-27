#pragma once

// 一个简易的 spdlog ringbuffer sink，用于在 ImGui 里展示运行日志。
// 用法：在 main() 开头调用 UiLogSink::install()，之后的 spdlog::info/warn/error
//       都会写入固定大小的环形缓冲区，并带时间戳和日志级别颜色。

#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>
#include <spdlog/sinks/base_sink.h>

struct UiLogEntry {
    uint64_t ts_ms = 0;        // ms since app start (便于排序)
    int level = 0;             // spdlog::level::level_enum
    std::string module;        // [Main] / [Bridge] ... 模块名
    std::string msg;           // 日志内容
};

class UiLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    static constexpr size_t MAX_LINES = 2000;

    // 调用一次即可：把一个 UiLogSink 加到 spdlog 的默认 logger。
    static void install();

    // 获取全部条目（返回拷贝），线程安全
    static std::vector<UiLogEntry> snapshot();

    // 清空历史
    static void clear();

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    static std::mutex& mtx();
    static std::deque<UiLogEntry>& ring();
    // 用一个指针记录唯一安装的 sink，避免重复注册。
    static UiLogSink*& instance();
};
