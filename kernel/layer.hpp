/// 重ね合わせ処理

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"

/// 1つの描画層
/// 現状では1つのウィンドウしか保持できない設計だが、
/// 将来的には複数のウィンドウを持ち得る
class Layer {
public:
private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
};

/// 複数のレイヤーを管理する
class LayerManager {
public:
private:
};

extern LayerManager* g_layer_manager;