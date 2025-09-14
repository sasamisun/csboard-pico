/*
 * LGFX_ST7789P3_76x284.hpp (rotation=3専用シンプル版)
 * ST7789P3 左回り270度回転専用
 */

#pragma once

#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

// rotation=3専用オフセット調整値（この値だけを調整すれば位置が変わる）
constexpr int OFFSET_X = 18;  // X方向オフセット（左右調整）
constexpr int OFFSET_Y = 82;  // Y方向オフセット（上下調整）

// ST7789P3ピン定義（M5StampPico用）
constexpr int PIN_SCL = 18;  // SCLK (SPI Clock)
constexpr int PIN_SDA = 26;  // MOSI (SDA)
constexpr int PIN_RST = 22;  // Reset
constexpr int PIN_DC = 21;   // Data/Command
constexpr int PIN_CS = 19;   // Chip Select
constexpr int PIN_BLK = -1;  // Backlight - ハードウェア制御

/**
 * ST7789P3 (76×284) rotation=3専用LGFXクラス
 * 左回り270度固定、284×76表示
 */
class LGFX_ST7789P3_76x284 : public lgfx::LGFX_Device
{
private:
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    /**
     * コンストラクタ
     * rotation=3専用設定
     */
    LGFX_ST7789P3_76x284(void);

    /**
     * rotation=3専用初期化
     */
    void init();

    /**
     * 設定値取得関数群
     */
    static constexpr int getOffsetX() { return OFFSET_X; }
    static constexpr int getOffsetY() { return OFFSET_Y; }
    static constexpr int getPinSCL() { return PIN_SCL; }
    static constexpr int getPinSDA() { return PIN_SDA; }
    static constexpr int getPinRST() { return PIN_RST; }
    static constexpr int getPinDC() { return PIN_DC; }
    static constexpr int getPinCS() { return PIN_CS; }
    static constexpr int getPinBLK() { return PIN_BLK; }
};