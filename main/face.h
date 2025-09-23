/*
 * Auto-generated from .\face.png
 * Original size: 12x12
 * Final size: 12x12
 * Palette: classic
 * Dithering: OFF
 * Color space: lab
 * Variable name: face
 * 
 * M5StampPico 16-Color Palette Image Tool (Improved Version)
 */

#pragma once
#include "RetroGamePaletteImage.hpp"

// Image: 12x12 pixels, 16-color palette
// Generated data size: 72 bytes
// Memory efficiency: 75.0% saving vs 16-bit

// 画像サイズ情報
const uint16_t face_width = 12;
const uint16_t face_height = 12;

// 画像データ配列（1バイトに2ピクセル格納）
const uint8_t face_data[72] = {
    0x00, 0x0F, 0x00, 0x00, 0xF0, 0x00, 0xF0, 0xF1, 0x00, 0x00, 0x1F, 0x0F, 0xF0, 0x11, 0x0F, 0xF0,
    0x11, 0x0F, 0x1F, 0x11, 0xF1, 0x1F, 0x11, 0xF1, 0x1F, 0xF1, 0x11, 0x11, 0x1F, 0xF1, 0x1F, 0x11,
    0x11, 0x11, 0x11, 0xF1, 0x1F, 0x11, 0x11, 0x11, 0x11, 0xF1, 0xF0, 0x11, 0xF1, 0x1F, 0x11, 0x0F,
    0xF0, 0x11, 0x11, 0x11, 0x11, 0x0F, 0x00, 0x1F, 0x11, 0x11, 0xF1, 0x00, 0x00, 0xF0, 0xFF, 0xFF,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 便利なマクロ定義
#define FACE_WIDTH  12
#define FACE_HEIGHT 12
#define FACE_SIZE   72

// 使用例:
// PaletteImageData myImage(face_data, face_width, face_height);
// または
// PaletteImageData myImage(face_data, FACE_WIDTH, FACE_HEIGHT);
// renderer.drawToCanvas(myImage, x, y, true);

// Classic カラーパレット定義
// パレット初期化関数
void face_palette_init(RetroColorPalette& palette) {
    palette.setColor(0, 0xFFFF); // 透明色
    palette.setColor(1, 0xF79E); // RGB(240,240,240)
    palette.setColor(2, 0xF008); // RGB(240,0,64)
    palette.setColor(3, 0x001F); // RGB(0,0,248)
    palette.setColor(4, 0x2C8A); // RGB(40,144,80)
    palette.setColor(5, 0xCEA6); // RGB(200,212,48)
    palette.setColor(6, 0xEB90); // RGB(232,112,128)
    palette.setColor(7, 0x0CD8); // RGB(8,152,192)
    palette.setColor(8, 0xF3A0); // RGB(240,116,0)
    palette.setColor(9, 0x8000); // RGB(128,0,0)
    palette.setColor(10, 0x01C0); // RGB(0,56,0)
    palette.setColor(11, 0x0014); // RGB(0,0,160)
    palette.setColor(12, 0x7A27); // RGB(120,68,56)
    palette.setColor(13, 0x7BEF); // RGB(120,124,120)
    palette.setColor(14, 0x3908); // RGB(56,32,64)
    palette.setColor(15, 0x18C3); // RGB(24,24,24)
}

// パレット配列定義（RGB565形式）
const uint16_t face_palette[16] = {
    0xFFFF, 0xF79E, 0xF008, 0x001F,  // 透明, RGB(240,240,240), RGB(240,0,64), RGB(0,0,248)
    0x2C8A, 0xCEA6, 0xEB90, 0x0CD8,  // RGB(40,144,80), RGB(200,212,48), RGB(232,112,128), RGB(8,152,192)
    0xF3A0, 0x8000, 0x01C0, 0x0014,  // RGB(240,116,0), RGB(128,0,0), RGB(0,56,0), RGB(0,0,160)
    0x7A27, 0x7BEF, 0x3908, 0x18C3  // RGB(120,68,56), RGB(120,124,120), RGB(56,32,64), RGB(24,24,24)
};