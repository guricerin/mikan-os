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
    /// 指定IDを持つレイヤーを生成
    Layer(unsigned int id = 0);
    unsigned int ID() const;

    /// 既存のウィンドウはこのレイヤーから外れる
    Layer& SetWindow(const std::shared_ptr<Window>& window);
    /// 設定されたウィンドウを返す
    std::shared_ptr<Window> GetWindow() const;
    /// レイヤーの原点座標を取得
    Vector2D<int> GetPosition() const;
    Layer& SetDraggable(bool draggable);
    bool IsDraggable() const;

    /// レイヤーの位置情報を指定の絶対座標へと更新。再描画はしない
    Layer& Move(Vector2D<int> pos);
    /// レイヤーの位置情報を指定の相対座標へと更新。再描画はしない
    Layer& MoveRelative(Vector2D<int> pos_diff);

    /// 指定の描画先にウィンドウの内容を描画
    void DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const;

private:
    unsigned int id_;
    /// 原点座標
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
    // true : マウスドラッグ可能
    bool draggable_{false};
};

/// 複数のレイヤーを管理する
class LayerManager {
public:
    void SetScreen(FrameBuffer* screen);

    /// 新しいレイヤーを生成して参照を返す
    /// そのレイヤーの実体はLayerManager内部のコンテナで保持される
    Layer& NewLayer();

    /// 現在表示状態にあるレイヤーを描画
    void Draw(const Rectangle<int>& area) const;
    /// 指定レイヤーに設定されているウィンドウの描画領域内を描画
    void Draw(unsigned int id) const;

    /// 例親ーの位置情報を指定の絶対座標へと更新。再描画。
    void Move(unsigned int id, Vector2D<int> new_position);

    /// 例親ーの位置情報を指定の相対座標へと更新。再描画。
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

    /// レイヤーの重なり順の位置を指定の位置に移動
    /// new_heightに負の位置を指定するとレイヤーは非表示となり、0以上を指定するとその位置となる
    /// 現在のレイヤー数以上の数値を指定した場合は最前面のレイヤーとなる
    void UpDown(unsigned int id, int new_height);

    /// レイヤーを非表示にする
    void Hide(unsigned int id);

    /// 指定座標にウィンドウをもつ最前面のレイヤーを取得
    Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;

private:
    FrameBuffer* screen_{nullptr};
    /// ダブルバッファリング用
    /// mutable修飾子はconstメソッド内からでも変更可能
    mutable FrameBuffer back_buffer_{};
    std::vector<std::unique_ptr<Layer>> layers_{};
    /// 配列の先頭を再背面、末尾を最前面とする。非表示レイヤは含まない
    std::vector<Layer*> layer_stack_{};
    unsigned int latest_id_{0};

    /// 指定IDのレイヤーを返す
    Layer* FindLayer(unsigned int id);
};

extern LayerManager* g_layer_manager;