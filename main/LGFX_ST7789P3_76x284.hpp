/*
 * LGFX_ST7789P3_76x284.hpp
 * ST7789P3 (76×284) 専用LGFXクラス定義
 * ランダムドット問題解決版 for M5StampPico
 */

#pragma once

#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

// 76×284専用オフセット調整値（ランダムドット対策）
constexpr int OFFSET_X = 82;  // X方向オフセット（左右調整）
constexpr int OFFSET_Y = 18;  // Y方向オフセット（上下調整：下20pxランダムドット対策）

// ST7789P3ピン定義（M5StampPico用）
constexpr int PIN_SCL = 18;  // SCLK (SPI Clock)
constexpr int PIN_SDA = 26;  // MOSI (SDA)
constexpr int PIN_RST = 22;  // Reset
constexpr int PIN_DC = 21;   // Data/Command
constexpr int PIN_CS = 19;   // Chip Select
constexpr int PIN_BLK = -1;  // Backlight - ハードウェア制御

/**
 * ST7789P3 (76×284) 専用LGFXクラス
 * 
 * 特徴：
 * - 76×284解像度専用最適化
 * - ランダムドット問題解決
 * - カスタム初期化シーケンス
 * - M5StampPico専用ピン設定
 */
class LGFX_ST7789P3_76x284 : public lgfx::LGFX_Device
{
private:
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    /**
     * コンストラクタ
     * SPI設定とパネル設定を行う
     */
    LGFX_ST7789P3_76x284(void);

    /**
     * ST7789P3 (76×284) 専用カスタム初期化
     * 標準初期化後に実行する追加設定
     */
    void performCustomInitialization();

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