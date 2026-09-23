#pragma once
// ============================================================
// AuroraGlass P3 interactive sample - SAMPLE-ONLY test bench background.
//
// Generates a bright/neutral reference image on the CPU (no DirectWrite, no SDK
// text stack). Contains: simple 5x7 text zones, basic shapes, a small fine-line
// patch, and contrast blocks. Purpose: a clean surface on which liquid-glass
// refraction / magnification / line bending can be judged by eye.
//
// NOT part of AuroraGlass Core.
// ============================================================

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace testbench {

inline uint32_t MakeRGB(int r, int g, int b) {
    return 0xFF000000u | ((uint32_t)(b & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(r & 255);
}

// 5x7 glyph rows (low 5 bits). Uppercase + digits + a few symbols.
inline const uint8_t* Glyph(char c) {
    static const uint8_t SP[7] = {0,0,0,0,0,0,0};
    static const uint8_t A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const uint8_t B[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
    static const uint8_t C[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
    static const uint8_t D[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    static const uint8_t E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    static const uint8_t F[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
    static const uint8_t G[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F};
    static const uint8_t H[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const uint8_t I[7] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
    static const uint8_t J[7] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C};
    static const uint8_t K[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    static const uint8_t L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
    static const uint8_t M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
    static const uint8_t N[7] = {0x11,0x11,0x19,0x15,0x13,0x11,0x11};
    static const uint8_t O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const uint8_t P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
    static const uint8_t Q[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
    static const uint8_t R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    static const uint8_t S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    static const uint8_t T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
    static const uint8_t U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const uint8_t V[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
    static const uint8_t W[7] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11};
    static const uint8_t X[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
    static const uint8_t Y[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
    static const uint8_t Z[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
    static const uint8_t D0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
    static const uint8_t D1[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
    static const uint8_t D2[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
    static const uint8_t D3[7] = {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E};
    static const uint8_t D4[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
    static const uint8_t D5[7] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
    static const uint8_t D6[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
    static const uint8_t D7[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
    static const uint8_t D8[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
    static const uint8_t D9[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};
    static const uint8_t DOT[7] = {0,0,0,0,0,0x0C,0x0C};
    switch (c) {
        case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D;
        case 'E': return E; case 'F': return F; case 'G': return G; case 'H': return H;
        case 'I': return I; case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O; case 'P': return P;
        case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
        case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X;
        case 'Y': return Y; case 'Z': return Z;
        case '0': return D0; case '1': return D1; case '2': return D2; case '3': return D3;
        case '4': return D4; case '5': return D5; case '6': return D6; case '7': return D7;
        case '8': return D8; case '9': return D9; case '.': return DOT;
        default:  return SP;
    }
}

struct Img {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    Img(int W, int H) : w(W), h(H), px((size_t)W * H, MakeRGB(240,240,240)) {}
    void Set(int x, int y, uint32_t c) {
        if (x >= 0 && y >= 0 && x < w && y < h) px[(size_t)y * w + x] = c;
    }
    void FillRect(int x, int y, int rw, int rh, uint32_t c) {
        for (int j = 0; j < rh; ++j) for (int i = 0; i < rw; ++i) Set(x + i, y + j, c);
    }
    void HLine(int x, int y, int len, int th, uint32_t c) { FillRect(x, y, len, th, c); }
    void VLine(int x, int y, int len, int th, uint32_t c) { FillRect(x, y, th, len, c); }
    void CircleOutline(int cx, int cy, int r, int th, uint32_t c) {
        for (int a = 0; a < 3600; ++a) {
            float rad = (float)a * 3.14159265f / 1800.0f;
            for (int t = 0; t < th; ++t) {
                int rr = r + t;
                Set(cx + (int)std::lround(std::cos(rad) * rr), cy + (int)std::lround(std::sin(rad) * rr), c);
            }
        }
    }
    void Text(int x, int y, int scale, uint32_t c, const std::string& s) {
        int cx = x;
        for (char ch : s) {
            const uint8_t* g = Glyph(ch);
            for (int ry = 0; ry < 7; ++ry)
                for (int rx = 0; rx < 5; ++rx)
                    if (g[ry] & (1 << (4 - rx)))
                        FillRect(cx + rx * scale, y + ry * scale, scale, scale, c);
            cx += (5 + 1) * scale;
        }
    }
};

// Calibration-only repeated reference tile.
//
// Every material preset is evaluated over the SAME content:
//   - fine grid
//   - text + digits
//   - high-contrast vertical/horizontal edges
//   - dark/light blocks
//   - red/blue color blocks
//
// This does NOT replace Generate(); the original 01-14 visual baseline remains
// unchanged.
inline void DrawMaterialCalibrationTile(
    Img& img,
    int x,
    int y,
    int w,
    int h)
{
    const uint32_t tileBg = MakeRGB(238, 240, 242);
    const uint32_t grid   = MakeRGB(175, 180, 186);
    const uint32_t ink    = MakeRGB(38, 40, 46);
    const uint32_t red    = MakeRGB(205, 58, 62);
    const uint32_t blue   = MakeRGB(58, 88, 198);

    img.FillRect(x, y, w, h, tileBg);

    for (int gx = 0; gx <= w; gx += 20)
        img.VLine(x + gx, y, h, 1, grid);

    for (int gy = 0; gy <= h; gy += 20)
        img.HLine(x, y + gy, w, 1, grid);

    img.Text(x + 10, y + 10, 2, ink, "GLASS");
    img.Text(x + 10, y + 31, 2, ink, "0123456789");

    const int crossX = x + w / 2;
    const int crossY = y + h / 2;

    img.VLine(crossX, y + 4, h - 8, 3, red);
    img.HLine(x + 4, crossY, w - 8, 3, blue);

    const int blockY = y + h - 38;
    const int blockW = 48;
    const int blockH = 28;
    const int gap = 8;
    const int blockX = x + 12;

    img.FillRect(
        blockX,
        blockY,
        blockW,
        blockH,
        MakeRGB(28, 30, 34));

    img.FillRect(
        blockX + (blockW + gap),
        blockY,
        blockW,
        blockH,
        MakeRGB(250, 250, 250));

    img.FillRect(
        blockX + (blockW + gap) * 2,
        blockY,
        blockW,
        blockH,
        red);

    img.FillRect(
        blockX + (blockW + gap) * 3,
        blockY,
        blockW,
        blockH,
        blue);
}

inline void GenerateMaterialCalibration(
    int W,
    int H,
    std::vector<uint32_t>& out)
{
    Img img(W, H);

    for (int y = 0; y < H; ++y) {
        int v = 244 - (y * 10) / (H > 0 ? H : 1);
        uint32_t c = MakeRGB(v, v, v);
        for (int x = 0; x < W; ++x)
            img.Set(x, y, c);
    }

    const int tileW = 260;
    const int tileH = 150;
    const int tileY = (int)((float)H * 0.30f);

    const int xs[3] = {
        (int)((float)W * 0.07f),
        (int)((float)W * 0.395f),
        (int)((float)W * 0.72f)
    };

    const char* labels[3] = {
        "CLEAR",
        "REGULAR",
        "THICK"
    };

    const uint32_t labelColor =
        MakeRGB(42, 44, 50);

    for (int i = 0; i < 3; ++i) {
        img.Text(
            xs[i] + 4,
            tileY - 38,
            3,
            labelColor,
            labels[i]);

        DrawMaterialCalibrationTile(
            img,
            xs[i],
            tileY,
            tileW,
            tileH);
    }

    out = std::move(img.px);
}

// Build the test bench image (bright/neutral, lots of whitespace).
inline void Generate(int W, int H, std::vector<uint32_t>& out) {
    Img img(W, H);

    // Base: light neutral gradient (bright, clean).
    for (int y = 0; y < H; ++y) {
        int v = 244 - (y * 14) / (H > 0 ? H : 1);   // 244 -> 230
        uint32_t c = MakeRGB(v, v, (int)(v * 0.99f));
        for (int x = 0; x < W; ++x) img.Set(x, y, c);
    }

    const int ink  = 40;
    const int inkL = 90;
    uint32_t dark  = MakeRGB(ink, ink, ink + 6);
    uint32_t soft  = MakeRGB(inkL, inkL, inkL + 6);

    int sc = std::max(3, W / 220);          // text scale
    int x0 = W / 12;
    int y0 = H / 10;

    // Typography zone.
    img.Text(x0, y0,        sc + 2, dark, "AURORAGLASS");
    img.Text(x0, y0 + 48,   sc,     soft, "LIQUID GLASS");
    img.Text(x0, y0 + 90,   sc,     soft, "ABCDEFGHIJKLM");
    img.Text(x0, y0 + 130,  sc,     soft, "0123456789");

    // Shapes zone (circle, square, triangle, star-like).
    int sy = y0 + 200;
    int cx = x0 + 40;
    img.CircleOutline(cx, sy + 40, 34, 3, dark);
    img.FillRect(x0 + 110, sy + 6, 68, 68, MakeRGB(70, 90, 140));
    // triangle
    for (int j = 0; j < 70; ++j) {
        int half = (j * 40) / 70;
        img.FillRect(x0 + 220 - half, sy + 6 + j, half * 2 + 1, 1, MakeRGB(140, 80, 60));
    }
    // star-like (4-point) via two rotated bars
    img.HLine(x0 + 300, sy + 38, 80, 8, MakeRGB(90, 120, 90));
    img.VLine(x0 + 336, sy + 2, 80, 8, MakeRGB(90, 120, 90));

    // Fine-line zone (small patch only).
    int fx = W / 2 + W / 12;
    int fy = y0 + 40;
    uint32_t line = MakeRGB(60, 60, 66);
    for (int i = 0; i < 10; ++i) img.HLine(fx, fy + i * 12, 240, 1, line);
    for (int i = 0; i < 16; ++i) img.VLine(fx + i * 16, fy, 108, 1, line);
    img.VLine(fx + 120, fy, 120, 2, MakeRGB(180, 60, 60));   // red cross center
    img.HLine(fx, fy + 54, 240, 2, MakeRGB(60, 60, 180));

    // Contrast blocks (light/dark) for chromatic/refraction edge reading.
    int bx = W / 2 + W / 12;
    int by = y0 + 200;
    img.FillRect(bx,        by, 90, 60, MakeRGB(30, 30, 34));      // dark
    img.FillRect(bx + 100,  by, 90, 60, MakeRGB(250, 250, 250));   // light
    img.FillRect(bx + 200,  by, 90, 60, MakeRGB(200, 60, 60));     // red
    img.FillRect(bx + 300,  by, 90, 60, MakeRGB(60, 90, 200));     // blue

    out = std::move(img.px);
}

} // namespace testbench
