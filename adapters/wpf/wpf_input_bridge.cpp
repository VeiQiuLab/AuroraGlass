#include "wpf/wpf_input_bridge.h"

#include "win32/win32_control_input_bridge.h"

#include <Windows.h>

#include <cmath>
#include <memory>
#include <new>
#include <vector>

using AuroraGlass::ControlInteractionState;
using AuroraGlass::GlassButton;
using AuroraGlass::GlassSlider;
using AuroraGlass::GlassToggle;
using AuroraGlass::Adapters::Win32::Win32ControlInputBridge;

struct AuroraGlassWpfButton {
    GlassButton control{};
    uint32_t clickCount = 0;
};

struct AuroraGlassWpfToggle {
    GlassToggle control{};
};

struct AuroraGlassWpfSlider {
    GlassSlider control{};
};

struct AuroraGlassWpfInputBridge {
    std::vector<std::unique_ptr<AuroraGlassWpfButton>>
        buttons{};

    std::vector<std::unique_ptr<AuroraGlassWpfToggle>>
        toggles{};

    std::vector<std::unique_ptr<AuroraGlassWpfSlider>>
        sliders{};

    Win32ControlInputBridge input{};
};

namespace {

bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

bool ValidBounds(
    float x,
    float y,
    float width,
    float height) noexcept
{
    return IsFinite(x) &&
           IsFinite(y) &&
           IsFinite(width) &&
           IsFinite(height) &&
           width >= 0.0f &&
           height >= 0.0f;
}

template <typename T>
void AssignBounds(
    T& control,
    float x,
    float y,
    float width,
    float height) noexcept
{
    control.bounds.x = x;
    control.bounds.y = y;
    control.bounds.width = width;
    control.bounds.height = height;
}

template <typename T>
int32_t SetControlBounds(
    T* wrapper,
    float x,
    float y,
    float width,
    float height) noexcept
{
    if (wrapper == nullptr ||
        !ValidBounds(
            x,
            y,
            width,
            height))
    {
        return AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT;
    }

    AssignBounds(
        wrapper->control,
        x,
        y,
        width,
        height);

    return AURORAGLASS_WPF_INPUT_OK;
}

}

extern "C" {

AuroraGlassWpfInputBridge*
AuroraGlassWpfInputCreate(void) noexcept
{
    return new (std::nothrow)
        AuroraGlassWpfInputBridge{};
}

void
AuroraGlassWpfInputDestroy(
    AuroraGlassWpfInputBridge* bridge) noexcept
{
    delete bridge;
}

int32_t
AuroraGlassWpfInputAttach(
    AuroraGlassWpfInputBridge* bridge,
    intptr_t hwndValue) noexcept
{
    if (bridge == nullptr ||
        hwndValue == 0)
    {
        return AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT;
    }

    HWND hwnd =
        reinterpret_cast<HWND>(
            hwndValue);

    if (!IsWindow(hwnd)) {
        return AURORAGLASS_WPF_INPUT_INVALID_WINDOW;
    }

    if (bridge->input.IsAttached()) {
        if (bridge->input.Window() == hwnd) {
            return AURORAGLASS_WPF_INPUT_OK;
        }

        return
            AURORAGLASS_WPF_INPUT_DIFFERENT_WINDOW_ALREADY_ATTACHED;
    }

    if (!bridge->input.Attach(hwnd)) {
        return AURORAGLASS_WPF_INPUT_ATTACH_FAILED;
    }

    return AURORAGLASS_WPF_INPUT_OK;
}

void
AuroraGlassWpfInputDetach(
    AuroraGlassWpfInputBridge* bridge) noexcept
{
    if (bridge != nullptr) {
        bridge->input.Detach();
    }
}

int32_t
AuroraGlassWpfInputIsAttached(
    const AuroraGlassWpfInputBridge* bridge) noexcept
{
    return bridge != nullptr &&
           bridge->input.IsAttached()
        ? 1
        : 0;
}

int32_t
AuroraGlassWpfInputOwnsCapture(
    const AuroraGlassWpfInputBridge* bridge) noexcept
{
    return bridge != nullptr &&
           bridge->input.OwnsCapture()
        ? 1
        : 0;
}

int32_t
AuroraGlassWpfInputAddButton(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfButton** button) noexcept
{
    if (bridge == nullptr ||
        button == nullptr ||
        !ValidBounds(
            x,
            y,
            width,
            height))
    {
        return AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT;
    }

    auto item =
        std::make_unique<
            AuroraGlassWpfButton>();

    AssignBounds(
        item->control,
        x,
        y,
        width,
        height);

    AuroraGlassWpfButton* raw =
        item.get();

    item->control.onClick =
        [raw]() {
            ++raw->clickCount;
        };

    if (!bridge->input.AddButton(
            item->control))
    {
        return
            AURORAGLASS_WPF_INPUT_ADD_CONTROL_FAILED;
    }

    bridge->buttons.push_back(
        std::move(item));

    *button = raw;

    return AURORAGLASS_WPF_INPUT_OK;
}

int32_t
AuroraGlassWpfInputAddToggle(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfToggle** toggle) noexcept
{
    if (bridge == nullptr ||
        toggle == nullptr ||
        !ValidBounds(
            x,
            y,
            width,
            height))
    {
        return AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT;
    }

    auto item =
        std::make_unique<
            AuroraGlassWpfToggle>();

    AssignBounds(
        item->control,
        x,
        y,
        width,
        height);

    AuroraGlassWpfToggle* raw =
        item.get();

    if (!bridge->input.AddToggle(
            item->control))
    {
        return
            AURORAGLASS_WPF_INPUT_ADD_CONTROL_FAILED;
    }

    bridge->toggles.push_back(
        std::move(item));

    *toggle = raw;

    return AURORAGLASS_WPF_INPUT_OK;
}

int32_t
AuroraGlassWpfInputAddSlider(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfSlider** slider) noexcept
{
    if (bridge == nullptr ||
        slider == nullptr ||
        !ValidBounds(
            x,
            y,
            width,
            height))
    {
        return AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT;
    }

    auto item =
        std::make_unique<
            AuroraGlassWpfSlider>();

    AssignBounds(
        item->control,
        x,
        y,
        width,
        height);

    AuroraGlassWpfSlider* raw =
        item.get();

    if (!bridge->input.AddSlider(
            item->control))
    {
        return
            AURORAGLASS_WPF_INPUT_ADD_CONTROL_FAILED;
    }

    bridge->sliders.push_back(
        std::move(item));

    *slider = raw;

    return AURORAGLASS_WPF_INPUT_OK;
}

int32_t
AuroraGlassWpfButtonSetBounds(
    AuroraGlassWpfButton* button,
    float x,
    float y,
    float width,
    float height) noexcept
{
    return SetControlBounds(
        button,
        x,
        y,
        width,
        height);
}

uint32_t
AuroraGlassWpfButtonClickCount(
    const AuroraGlassWpfButton* button) noexcept
{
    return button == nullptr
        ? 0u
        : button->clickCount;
}

int32_t
AuroraGlassWpfButtonIsPressed(
    const AuroraGlassWpfButton* button) noexcept
{
    return button != nullptr &&
           button->control.State() ==
               ControlInteractionState::Pressed
        ? 1
        : 0;
}

int32_t
AuroraGlassWpfToggleSetBounds(
    AuroraGlassWpfToggle* toggle,
    float x,
    float y,
    float width,
    float height) noexcept
{
    return SetControlBounds(
        toggle,
        x,
        y,
        width,
        height);
}

int32_t
AuroraGlassWpfToggleIsChecked(
    const AuroraGlassWpfToggle* toggle) noexcept
{
    return toggle != nullptr &&
           toggle->control.IsChecked()
        ? 1
        : 0;
}

int32_t
AuroraGlassWpfToggleIsPressed(
    const AuroraGlassWpfToggle* toggle) noexcept
{
    return toggle != nullptr &&
           toggle->control.State() ==
               ControlInteractionState::Pressed
        ? 1
        : 0;
}

int32_t
AuroraGlassWpfSliderSetBounds(
    AuroraGlassWpfSlider* slider,
    float x,
    float y,
    float width,
    float height) noexcept
{
    return SetControlBounds(
        slider,
        x,
        y,
        width,
        height);
}

float
AuroraGlassWpfSliderValue(
    const AuroraGlassWpfSlider* slider) noexcept
{
    return slider == nullptr
        ? 0.0f
        : slider->control.Value();
}

int32_t
AuroraGlassWpfSliderIsPressed(
    const AuroraGlassWpfSlider* slider) noexcept
{
    return slider != nullptr &&
           slider->control.State() ==
               ControlInteractionState::Pressed
        ? 1
        : 0;
}

}
