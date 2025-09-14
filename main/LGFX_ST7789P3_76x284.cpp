/*
 * LGFX_ST7789P3_76x284.cpp (rotation=3専用シンプル版)
 * ST7789P3 左回り270度回転専用
 */

#include "LGFX_ST7789P3_76x284.hpp"
#include "esp_log.h"

static const char *TAG = "LGFX_ST7789P3";

/**
 * コンストラクタ：rotation=3専用設定
 */
LGFX_ST7789P3_76x284::LGFX_ST7789P3_76x284(void)
{
    ESP_LOGI(TAG, "Initializing LGFX_ST7789P3_76x284 (rotation=3 only)...");

    // SPIバス設定
    {
        auto cfg = _bus_instance.config();
        cfg.spi_host = HSPI_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 20000000;
        cfg.freq_read = 10000000;
        cfg.spi_3wire = false;
        cfg.use_lock = true;
        cfg.dma_channel = 1;
        cfg.pin_sclk = PIN_SCL;
        cfg.pin_mosi = PIN_SDA;
        cfg.pin_miso = -1;
        cfg.pin_dc = PIN_DC;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);

        ESP_LOGI(TAG, "SPI bus configured");
    }

    // パネル設定（rotation=3専用）
    {
        auto cfg = _panel_instance.config();

        cfg.pin_cs = PIN_CS;
        cfg.pin_rst = PIN_RST;
        cfg.pin_busy = -1;

        // rotation=3用解像度設定
        cfg.memory_width = 320;
        cfg.memory_height = 320;
        cfg.panel_width = 284; // rotation=3では横284
        cfg.panel_height = 76; // rotation=3では縦76

        // オフセット設定（この値がOFFSET_X, OFFSET_Yで直接調整される）
        cfg.offset_x = OFFSET_X;
        cfg.offset_y = OFFSET_Y;
        cfg.offset_rotation = 3; // rotation=3固定

        // その他設定
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = false;
        cfg.invert = false;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = true;

        _panel_instance.config(cfg);

        ESP_LOGI(TAG, "Panel configured for rotation=3: %dx%d, offset=(%d,%d)",
                 cfg.panel_width, cfg.panel_height, cfg.offset_x, cfg.offset_y);
    }

    setPanel(&_panel_instance);
    ESP_LOGI(TAG, "LGFX_ST7789P3_76x284 initialization complete");
}

/**
 * rotation=3専用初期化
 */
void LGFX_ST7789P3_76x284::init()
{
    ESP_LOGI(TAG, "=== Starting rotation=3 initialization ===");

    // 基本初期化
    lgfx::LGFX_Device::init();

    // rotation=3設定（左回り270度）
    setRotation(1);
    ESP_LOGI(TAG, "Rotation set to 3, display size: %ldx%ld", width(), height());

    // ST7789P3ハードウェア設定
    startWrite();

    // MADCTL設定（rotation=3用：MV=1, MX=1, MY=1）
    ESP_LOGI(TAG, "Setting MADCTL for rotation=1 (0x20)...");
    writeCommand(0x36); // MADCTL
    writeData(0x60);    // MV=1のみ（右回り90度）

    // Color Mode設定
    ESP_LOGI(TAG, "Setting Color Mode to RGB565...");
    writeCommand(0x3A); // COLMOD
    writeData(0x05);    // 16-bit/pixel

    // 表示領域設定（OFFSET_X, OFFSET_Yを直接使用）
    ESP_LOGI(TAG, "Setting display area with offset X=%d, Y=%d...", OFFSET_X, OFFSET_Y);

    // Column Address Set (CASET)
    uint16_t x_start = OFFSET_X;
    uint16_t x_end = x_start + 284 - 1; // rotation=3では幅284

    writeCommand(0x2A); // CASET
    writeData(x_start >> 8);
    writeData(x_start & 0xFF);
    writeData(x_end >> 8);
    writeData(x_end & 0xFF);
    ESP_LOGI(TAG, "CASET: %d-%d (width=284)", x_start, x_end);

    // Row Address Set (RASET)
    uint16_t y_start = OFFSET_Y;
    uint16_t y_end = y_start + 76 - 1; // rotation=3では高さ76

    writeCommand(0x2B); // RASET
    writeData(y_start >> 8);
    writeData(y_start & 0xFF);
    writeData(y_end >> 8);
    writeData(y_end & 0xFF);
    ESP_LOGI(TAG, "RASET: %d-%d (height=76)", y_start, y_end);

    // その他のST7789P3設定

    // Porch Setting
    writeCommand(0xB2); // PORCTRL
    writeData(0x0C);
    writeData(0x0C);
    writeData(0x00);
    writeData(0x33);
    writeData(0x33);

    // Gate Control
    writeCommand(0xB7); // GCTRL
    writeData(0x75);

    // VCOM Setting
    writeCommand(0xBB); // VCOMS
    writeData(0x28);

    // Power Control
    writeCommand(0xC2); // PWCTRL1
    writeData(0x01);
    writeCommand(0xC3); // PWCTRL2
    writeData(0x19);
    writeCommand(0xC4); // PWCTRL3
    writeData(0x20);
    writeCommand(0xC6); // PWCTRL4
    writeData(0x0F);

    // Gamma Setting
    writeCommand(0xE0); // PVGAMCTRL
    writeData(0xD0);
    writeData(0x00);
    writeData(0x02);
    writeData(0x07);
    writeData(0x0A);
    writeData(0x28);
    writeData(0x32);
    writeData(0x44);
    writeData(0x42);
    writeData(0x06);
    writeData(0x0E);
    writeData(0x12);
    writeData(0x14);
    writeData(0x17);

    writeCommand(0xE1); // NVGAMCTRL
    writeData(0xD0);
    writeData(0x00);
    writeData(0x02);
    writeData(0x07);
    writeData(0x0A);
    writeData(0x28);
    writeData(0x31);
    writeData(0x54);
    writeData(0x47);
    writeData(0x0E);
    writeData(0x1C);
    writeData(0x17);
    writeData(0x1B);
    writeData(0x1E);

    // Display On
    ESP_LOGI(TAG, "Enabling display...");
    writeCommand(0x20); // INVOFF
    writeCommand(0x13); // NORON
    writeCommand(0x29); // DISPON
    lgfx::v1::delay(120);

    endWrite();

    ESP_LOGI(TAG, "=== rotation=3 initialization complete ===");
    ESP_LOGI(TAG, "Final display size: %ldx%ld", width(), height());
    ESP_LOGI(TAG, "Offset values used: X=%d, Y=%d", OFFSET_X, OFFSET_Y);
}