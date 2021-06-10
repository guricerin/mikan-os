#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// アプリ側が受信するイベント
struct AppEvent {
    enum Type {
        kQuit,
        kMouseMove,
        kMouseButton,
    } type;

    union {
        struct {
            /// ウィンドウ左上が基準の座標
            int x, y;
            /// 直前からの移動量
            int dx, dy;
            /// マウスのボタンの押下状況
            uint8_t buttons;
        } mouse_move;

        struct {
            /// ウィンドウ左上が基準の座標
            int x, y;
            // 1 : press, 0 : release
            int press;
            /// クリックされたボタンの種類（[0b00000000, 0b11111111]）
            int button;
        } mouse_button;
    } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
