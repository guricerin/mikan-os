#include "terminal.hpp"

#include <cstring>

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"

Terminal::Terminal() {
    window_ = std::make_shared<TopLevelWindow>(
        kColumns * 8 + 8 + TopLevelWindow::kMarginX,
        kRows * 16 + 8 + TopLevelWindow::kMarginY,
        g_screen_config.pixel_format,
        "MikanTerm");
    DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

    layer_id_ = g_layer_manager->NewLayer()
                    .SetWindow(window_)
                    .SetDraggable(true)
                    .ID();

    Print(">"); // プロンプト
    cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor() {
    cursol_visible_ = !cursol_visible_;
    DrawCursor(cursol_visible_);

    return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible) {
    const auto color = visible ? ToColor(0xffffff) : ToColor(0);
    FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii) {
    DrawCursor(false);

    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n') {              // enter key
        linebuf_[linebuf_index_] = 0; // null文字
        // コマンド履歴を更新
        if (linebuf_index_ > 0) {
            cmd_history_.pop_back();
            cmd_history_.push_front(linebuf_);
        }
        linebuf_index_ = 0;
        cmd_history_index_ = -1;
        cursor_.x = 0;
        if (cursor_.y < kRows - 1) {
            cursor_.y++;
        } else {
            // 最終行ならスクロール
            Scroll1();
        }
        ExecuteLine();
        Print(">"); // プロンプト
        draw_area.pos = TopLevelWindow::kTopLeftMargin;
        draw_area.size = window_->InnerSize();
    } else if (ascii == '\b') { // back key
        if (cursor_.x > 0) {
            // 1文字消してカーソルを左に戻す
            cursor_.x--;
            FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
            draw_area.pos = CalcCursorPos();

            if (linebuf_index_ > 0) {
                linebuf_index_--;
            }
        }
    } else if (ascii != 0) {
        if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
            linebuf_[linebuf_index_] = ascii;
            linebuf_index_++;
            WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
            cursor_.x++;
        }
    } else if (keycode == 0x51) { // down arrow
        draw_area = HistoryUpDown(-1);
    } else if (keycode == 0x52) { // up arrow
        draw_area = HistoryUpDown(1);
    }

    DrawCursor(true);
    return draw_area;
}

Vector2D<int> Terminal::CalcCursorPos() const {
    return TopLevelWindow::kTopLeftMargin + Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

void Terminal::Scroll1() {
    Rectangle<int> move_src{
        TopLevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
        {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move(TopLevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
    // 最終行を塗りつぶす
    FillRectangle(*window_->InnerWriter(), {4, 4 + 16 * cursor_.y}, {8 * kColumns, 16}, {0, 0, 0});
}

void Terminal::ExecuteLine() {
    char* command = &linebuf_[0];
    char* first_arg = strchr(&linebuf_[0], ' ');
    if (first_arg) {
        // コマンド名と引数をスペースで分離
        *first_arg = 0;
        first_arg++;
    }

    if (strcmp(command, "echo") == 0) {
        if (first_arg) {
            Print(first_arg);
        }
        Print("\n");
    } else if (strcmp(command, "clear") == 0) {
        FillRectangle(*window_->InnerWriter(), {4, 4}, {8 * kColumns, 16 * kRows}, {0, 0, 0});
        cursor_.y = 0;
    } else if (strcmp(command, "lspci") == 0) {
        char s[64];
        for (int i = 0; i < pci::g_num_device; i++) {
            const auto& device = pci::g_devices[i];
            auto vendor_id = pci::ReadVendorId(device.bus, device.device, device.function);
            sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
                    device.bus, device.device, device.function, vendor_id, device.header_type,
                    device.class_code.base, device.class_code.sub, device.class_code.interface);
            Print(s);
        }
    } else if (strcmp(command, "ls") == 0) {
        auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(fat::g_boot_volume_image->root_cluster);
        // ディレクトリエントリ数 / クラスタ
        auto entries_per_cluster = fat::g_boot_volume_image->bytes_per_sector / sizeof(fat::DirectoryEntry) * fat::g_boot_volume_image->sectors_per_cluster;
        char base[9], ext[4];
        char s[64];
        for (int i = 0; i < entries_per_cluster; i++) {
            fat::ReadName(root_dir_entries[i], base, ext);
            if (base[0] == 0x00) { // ディレクトリエントリが空で、これより後ろに有効なエントリが存在しない
                break;
            } else if (static_cast<uint8_t>(base[0]) == 0xe5) { // ディレクトリエントリが空
                continue;
            } else if (root_dir_entries[i].attr == fat::Attribute::kLongName) { // エントリの構造がDirectoryEntryとは異なるので無視
                continue;
            }

            if (ext[0]) {
                sprintf(s, "%s.%s\n", base, ext);
            } else {
                sprintf(s, "%s\n", base);
            }
            Print(s);
        }
    } else if (strcmp(command, "cat") == 0) {
        char s[64];

        // ルートから検索
        auto file_entry = fat::FindFile(first_arg);
        if (!file_entry) {
            sprintf(s, "no such file: %s\n", first_arg);
            Print(s);
        } else {
            auto cluster = file_entry->FirstCluster();
            auto remain_bytes = file_entry->file_size;

            DrawCursor(false);
            while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
                char* p = fat::GetSectorByCluster<char>(cluster);

                int i = 0;
                for (; i < fat::g_bytes_per_cluster && i < remain_bytes; i++) {
                    Print(*p);
                    p++;
                }
                remain_bytes -= i;
                // ファイルが複数クラスタにまたがっていても対応
                cluster = fat::NextCluster(cluster);
            }
            DrawCursor(true);
        }
    } else if (command[0] != 0) {
        auto file_entry = fat::FindFile(command);
        if (!file_entry) {
            Print("no such command: ");
            Print(command);
            Print("\n");
        } else {
            ExecuteFile(*file_entry);
        }
    }
}

void Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry) {
    auto cluster = file_entry.FirstCluster();
    auto remain_bytes = file_entry.file_size;

    std::vector<uint8_t> file_buf(remain_bytes);
    auto p = &file_buf[0];

    while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        const auto copy_bytes = fat::g_bytes_per_cluster < remain_bytes ? fat::g_bytes_per_cluster : remain_bytes;
        memcpy(p, fat::GetSectorByCluster<uint8_t>(cluster), copy_bytes);

        remain_bytes -= copy_bytes;
        p += copy_bytes;
        // 複数クラスタにまたがる場合にも対応
        cluster = fat::NextCluster(cluster);
    }

    using Func = void();
    // ファイルの先頭に実行可能な機械語が配置されている想定
    auto f = reinterpret_cast<Func*>(&file_buf[0]);
    f();
}

void Terminal::Print(const char* s) {
    DrawCursor(false);

    while (*s) {
        Print(*s);
        s++;
    }

    DrawCursor(true);
}

void Terminal::Print(char c) {
    auto newline = [this]() {
        cursor_.x = 0;
        if (cursor_.y < kRows - 1) {
            cursor_.y++;
        } else {
            Scroll1();
        }
    };

    if (c == '\n') {
        newline();
    } else {
        WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
        // 画面右端に到達したら改行
        if (cursor_.x == kColumns - 1) {
            newline();
        } else {
            cursor_.x++;
        }
    }
}

Rectangle<int> Terminal::HistoryUpDown(int direction) {
    if (direction == -1 && cmd_history_index_ >= 0) { // 最新に近づく
        cmd_history_index_--;
    } else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) { // 過去に遡る
        cmd_history_index_++;
    }

    cursor_.x = 1;
    const auto first_pos = CalcCursorPos();

    Rectangle<int> draw_area{first_pos, {8 * (kColumns - 1), 16}};
    FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0});

    const char* history = "";
    if (cmd_history_index_ >= 0) {
        history = &cmd_history_[cmd_history_index_][0];
    }

    strcpy(&linebuf_[0], history);
    linebuf_index_ = strlen(history);

    WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
    cursor_.x = linebuf_index_ + 1;
    return draw_area;
}

void TaskTerminal(uint64_t task_id, int64_t data) {
    // グローバル変数を扱う際は割り込みを禁止しておくのが無難
    __asm__("cli");
    Task& task = g_task_manager->CurrentTask();
    Terminal* terminal = new Terminal;
    g_layer_manager->Move(terminal->LayerID(), {100, 200});
    g_active_layer->Activate(terminal->LayerID());
    g_layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    __asm__("sti");

    while (true) {
        __asm__("cli");
        auto msg = task.ReceiveMessage();
        if (!msg) {
            task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type) {
        case Message::kTimerTimeout: {
            // 一定時間ごとにカーゾルを点滅させる
            const auto area = terminal->BlinkCursor();
            Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
            // メインタスクに描画処理を要求
            __asm__("cli");
            g_task_manager->SendMessage(1, msg);
            __asm__("sti");
        } break;
        case Message::kKeyPush: {
            const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                                 msg->arg.keyboard.keycode,
                                                 msg->arg.keyboard.ascii);
            Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
            // メインタスクに描画処理を要求
            __asm__("cli");
            g_task_manager->SendMessage(1, msg);
            __asm__("sti");
        } break;
        default:
            break;
        }
    }
}