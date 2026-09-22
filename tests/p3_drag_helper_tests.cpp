// AuroraGlass P3 sample-only drag helper tests (pure CPU).
#include "drag_helper.h"
#include <cstdio>
using namespace AuroraGlass;
static int g_c = 0, g_f = 0;
#define CHECK(x) do { ++g_c; if(!(x)){ ++g_f; std::printf("  FAIL %d %s\n", __LINE__, #x);} } while(0)
int main() {
    ControlBounds obj{ 100, 100, 160, 90 };
    sample::DragState st;

    // Down outside -> no drag.
    CHECK(!sample::BeginDrag(obj, { 10, 10 }, st));
    CHECK(!st.active);

    // Down inside -> starts, offset preserved (center does not jump).
    CHECK(sample::BeginDrag(obj, { 120, 130 }, st));
    CHECK(st.active);
    CHECK(st.offX == 20.0f && st.offY == 30.0f);

    // Move -> bounds updated by cursor - offset.
    ControlBounds m = sample::UpdateDrag(obj, { 300, 200 }, st);
    CHECK(m.x == 280.0f && m.y == 170.0f);

    // End -> stops; further update keeps bounds.
    sample::EndDrag(st);
    ControlBounds m2 = sample::UpdateDrag(m, { 500, 500 }, st);
    CHECK(m2.x == m.x && m2.y == m.y);

    std::printf("Checks: %d, Failures: %d\n", g_c, g_f);
    std::printf("%s\n", g_f == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_f == 0 ? 0 : 1;
}
