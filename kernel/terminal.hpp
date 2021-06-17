/// ターミナルウィンドウ

#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>

#include "fat.hpp"
#include "layer.hpp"
#include "paging.hpp"
#include "task.hpp"
#include "window.hpp"

/// アプリをロードした後の状態
struct AppLoadInfo {
    /// アプリのLOADセグメントの最終アドレス
    /// デマンドページングのアドレス範囲開始点として使用
    uint64_t vaddr_end;
    /// アプリのエントリポイントのアドレス
    uint64_t entry;
    /// アプリ固有の階層ページング構造
    PageMapEntry* pml4;
};

/// ロード済みアプリの一覧
extern std::map<fat::DirectoryEntry*, AppLoadInfo>* g_app_loads;

class Terminal {
public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal(Task& task, bool show_window);
    unsigned int LayerID() const { return layer_id_; }
    /// カーソルの点滅を切り替え、カーソルの描画領域を返す
    Rectangle<int> BlinkCursor();
    // キー入力を受付け、再描画すべき範囲を返す
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char* s, std::optional<size_t> len = std::nullopt);
    Task& UnderlyingTask() const { return task_; }

private:
    std::shared_ptr<TopLevelWindow> window_;
    unsigned int layer_id_;
    Task& task_;
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
    /// 標準入出力ファイル
    std::array<std::shared_ptr<IFileDescriptor>, 3> files_;
    /// 直前のアプリの終了コード
    int last_exit_code_{0};

    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;
    /// 1行だけスクロール
    void Scroll1();
    /// コマンド実行
    void ExecuteLine();
    /// 実行可能ファイル（カーネル本体に組み込まれていないアプリ）を読み込んで実行
    /// return : アプリの終了コード
    WithError<int> ExecuteFile(fat::DirectoryEntry& file_entry, char* command, char* first_arg);
    void Print(char32_t c);
    /// コマンド履歴を辿る
    Rectangle<int> HistoryUpDown(int direction);
};

void TaskTerminal(uint64_t task_id, int64_t data);

/// キーボードをファイルに見せかける
class TerminalFileDescriptor : public IFileDescriptor {
public:
    explicit TerminalFileDescriptor(Terminal& term);
    /// キーボード入力から1文字だけ読み取る（標準入力）
    size_t Read(void* buf, size_t len) override;
    /// ターミナルに出力する（標準出力）
    size_t Write(const void* buf, size_t len) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t len, size_t offset) override;

private:
    Terminal& term_;
};
