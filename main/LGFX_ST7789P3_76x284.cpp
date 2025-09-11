/*
 * LGFX_ST7789P3_76x284.cpp
 * ST7789P3 (76×284) 専用LGFXクラス実装
 * ランダムドット問題解決版 for M5StampPico
 */

#include "LGFX_ST7789P3_76x284.hpp"
#include "esp_log.h"

// ログタグ定義
static const char *TAG = "LGFX_ST7789P3";

/**
 * コンストラクタ：SPI設定とパネル設定
 */
LGFX_ST7789P3_76x284::LGFX_ST7789P3_76x284(void)
{
    ESP_LOGI(TAG, "Initializing LGFX_ST7789P3_76x284 class...");
    
    // SPIバス設定（76×284専用最適化）
    {
        auto cfg = _bus_instance.config();
        cfg.spi_host = HSPI_HOST;
        cfg.spi_mode = 0;                // SPI Mode 0
        cfg.freq_write = 20000000;       // 20MHz
        cfg.freq_read = 10000000;        // 10MHz
        cfg.spi_3wire = false;           // 4線式SPI
        cfg.use_lock = true;
        cfg.dma_channel = 1; // Use DMA channel 1, or set to -1 to disable DMA
        cfg.pin_sclk = PIN_SCL;
        cfg.pin_mosi = PIN_SDA;
        cfg.pin_miso = -1;
        cfg.pin_dc = PIN_DC;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
        
        ESP_LOGI(TAG, "SPI bus configured: SCLK=%d, MOSI=%d, DC=%d", PIN_SCL, PIN_SDA, PIN_DC);
    }

    // ST7789P3 (76×284) 専用パネル設定
    {
        auto cfg = _panel_instance.config();
        
        // 基本ピン設定
        cfg.pin_cs = PIN_CS;
        cfg.pin_rst = PIN_RST;
        cfg.pin_busy = -1;
        
        // 76×284専用解像度設定（重要！）
        cfg.memory_width = 320;          // ST7789の標準メモリ幅
        cfg.memory_height = 320;         // ST7789の標準メモリ高さ
        cfg.panel_width = 76;            // 実際のパネル表示幅
        cfg.panel_height = 284;          // 実際のパネル表示高さ
        
        // 76×284専用オフセット設定（ST7789P3の特殊配置に対応）
        cfg.offset_x = OFFSET_X;         // X方向オフセット
        cfg.offset_y = OFFSET_Y;         // Y方向オフセット（下20pxランダムドット対策）
        cfg.offset_rotation = 0;         // 回転オフセット
        
        // 読み込み設定（安定動作優先）
        cfg.dummy_read_pixel = 8;        // ダミー読み込みピクセル
        cfg.dummy_read_bits = 1;         // ダミー読み込みビット
        cfg.readable = false;            // 読み込み無効（書き込み専用）
        
        // ST7789P3 (76×284)専用カラー設定
        cfg.invert = false;              // 色反転無し
        cfg.rgb_order = false;           // RGB順序（false=RGB, true=BGR）
        cfg.dlen_16bit = false;          // 16bit長無し
        cfg.bus_shared = true;
        
        _panel_instance.config(cfg);
        
        ESP_LOGI(TAG, "Panel configured: %dx%d display, offset=(%d,%d)", 
                 cfg.panel_width, cfg.panel_height, cfg.offset_x, cfg.offset_y);
    }

    setPanel(&_panel_instance);
    ESP_LOGI(TAG, "LGFX_ST7789P3_76x284 class initialization complete");
}

/**
 * ST7789P3 (76×284) 専用カスタム初期化
 * 標準初期化後に実行する追加設定
 */
void LGFX_ST7789P3_76x284::performCustomInitialization()
{
    ESP_LOGI(TAG, "=== Starting ST7789P3 (76×284) Custom Initialization ===");
    
    startWrite();
    
    // Memory Data Access Control (MADCTL) - 76×284専用設定
    ESP_LOGI(TAG, "Setting MADCTL (Memory Data Access Control)...");
    writeCommand(0x36);  // MADCTL
    writeData(0x00);     // MY=0, MX=0, MV=0, ML=0, RGB=0, MH=0
    ESP_LOGI(TAG, "✓ MADCTL set to 0x00 (Normal orientation, RGB order)");
    
    // Color Mode - 16bit RGB565確認
    ESP_LOGI(TAG, "Setting Color Mode...");
    writeCommand(0x3A);  // COLMOD (Interface Pixel Format)
    writeData(0x05);     // 16-bit/pixel (RGB565)
    ESP_LOGI(TAG, "✓ COLMOD set to 0x05 (16-bit RGB565)");
    
    // Column Address Set - 76×284専用（重要！）
    ESP_LOGI(TAG, "Setting Column Address (CASET)...");
    uint16_t x_start = OFFSET_X;       // オフセットX
    uint16_t x_end = x_start + 75;     // 76ピクセル分
    writeCommand(0x2A);  // CASET
    writeData(x_start >> 8);         // XS[15:8]
    writeData(x_start & 0xFF);       // XS[7:0]
    writeData(x_end >> 8);           // XE[15:8] 
    writeData(x_end & 0xFF);         // XE[7:0]
    ESP_LOGI(TAG, "✓ CASET set to 0x%04X-0x%04X (%d-%d, width=76)", x_start, x_end, x_start, x_end);
    
    // Row Address Set - 76×284専用（重要！）
    ESP_LOGI(TAG, "Setting Row Address (RASET)...");
    uint16_t y_start = OFFSET_Y;       // オフセットY（下20pxランダムドット対策）
    uint16_t y_end = y_start + 283;    // 284ピクセル分
    writeCommand(0x2B);  // RASET
    writeData(y_start >> 8);         // YS[15:8]
    writeData(y_start & 0xFF);       // YS[7:0]
    writeData(y_end >> 8);           // YE[15:8]
    writeData(y_end & 0xFF);         // YE[7:0]
    ESP_LOGI(TAG, "✓ RASET set to 0x%04X-0x%04X (%d-%d, height=284)", y_start, y_end, y_start, y_end);
    
    // Porch Setting - ST7789P3最適化
    ESP_LOGI(TAG, "Setting Porch Control...");
    writeCommand(0xB2);  // PORCTRL
    writeData(0x0C);     // Back porch (normal)
    writeData(0x0C);     // Front porch (normal)
    writeData(0x00);     // Separate porch disable
    writeData(0x33);     // Back porch (idle)
    writeData(0x33);     // Front porch (idle)
    ESP_LOGI(TAG, "✓ Porch control configured");
    
    // Gate Control - ST7789P3専用
    ESP_LOGI(TAG, "Setting Gate Control...");
    writeCommand(0xB7);  // GCTRL
    writeData(0x35);     // VGH/VGL設定
    ESP_LOGI(TAG, "✓ Gate control configured");
    
    // VCOM Setting - ST7789P3最適化
    ESP_LOGI(TAG, "Setting VCOM...");
    writeCommand(0xBB);  // VCOMS
    writeData(0x19);     // VCOM = 1.35V
    ESP_LOGI(TAG, "✓ VCOM configured");
    
    // LCM Control
    ESP_LOGI(TAG, "Setting LCM Control...");
    writeCommand(0xC0);  // LCMCTRL
    writeData(0x2C);     // LCM control
    ESP_LOGI(TAG, "✓ LCM control configured");
    
    // VRH Enable
    ESP_LOGI(TAG, "Enabling VRH...");
    writeCommand(0xC2);  // VRHEN
    writeData(0x01);     // VRH command enable
    ESP_LOGI(TAG, "✓ VRH enabled");
    
    // VRH Set
    ESP_LOGI(TAG, "Setting VRH...");
    writeCommand(0xC3);  // VRHS
    writeData(0x12);     // VRH = 4.45V
    ESP_LOGI(TAG, "✓ VRH configured");
    
    // VDVS Set
    ESP_LOGI(TAG, "Setting VDVS...");
    writeCommand(0xC4);  // VDVSET
    writeData(0x20);     // VDVS = 0V
    ESP_LOGI(TAG, "✓ VDVS configured");
    
    // Frame Rate Control
    ESP_LOGI(TAG, "Setting Frame Rate...");
    writeCommand(0xC6);  // FRCTRL2
    writeData(0x0F);     // 60Hz in normal mode
    ESP_LOGI(TAG, "✓ Frame rate set to 60Hz");
    
    // Power Control 1
    ESP_LOGI(TAG, "Setting Power Control...");
    writeCommand(0xD0);  // PWCTRL1
    writeData(0xA4);     // Power control settings
    writeData(0xA1);     // Power control settings
    ESP_LOGI(TAG, "✓ Power control configured");
    
    // Positive Voltage Gamma Control
    ESP_LOGI(TAG, "Setting Positive Gamma...");
    writeCommand(0xE0);  // PVGAMCTRL
    writeData(0xD0); writeData(0x04); writeData(0x0D); writeData(0x11);
    writeData(0x13); writeData(0x2B); writeData(0x3F); writeData(0x54);
    writeData(0x4C); writeData(0x18); writeData(0x0D); writeData(0x0B);
    writeData(0x1F); writeData(0x23);
    ESP_LOGI(TAG, "✓ Positive gamma configured");
    
    // Negative Voltage Gamma Control
    ESP_LOGI(TAG, "Setting Negative Gamma...");
    writeCommand(0xE1);  // NVGAMCTRL
    writeData(0xD0); writeData(0x04); writeData(0x0C); writeData(0x11);
    writeData(0x13); writeData(0x2C); writeData(0x3F); writeData(0x44);
    writeData(0x51); writeData(0x2F); writeData(0x1F); writeData(0x1F);
    writeData(0x20); writeData(0x23);
    ESP_LOGI(TAG, "✓ Negative gamma configured");
    
    // Display Inversion Off
    ESP_LOGI(TAG, "Disabling Display Inversion...");
    writeCommand(0x20);  // INVOFF
    ESP_LOGI(TAG, "✓ Display inversion disabled");
    
    // Normal Display Mode On
    ESP_LOGI(TAG, "Enabling Normal Display Mode...");
    writeCommand(0x13);  // NORON
    ESP_LOGI(TAG, "✓ Normal display mode enabled");
    
    // Display On
    ESP_LOGI(TAG, "Turning Display On...");
    writeCommand(0x29);  // DISPON
    lgfx::v1::delay(120);  // 正しいdelay関数を使用
    
    endWrite();
    
    ESP_LOGI(TAG, "=== ST7789P3 (76×284) Custom Initialization Complete! ===");
    ESP_LOGI(TAG, "Display resolution: %ldx%ld", width(), height());
}