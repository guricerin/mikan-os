#pragma once

#include <optional>
#include <vector>

#include "frame_buffer.hpp"
#include "graphics.hpp"

/// グラフィック（四角形）の表示領域を表す
/// タイトルやメニューがあるウィンドウだけでなく、マウスカーソルの表示領域なども対象とする
class Window {
public:
    /// Window専用のPixelWriterを提供する
    class WindowWriter : public PixelWriter {
    public:
        WindowWriter(Window& window) : window_{window} {}
        /// 指定された位置に指定された色を描画
        virtual void Write(Vector2D<int> pos, const PixelColor& color) override {
            window_.Write(pos, color);
        }
        virtual int Width() const override { return window_.Width(); }
        virtual int Height() const override { return window_.Height(); }

    private:
        Window& window_;
    };

    /// 指定されたピクセル数の平面描画領域を作成
    Window(int width, int height, PixelFormat shadow_format);
    ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    /// 与えられたPixelWriterにこのウィンドウの表示領域を描画
    /// dst : 描画先
    /// position : dstの左上を基準としたウィンドウ位置
    /// area : dstの左上の基準とた描画対象範囲
    void DrawTo(FrameBuffer& dst, Vector2D<int> position, const Rectangle<int>& area);
    void SetTransparentColor(std::optional<PixelColor> color);
    /// このインスタンスに紐付いたWindowWriterを取得
    WindowWriter* Writer();

    /// 指定した位置のピクセルを返す
    const PixelColor& At(Vector2D<int> pos) const;

    void Write(Vector2D<int> pos, PixelColor color);

    int Width() const;
    int Height() const;
    Vector2D<int> Size() const;

    /// このウィンドウの平面領域内で、矩形領域を移動する
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    /// 透過色
    std::optional<PixelColor> transparent_color_{std::nullopt};

    /// 本命のメモリ領域には最適化されたmemcpyで後で一気に書き込む
    FrameBuffer shadow_buffer_{};
};

/// 窓を描画
void DrawWindow(PixelWriter& writer, const char* title);