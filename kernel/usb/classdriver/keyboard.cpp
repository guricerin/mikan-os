#include "usb/classdriver/keyboard.hpp"

#include "usb/device.hpp"
#include "usb/memory.hpp"
#include <algorithm>
#include <bitset>

namespace usb {
    HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
        : HIDBaseDriver{dev, interface_index, 8} {
    }

    Error HIDKeyboardDriver::OnDataReceived() {
        // キーコードの配置はブートインターフェースに則る
        // ブートインターフェース :
        /// HID機器に内蔵されたレポートディスクリプタを解釈するのではなく、
        /// UEFI BIOSなどあまり高度なことをしたくないソフトでマウスやキーボードなど最低限の機器を制御したい場合のインターフェース
        std::bitset<256> prev, current;
        for (int i = 2; i < 8; ++i) {
            prev.set(PreviousBuffer()[i], true);
            current.set(Buffer()[i], true);
        }

        const auto changed = prev ^ current;
        const auto pressed = changed & current;
        for (int key = 1; key < 256; key++) {
            if (changed.test(key)) {
                // ブートインターフェースの場合、モディファイアキーのバイト位置は0固定
                // ちなみにBuffer()[1]は予約領域
                const uint8_t modifier = Buffer()[0];
                NotifyKeyPush(modifier, key, pressed.test(key));
            }
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

    void HIDKeyboardDriver::NotifyKeyPush(uint8_t modifier, uint8_t keycode, bool press) {
        for (int i = 0; i < num_observers_; ++i) {
            observers_[i](modifier, keycode, press);
        }
    }
} // namespace usb
