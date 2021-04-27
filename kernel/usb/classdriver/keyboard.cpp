#include "usb/classdriver/keyboard.hpp"

#include "usb/device.hpp"
#include "usb/memory.hpp"
#include <algorithm>

namespace usb {
    HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
        : HIDBaseDriver{dev, interface_index, 8} {
    }

    Error HIDKeyboardDriver::OnDataReceived() {
        // キーコードの配置はブートインターフェースに則る
        // ブートインターフェース :
        /// HID機器に内蔵されたレポートディスクリプタを解釈するのではなく、
        /// UEFI BIOSなどあまり高度なことをしたくないソフトでマウスやキーボードなど最低限の機器を制御したい場合のインターフェース
        for (int i = 2; i < 8; ++i) {
            const uint8_t key = Buffer()[i];
            if (key == 0) {
                continue;
            }
            const auto& prev_buf = PreviousBuffer();
            // +2 は、キーコードが配置されるのが [Buffer()[2], Buffer()[7]]のため
            if (std::find(prev_buf.begin() + 2, prev_buf.end(), key) != prev_buf.end()) {
                continue;
            }

            // ブートインターフェースの場合、モディファイアキーのバイト位置は0固定
            // ちなみにBuffer()[1]は予約領域
            const uint8_t modifier = Buffer()[0];
            NotifyKeyPush(modifier, key);
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    void* HIDKeyboardDriver::operator new(size_t size) {
        return AllocMem(sizeof(HIDKeyboardDriver), 0, 0);
    }

    void HIDKeyboardDriver::operator delete(void* ptr) noexcept {
        FreeMem(ptr);
    }

    void HIDKeyboardDriver::SubscribeKeyPush(
        std::function<ObserverType> observer) {
        observers_[num_observers_++] = observer;
    }

    std::function<HIDKeyboardDriver::ObserverType> HIDKeyboardDriver::default_observer;

    void HIDKeyboardDriver::NotifyKeyPush(uint8_t modifier, uint8_t keycode) {
        for (int i = 0; i < num_observers_; ++i) {
            observers_[i](modifier, keycode);
        }
    }
} // namespace usb
