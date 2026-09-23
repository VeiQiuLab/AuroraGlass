#pragma once

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WINUI_INTEROP_EXPORTS)
        #define AURORAGLASS_WINUI_INPUT_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WINUI_INPUT_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WINUI_INPUT_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WINUI_INPUT_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WINUI_INPUT_NOEXCEPT
#endif

typedef struct AuroraGlassWinUIInputBridge
    AuroraGlassWinUIInputBridge;

typedef struct AuroraGlassWinUIButton
    AuroraGlassWinUIButton;

typedef struct AuroraGlassWinUIToggle
    AuroraGlassWinUIToggle;

typedef struct AuroraGlassWinUISlider
    AuroraGlassWinUISlider;

typedef enum AuroraGlassWinUIInputStatus {
    AURORAGLASS_WINUI_INPUT_OK = 0,
    AURORAGLASS_WINUI_INPUT_INVALID_ARGUMENT = 1,
    AURORAGLASS_WINUI_INPUT_INVALID_WINDOW = 2,
    AURORAGLASS_WINUI_INPUT_DIFFERENT_WINDOW_ALREADY_ATTACHED = 3,
    AURORAGLASS_WINUI_INPUT_ATTACH_FAILED = 4,
    AURORAGLASS_WINUI_INPUT_ADD_CONTROL_FAILED = 5
} AuroraGlassWinUIInputStatus;

AURORAGLASS_WINUI_INPUT_API
AuroraGlassWinUIInputBridge*
AuroraGlassWinUIInputCreate(void)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
void
AuroraGlassWinUIInputDestroy(
    AuroraGlassWinUIInputBridge* bridge)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputAttach(
    AuroraGlassWinUIInputBridge* bridge,
    intptr_t hwnd)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
void
AuroraGlassWinUIInputDetach(
    AuroraGlassWinUIInputBridge* bridge)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputIsAttached(
    const AuroraGlassWinUIInputBridge* bridge)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputOwnsCapture(
    const AuroraGlassWinUIInputBridge* bridge)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputAddButton(
    AuroraGlassWinUIInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWinUIButton** button)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputAddToggle(
    AuroraGlassWinUIInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWinUIToggle** toggle)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIInputAddSlider(
    AuroraGlassWinUIInputBridge* bridge,
    float x,
    float y,
    float width,
    float height,
    AuroraGlassWinUISlider** slider)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIButtonSetBounds(
    AuroraGlassWinUIButton* button,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
uint32_t
AuroraGlassWinUIButtonClickCount(
    const AuroraGlassWinUIButton* button)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIButtonIsPressed(
    const AuroraGlassWinUIButton* button)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIToggleSetBounds(
    AuroraGlassWinUIToggle* toggle,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIToggleIsChecked(
    const AuroraGlassWinUIToggle* toggle)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUIToggleIsPressed(
    const AuroraGlassWinUIToggle* toggle)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUISliderSetBounds(
    AuroraGlassWinUISlider* slider,
    float x,
    float y,
    float width,
    float height)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
float
AuroraGlassWinUISliderValue(
    const AuroraGlassWinUISlider* slider)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

AURORAGLASS_WINUI_INPUT_API
int32_t
AuroraGlassWinUISliderIsPressed(
    const AuroraGlassWinUISlider* slider)
    AURORAGLASS_WINUI_INPUT_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WINUI_INPUT_NOEXCEPT
