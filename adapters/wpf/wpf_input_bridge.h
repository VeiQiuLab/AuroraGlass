#pragma once

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WPF_INTEROP_EXPORTS)
        #define AURORAGLASS_WPF_INPUT_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WPF_INPUT_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WPF_INPUT_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WPF_INPUT_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WPF_INPUT_NOEXCEPT
#endif

typedef struct AuroraGlassWpfInputBridge
    AuroraGlassWpfInputBridge;

typedef struct AuroraGlassWpfButton
    AuroraGlassWpfButton;

typedef struct AuroraGlassWpfToggle
    AuroraGlassWpfToggle;

typedef struct AuroraGlassWpfSlider
    AuroraGlassWpfSlider;

typedef enum AuroraGlassWpfInputStatus {
    AURORAGLASS_WPF_INPUT_OK = 0,
    AURORAGLASS_WPF_INPUT_INVALID_ARGUMENT = 1,
    AURORAGLASS_WPF_INPUT_INVALID_WINDOW = 2,
    AURORAGLASS_WPF_INPUT_DIFFERENT_WINDOW_ALREADY_ATTACHED = 3,
    AURORAGLASS_WPF_INPUT_ATTACH_FAILED = 4,
    AURORAGLASS_WPF_INPUT_ADD_CONTROL_FAILED = 5
} AuroraGlassWpfInputStatus;

AURORAGLASS_WPF_INPUT_API
AuroraGlassWpfInputBridge*
AuroraGlassWpfInputCreate(void)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
void
AuroraGlassWpfInputDestroy(
    AuroraGlassWpfInputBridge* bridge)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputAttach(
    AuroraGlassWpfInputBridge* bridge,
    intptr_t hwnd)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
void
AuroraGlassWpfInputDetach(
    AuroraGlassWpfInputBridge* bridge)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputIsAttached(
    const AuroraGlassWpfInputBridge* bridge)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputOwnsCapture(
    const AuroraGlassWpfInputBridge* bridge)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputAddButton(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfButton** button)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputAddToggle(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfToggle** toggle)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfInputAddSlider(
    AuroraGlassWpfInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWpfSlider** slider)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfButtonSetBounds(
    AuroraGlassWpfButton* button,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
uint32_t
AuroraGlassWpfButtonClickCount(
    const AuroraGlassWpfButton* button)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfButtonIsPressed(
    const AuroraGlassWpfButton* button)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfToggleSetBounds(
    AuroraGlassWpfToggle* toggle,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfToggleIsChecked(
    const AuroraGlassWpfToggle* toggle)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfToggleIsPressed(
    const AuroraGlassWpfToggle* toggle)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfSliderSetBounds(
    AuroraGlassWpfSlider* slider,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
float
AuroraGlassWpfSliderValue(
    const AuroraGlassWpfSlider* slider)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

AURORAGLASS_WPF_INPUT_API
int32_t
AuroraGlassWpfSliderIsPressed(
    const AuroraGlassWpfSlider* slider)
    AURORAGLASS_WPF_INPUT_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WPF_INPUT_NOEXCEPT
