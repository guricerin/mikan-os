/// ターミナルウィンドウ

#pragma once

#include <deque>
#include <map>
#include <optional>

#include "fat.hpp"
#include "layer.hpp"
#include "task.hpp"
#include "window.hpp"

class Terminal {
public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal(uint64_t task_id, bool show_window);
    unsigned int LayerID() const { return layer_id_; }
    /// カーソルの点滅を切り替え、カーソルの描画領域を返す
    Rectangle<int> BlinkCursor();
    // キー入力を受付け、再描画すべき範囲を返す
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char* s, std::optional<size_t> len = std::nullopt);

private:
    std::shared_ptr<TopLevelWindow> window_;
    unsigned int layer_id_;
    uint64_t task_id_;
    /// カーソルの現在位置
    Vector2D<int> cursor_{0, 0};
    bool cursol_visible_{false};
    /// キー入力を1行分ためておくバッファ
    std::array<char, kLineMax> linebuf_{};
    int linebuf_index_{0};
    /// コマンド履歴
    /// 先頭は最新の履歴
    std::deque<std::array<char, kLineMax>> cmd_history_{};
    /// -1は履歴を辿っていない状態
    int cmd_history_index_{-1};
    /// ターミナルウィンドウの表示/非表示
    bool show_window_;

    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;
    /// 1行だけスクロール
    void Scroll1();
    /// コマンド実行
    void ExecuteLine();
    /// 実行可能ファイル（カーネル本体に組み込まれていないアプリ）を読み込んで実行
    Error ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg);
    void Print(char c);
    /// コマンド履歴を辿る
    Rectangle<int> HistoryUpDown(int direction);
};

/// タスクIDとターミナルの対応表
extern std::map<uint64_t, Terminal*>* g_terminals;

void TaskTerminal(uint64_t task_id, int64_t data);

/// キーボードをファイルに見せかける（標準入力）
class TerminalFileDescriptor : public IFileDescriptor {
public:
    explicit TerminalFileDescriptor(Task& task, Terminal& term);
    /// キーボード入力から1文字だけ読み取る
    size_t Read(void* buf, size_t len) override;

private:
    Task& task_;
    Terminal& term_;
};
