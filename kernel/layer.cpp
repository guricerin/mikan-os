#include "layer.hpp"

#include <algorithm>

#include "console.hpp"
#include "logger.hpp"
#include "task.hpp"

namespace {
    template <class T, class U>
    void EraseIf(T& c, const U& pred) {
        auto it = std::remove_if(c.begin(), c.end(), pred);
        c.erase(it, c.end());
    }
} // namespace

Layer::Layer(unsigned int id) : id_{id} {}

unsigned int Layer::ID() const {
    return id_;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
    window_ = window;
    return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const {
    return window_;
}

Vector2D<int> Layer::GetPosition() const {
    return pos_;
}

Layer& Layer::SetDraggable(bool draggable) {
    draggable_ = draggable;
    return *this;
}

bool Layer::IsDraggable() const {
    return draggable_;
}

Layer& Layer::Move(Vector2D<int> pos) {
    pos_ = pos;
    return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
    pos_ += pos_diff;
    return *this;
}

void Layer::DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const {
    if (window_) {
        window_->DrawTo(screen, pos_, area);
    }
}

void LayerManager::SetScreen(FrameBuffer* screen) {
    screen_ = screen;

    // バックバッファ初期化
    FrameBufferConfig back_config = screen->Config();
    back_config.frame_buffer = nullptr;
    back_buffer_.Initailize(back_config);
}

Layer& LayerManager::NewLayer() {
    latest_id_++;
    return *layers_.emplace_back(new Layer{latest_id_});
}

void LayerManager::RemoveLayer(unsigned int id) {
    Hide(id);

    auto pred = [id](const std::unique_ptr<Layer>& elem) {
        return elem->ID() == id;
    };
    EraseIf(layers_, pred);
}

void LayerManager::Draw(const Rectangle<int>& area) const {
    for (auto layer : layer_stack_) {
        layer->DrawTo(back_buffer_, area);
    }
    screen_->Copy(area.pos, back_buffer_, area);
}

void LayerManager::Draw(unsigned int id) const {
    // レイヤ全体を描画
    Draw(id, {{0, 0}, {-1, -1}});
}

void LayerManager::Draw(unsigned int id, Rectangle<int> area) const {
    bool draw = false;
    Rectangle<int> window_area;
    for (auto layer : layer_stack_) {
        // 再描画されたレイヤーより前面のレイヤーはすべて再描画
        if (layer->ID() == id) {
            window_area.size = layer->GetWindow()->Size();
            window_area.pos = layer->GetPosition();
            if (area.size.x >= 0 || area.size.y >= 0) {
                // areaはウィンドウの左上を基準とした座標、window_areaはフレームバッファの左上を基準とした座標なので座標系を合わせる
                area.pos = area.pos + window_area.pos;
                window_area = window_area & area;
            }
            draw = true;
        }
        if (draw) {
            layer->DrawTo(back_buffer_, window_area);
        }
    }
    screen_->Copy(window_area.pos, back_buffer_, window_area);
}

void LayerManager::Move(unsigned int id, Vector2D<int> new_position) {
    auto layer = FindLayer(id);
    const auto window_size = layer->GetWindow()->Size();
    const auto old_pos = layer->GetPosition();
    layer->Move(new_position);
    Draw({old_pos, window_size});
    Draw(id);
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
    auto layer = FindLayer(id);
    const auto window_size = layer->GetWindow()->Size();
    const auto old_pos = layer->GetPosition();
    layer->MoveRelative(pos_diff);
    Draw({old_pos, window_size});
    Draw(id);
}

void LayerManager::UpDown(unsigned int id, int new_height) {
    if (new_height < 0) {
        Hide(id);
        return;
    }

    if (new_height > layer_stack_.size()) {
        new_height = layer_stack_.size();
    }

    auto layer = FindLayer(id);
    auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    auto new_pos = layer_stack_.begin() + new_height;

    if (old_pos == layer_stack_.end()) {
        layer_stack_.insert(new_pos, layer);
        return;
    }

    if (new_pos == layer_stack_.end()) {
        new_pos--;
    }
    layer_stack_.erase(old_pos);
    layer_stack_.insert(new_pos, layer);
}

void LayerManager::Hide(unsigned int id) {
    auto layer = FindLayer(id);
    auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    if (pos != layer_stack_.end()) {
        layer_stack_.erase(pos);
    }
}

Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const {
    auto pred = [pos, exclude_id](Layer* layer) {
        if (layer->ID() == exclude_id) {
            return false;
        }
        const auto& win = layer->GetWindow();
        if (!win) {
            return false;
        }

        const auto win_pos = layer->GetPosition();
        const auto win_end_pos = win_pos + win->Size();
        return win_pos.x <= pos.x && pos.x < win_end_pos.x &&
               win_pos.y <= pos.y && pos.y < win_end_pos.y;
    };

    // find back
    auto it = std::find_if(layer_stack_.rbegin(), layer_stack_.rend(), pred);
    if (it == layer_stack_.rend()) {
        return nullptr;
    }
    return *it;
}

Layer* LayerManager::FindLayer(unsigned int id) {
    auto pred = [id](const std::unique_ptr<Layer>& elem) {
        return elem->ID() == id;
    };
    auto it = std::find_if(layers_.begin(), layers_.end(), pred);
    if (it == layers_.end()) {
        return nullptr;
    }
    return it->get();
}

int LayerManager::GetHeight(unsigned int id) {
    for (int h = 0; h < layer_stack_.size(); h++) {
        if (layer_stack_[h]->ID() == id) {
            return h;
        }
    }
    return -1;
}

namespace {
    /// 本物のフレームバッファー
    FrameBuffer* g_screen;

    /// 指定レイヤが所属するタスクにアクティブ状態変更メッセージを送信
    Error SendWindowActiveMessage(unsigned int layer_id, int activate) {
        auto task_it = g_layer_task_map->find(layer_id);
        if (task_it == g_layer_task_map->end()) {
            return MAKE_ERROR(Error::kNoSuchTask);
        }

        Message msg{Message::kWindowActive};
        msg.arg.window_active.activate = activate;
        return g_task_manager->SendMessage(task_it->second, msg);
    }
} // namespace

LayerManager* g_layer_manager;

ActiveLayer::ActiveLayer(LayerManager& manager) : manager_{manager} {}

void ActiveLayer::SetMouseLayer(unsigned int mouse_layer) {
    mouse_layer_ = mouse_layer;
}

void ActiveLayer::Activate(unsigned int layer_id) {
    if (active_layer_ == layer_id) {
        return;
    }

    // 今までアクティブだったレイヤがもつ描画領域を非アクティブ化する
    if (active_layer_ > 0) {
        Layer* layer = manager_.FindLayer(active_layer_);
        layer->GetWindow()->Deactivate();
        manager_.Draw(active_layer_);
        SendWindowActiveMessage(active_layer_, 0);
    }

    // 指定レイヤを最前面に移動
    active_layer_ = layer_id;
    if (active_layer_ > 0) {
        Layer* layer = manager_.FindLayer(active_layer_);
        layer->GetWindow()->Activate();
        // マウスレイヤの1つ下
        manager_.UpDown(active_layer_, manager_.GetHeight(mouse_layer_) - 1);
        manager_.Draw(active_layer_);
        SendWindowActiveMessage(active_layer_, 1);
    }
}

ActiveLayer* g_active_layer;
std::map<unsigned int, uint64_t>* g_layer_task_map;

void InitializeLayer() {
    const auto screen_size = ScreenSize();

    // 背景ウィンドウ
    auto bg_window = std::make_shared<Window>(screen_size.x, screen_size.y, g_screen_config.pixel_format);
    DrawDesktop(*bg_window->Writer());

    // コンソール
    auto console_window = std::make_shared<Window>(Console::kColumns * 8, Console::kRows * 16, g_screen_config.pixel_format);
    // レイヤーマネージャーの準備が整ったので、コンソールをレイヤーの仕組みに載せ替える
    g_console->SetWindow(console_window);

    g_screen = new FrameBuffer;
    if (auto err = g_screen->Initailize(g_screen_config)) {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        exit(1);
    }

    g_layer_manager = new LayerManager;
    g_layer_manager->SetScreen(g_screen);

    auto bg_layer_id = g_layer_manager->NewLayer()
                           .SetWindow(bg_window)
                           .Move({0, 0})
                           .ID();
    g_console->SetLayerID(g_layer_manager->NewLayer()
                              .SetWindow(console_window)
                              .Move({0, 0})
                              .ID());

    g_layer_manager->UpDown(bg_layer_id, 0);
    g_layer_manager->UpDown(g_console->LayerID(), 1);

    g_active_layer = new ActiveLayer{*g_layer_manager};

    g_layer_task_map = new std::map<unsigned int, uint64_t>;
}

void ProcessLayerMessage(const Message& msg) {
    const auto& arg = msg.arg.layer;
    switch (arg.op) {
    case LayerOperation::Move:
        g_layer_manager->Move(arg.layer_id, {arg.x, arg.y});
        break;
    case LayerOperation::MoveRelative:
        g_layer_manager->MoveRelative(arg.layer_id, {arg.x, arg.y});
        break;
    case LayerOperation::Draw:
        g_layer_manager->Draw(arg.layer_id);
        break;
    case LayerOperation::DrawArea:
        g_layer_manager->Draw(arg.layer_id, {{arg.x, arg.y}, {arg.w, arg.h}});
        break;
    default:
        Log(kError, "LayerOperation is not exhausted");
        exit(1);
    }
}