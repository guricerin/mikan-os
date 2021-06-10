#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// アプリ側が受信するイベント
struct AppEvent {
    enum Type {
        kQuit,
        kMouseMove,
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
    } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
