#include "terminal.hpp"

#include <cstring>

#include "../MikanLoaderPkg/elf.h"
#include "asmfunc.h"
#include "font.hpp"
#include "layer.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "pci.hpp"

namespace {
    /// 空白区切りのコマンドライン引数を配列（argbuf）に詰める
    WithError<int> MakeArgVector(char* command, char* first_arg, char** argv, int argv_len, char* argbuf, int argbuf_len) {
        int argc = 0;
        int argbuf_index = 0;

        auto push_to_argv = [&](const char* s) {
            if (argc >= argv_len || argbuf_index >= argbuf_len) {
                return MAKE_ERROR(Error::kFull);
            }

            argv[argc] = &argbuf[argbuf_index];
            argc++;
            strcpy(&argbuf[argbuf_index], s);
            argbuf_index += strlen(s) + 1;
            return MAKE_ERROR(Error::kSuccess);
        };

        if (auto err = push_to_argv(command)) {
            return {argc, err};
        }
        if (!first_arg) {
            return {argc, MAKE_ERROR(Error::kSuccess)};
        }

        char* p = first_arg;
        while (true) {
            while (isspace(p[0])) {
                p++;
            }
            if (p[0] == 0) {
                break;
            }

            const char* arg = p;
            while (p[0] != 0 && !isspace(p[0])) {
                p++;
            }

            const bool is_end = p[0] == 0;
            p[0] = 0;
            if (auto err = push_to_argv(arg)) {
                return {argc, err};
            }
            if (is_end) {
                break;
            }

            p++;
        }

        return {argc, MAKE_ERROR(Error::kSuccess)};
    }

    Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
        return reinterpret_cast<Elf64_Phdr*>(reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
    }

    uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
        auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type != PT_LOAD) {
                continue;
            }
            return phdr[i].p_vaddr;
        }
        return 0;
    }

    static_assert(kBytesPerFrame >= 4096);

    /// 新たなページング構造を生成
    WithError<PageMapEntry*> NewPageMap() {
        auto frame = g_memory_manager->Allocate(1);
        if (frame.error) {
            return {nullptr, frame.error};
        }

        auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
        memset(e, 0, sizeof(uint64_t) * 512);
        return {e, MAKE_ERROR(Error::kSuccess)};
    }

    /// 必要に応じて新たなページング構造を生成してエントリに設定
    WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
        // 有効な値が設定済み
        if (entry.bits.present) {
            return {entry.Pointer(), MAKE_ERROR(Error::kSuccess)};
        }

        auto [child_map, err] = NewPageMap();
        if (err) {
            return {nullptr, err};
        }

        entry.SetPointer(child_map);
        entry.bits.present = 1;

        return {child_map, MAKE_ERROR(Error::kSuccess)};
    }

    /// 指定階層を設定
    /// page_map : 階層ページング構造の物理アドレス
    /// page_map_level : 設定対象のページング階層
    /// addr : LOADセグメントを配置する先頭アドレス
    /// num_4kPages : 4KiBページ単位のセグメントの大きさ
    /// ret : 未処理のページ数
    WithError<size_t> SetupPageMap(PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kPages) {
        while (num_4kPages > 0) {
            // 仮想アドレスの指定した階層の値
            const auto entry_index = addr.Part(page_map_level);

            auto [child_map, err] = SetNewPageMapIfNotPresent(page_map[entry_index]);
            if (err) {
                return {num_4kPages, err};
            }
            page_map[entry_index].bits.writable = 1;
            // 権限レベルが最低でも命令フェッチを許可する
            // この関数はアプリ用の階層ページング構造を設定するものなので、OS領域のuserビットは0のまま変更していない
            // -> OSのメモリ空間は保護される
            page_map[entry_index].bits.user = 1;

            if (page_map_level == 1) {
                num_4kPages--;
            } else {
                auto [num_remain_pages, err] = SetupPageMap(child_map, page_map_level - 1, addr, num_4kPages);
                if (err) {
                    return {num_4kPages, err};
                }
                num_4kPages = num_remain_pages;
            }

            if (entry_index == 511) {
                break;
            }

            addr.SetPart(page_map_level, entry_index + 1);
            for (int level = page_map_level - 1; level >= 1; level--) {
                addr.SetPart(level, 0);
            }
        }

        return {num_4kPages, MAKE_ERROR(Error::kSuccess)};
    }

    /// 階層ページング構造の設定
    /// addr : LOADセグメントを配置する先頭アドレス
    /// num_4kPages : 4KiBページ単位のセグメントの大きさ
    Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
        auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
        return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
    }

    /// LOADセグメントを最終目的地にコピー
    Error CopyLoadSegments(Elf64_Ehdr* ehdr) {
        auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type != PT_LOAD) {
                continue;
            }

            // 階層ページング構造の設定
            LinearAddress4Level dest_addr;
            dest_addr.value = phdr[i].p_vaddr;
            const auto num_4kpages = (phdr[i].p_memsz + 4096) / 4096;
            if (auto err = SetupPageMaps(dest_addr, num_4kpages)) {
                return err;
            }

            const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
            const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
            memcpy(dst, src, phdr[i].p_filesz);
            memcpy(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    /// elfファイルをアプリとして読み込み、実行可能にする
    Error LoadElf(Elf64_Ehdr* ehdr) {
        // elfファイルは実行可能か？
        if (ehdr->e_type != ET_EXEC) {
            return MAKE_ERROR(Error::kInvalidFormat);
        }

        // 1つ目のLOADセグメントの仮想アドレスがカノニカルアドレスの後半領域か？
        const auto addr_first = GetFirstLoadAddress(ehdr);
        if (addr_first < 0xffff800000000000) {
            return MAKE_ERROR(Error::kInvalidFormat);
        }

        if (auto err = CopyLoadSegments(ehdr)) {
            return err;
        }

        return MAKE_ERROR(Error::kSuccess);
    }

    /// 指定階層のページング構造内のエントリをすべて破棄
    Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
        for (int i = 0; i < 512; i++) {
            auto entry = page_map[i];
            if (!entry.bits.present) {
                continue;
            }

            // 深さ優先探索
            if (page_map_level > 1) {
                if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
                    return err;
                }
            }

            const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
            const FrameID map_frame{entry_addr / kBytesPerFrame};
            if (auto err = g_memory_manager->Free(map_frame, 1)) {
                return err;
            }
            page_map[i].data = 0;
        }

        return MAKE_ERROR(Error::kSuccess);
    }

    /// アプリ用のページング構造を破棄
    Error CleanPageMaps(LinearAddress4Level addr) {
        auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
        auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
        pml4_table[addr.parts.pml4].data = 0;
        if (auto err = CleanPageMap(pdp_table, 3)) {
            return err;
        }

        const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
        const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
        return g_memory_manager->Free(pdp_frame, 1);
    }
} // namespace

Terminal::Terminal(uint64_t task_id) : task_id_{task_id} {
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
        } else if (auto err = ExecuteFile(*file_entry, command, first_arg)) {
            Print("failed to exec file: ");
            Print(err.Name());
            Print("\n");
        }
    }
}

Error Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg) {
    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

    auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
    // フラットバイナリ（ファイルの先頭に実行可能な機械語が配置されている）の場合
    if (memcmp(elf_header->e_ident, "\x7f"
                                    "ELF",
               4) != 0) {
        using Func = void();
        auto f = reinterpret_cast<Func*>(&file_buf[0]);
        f();
        return MAKE_ERROR(Error::kSuccess);
    }

    // elf形式の場合
    if (auto err = LoadElf(elf_header)) {
        return err;
    }

    // コマンドライン引数を取得
    // アプリ用のページにargvを構築
    LinearAddress4Level args_frame_addr{0xfffffffffffff000};
    if (auto err = SetupPageMaps(args_frame_addr, 1)) {
        return err;
    }
    // new演算子はsbrk()（OS用のアドレス空間に存在するメモリ領域を使う）を呼び出してしまうので、使用しない
    // アプリ用の領域（userビットを1としたページ）に用意する
    auto argv = reinterpret_cast<char**>(args_frame_addr.value);
    // argvの要素数（決め打ちで32個を上限とする）
    int argv_len = 32; // argv = 8x32 = 256bytes
    auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char**) * argv_len);
    int argbuf_len = 4096 - sizeof(char**) * argv_len;
    auto argc = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
    if (argc.error) {
        return argc.error;
    }

    LinearAddress4Level stack_frame_addr{0xffffffffffffe000};
    if (auto err = SetupPageMaps(stack_frame_addr, 1)) {
        return err;
    }

    __asm__("cli");
    auto& task = g_task_manager->CurrentTask();
    __asm__("sti");

    // エントリポイントのアドレスを取得し、実行
    auto entry_addr = elf_header->e_entry;
    int ret = CallApp(argc.value,
                      argv,
                      3 << 3 | 3,
                      entry_addr,
                      stack_frame_addr.value + 4096 - 8,
                      &task.OSStackPointer()); // アプリ終了時に復帰するスタックポインタ

    char s[64];
    sprintf(s, "app exited. ret = %d\n", ret);

    // アプリ終了後、使用したメモリ領域を解放
    const auto addr_first = GetFirstLoadAddress(elf_header);
    if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
        return err;
    }

    return MAKE_ERROR(Error::kSuccess);
}

void Terminal::Print(const char* s, std::optional<size_t> len) {
    const auto cursor_before = CalcCursorPos();
    DrawCursor(false);

    if (len) { // バイト数を指定する場合
        for (size_t i = 0; i < *len; i++) {
            Print(*s);
            s++;
        }
    } else { // バイト数を指定しない場合
        while (*s) {
            Print(*s);
            s++;
        }
    }

    DrawCursor(true);
    const auto cursor_after = CalcCursorPos();

    Vector2D<int> draw_pos{TopLevelWindow::kTopLeftMargin.x, cursor_before.y};
    Vector2D<int> draw_size{window_->InnerSize().x, cursor_after.y - cursor_before.y + 16};
    Rectangle<int> draw_area{draw_pos, draw_size};

    // 画面を再描画
    Message msg = MakeLayerMessage(task_id_, LayerID(), LayerOperation::DrawArea, draw_area);
    __asm__("cli");
    g_task_manager->SendMessage(1, msg);
    __asm__("sti");
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

std::map<uint64_t, Terminal*>* g_terminals;

void TaskTerminal(uint64_t task_id, int64_t data) {
    // グローバル変数を扱う際は割り込みを禁止しておくのが無難
    __asm__("cli");
    Task& task = g_task_manager->CurrentTask();
    Terminal* terminal = new Terminal{task_id};
    g_layer_manager->Move(terminal->LayerID(), {100, 200});
    g_active_layer->Activate(terminal->LayerID());
    g_layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    // ターミナルのタスクが起動する時、自分自身を対応表に登録
    (*g_terminals)[task_id] = terminal;
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