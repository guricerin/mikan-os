#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// アプリ用のイベント
struct AppEvent {
    enum Type {
        kQuit,
    } type;
};

#ifdef __cplusplus
} // extern "C"
#endif
