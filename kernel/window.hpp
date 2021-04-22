#pragma once

#include "graphics.hpp"
#include <optional>
#include <vector>

/// グラフィック（四角形）の表示領域を表す
/// タイトルやメニューがあるウィンドウだけでなく、マウスカーソルの表示領域なども対象とする
class Window {
public:
    /// Window専用のPixelWriterを提供する
    class WindowWriter : public PixelWriter {
    public:
        WindowWriter(Window& window) : window_{window} {}
        /// 指定された位置に指定された色を描画
        virtual void Write(int x, int y, const PixelColor& color) override {
            window_.At(x, y) = color;
        }
        virtual int Width() const override { return window_.Width(); }
        virtual int Height() const override { return window_.Height(); }

    private:
        Window& window_;
    };

    /// 指定されたピクセル数の平面描画領域を作成
    Window(int width, int height);
    ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    /// 与えられたPixelWriterにこのウィンドウの表示領域を描画
    /// writer : 描画先
    /// position : writerの左上を基準とした描画位置
    void DrawTo(PixelWriter& writer, Vector2D<int> position);
    void SetTransparentColor(std::optional<PixelColor> color);
    /// このインスタンスに紐付いたWindowWriterを取得
    WindowWriter* Writer();

    /// 指定した位置のピクセルを返す
    PixelColor& At(int x, int y);
    const PixelColor& At(int x, int y) const;

    int Width() const;
    int Height() const;

private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    /// 透過色
    std::optional<PixelColor> transparent_color_{std::nullopt};
};