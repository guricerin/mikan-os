#pragma once

#include <cstdint>

enum class LayerOperation {
    /// 指定の座標に移動
    Move,
    /// 移動量を指定
    MoveRelative,
    /// 全体描画
    Draw,
    /// 描画領域を指定
    DrawArea,
};

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
        kTimerTimeout,
        kKeyPush,
        kLayer,
        kLayerFinish,
        kMouseMove,
        kMouseButton,
        kWindowActive,
        kPipe,
        kWindowClose,
    } type;

    /// メッセージ送信元のタスクID
    uint64_t src_task;

    union {
        /// タイマーイベント
        struct {
            unsigned long timeout;
            int value;
        } timer;

        /// キー押下イベント
        struct {
            uint8_t modifier;
            uint8_t keycode;
            char ascii;
            /// 1 : press, 0 : release
            int press;
        } keyboard;

        /// 描画イベント
        struct {
            LayerOperation op;
            /// 操作対象のレイヤーID
            unsigned int layer_id;
            int x, y;
            int w, h;
        } layer;

        /// マウス移動イベント
        struct {
            /// ウィンドウ左上が基準の座標
            int x, y;
            /// 直前からの移動量
            int dx, dy;
            /// マウスのボタンの押下状況
            uint8_t buttons;
        } mouse_move;

        /// マウスボタンクリックイベント
        struct {
            /// ウィンドウ左上が基準の座標
            int x, y;
            /// 1 : press, 0 : release
            int press;
            /// クリックされたボタンの種類（[0b00000000, 0b11111111]）
            int button;
        } mouse_button;

        /// ウィンドウアクティブ化イベント
        struct {
            /// 1: activate, 0: deactivate
            int activate;
        } window_active;

        /// パイプによる送信データ
        struct {
            char data[16];
            uint8_t len;
        } pipe;

        /// ウィンドウを閉じる
        struct {
            unsigned int layer_id;
        } window_close;
    } arg;
};
