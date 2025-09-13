/*
 * LGFX_ST7789P3_76x284.cpp (回転対応統合版)
 * ST7789P3 (76×284) 専用LGFXクラス実装
 * ランダムドット問題解決 + 回転対応 for M5StampPico
 */

// #include "LGFX_ST7789P3_76x284.hpp"
// #include "esp_log.h"

// ログタグ定義
static const char *TAG = "LGFX_ST7789P3";

// 回転角度別設定値の定義
const LGFX_ST7789P3_76x284::RotationConfig LGFX_ST7789P3_76x284::rotation_configs[4] = {
    // rotation=0: 76×284 (縦向き)
    {82, 18, 76, 284, 0x00, "Portrait (0°)"},
    
    // rotation=1: 284×76 (横向き、右回り90度)
    {18, 82, 284, 76, 0x60, "Landscape Right (90°)"},
    
    // rotation=2: 76×284 (縦向き反転、180度)
    {320-82-76, 320-18-284, 76, 284, 0xC0, "Portrait Flipped (180°)"},
    
    // rotation=3: 284×76 (横向き、左回り270度)
    {320-18-284, 320-82-76, 284, 76, 0xA0, "Landscape Left (270°)"}
};

/**
 * コンストラクタ：SPI設定とパネル設定
 */
LGFX_ST7789P3_76x284::LGFX_ST7789P3_76x284(void)
{
    ESP_LOGI(TAG, "Initializing LGFX_ST7789P3_76x284 class (rotation-aware)...");
    
    // SPIバス設定（76×284専用最適化）
    {
        auto cfg = _bus_instance.config();
        cfg.spi_host = HSPI_HOST;
        cfg.spi_mode = 0;                // SPI Mode 0
        cfg.freq_write = 20000000;       // 20MHz
        cfg.freq_read = 10000000;        // 10MHz
        cfg.spi_3wire = false;           // 4線式SPI
        cfg.use_lock = true;
        cfg.dma_channel = 1;
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
        
        // 76×284専用オフセット設定（デフォルト：rotation=0）
        cfg.offset_x = OFFSET_X;         // X方向オフセット
        cfg.offset_y = OFFSET_Y;         // Y方向オフセット
        cfg.offset_rotation = 0;         // 回転オフセット
        
        // 読み込み設定（安定動作優先）
        cfg.dummy_read_pixel = 8;        // ダミー読み込みピクセル
        cfg.dummy_read_bits = 1;         // ダミー読み込みビット
        cfg.readable = false;            // 読み込み無効（書き込み専用）
        
        // ST7789P3専用カラー設定
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
 * 回転対応初期化（推奨メソッド）
 */
void LGFX_ST7789P3_76x284::initWithRotation(int rotation)
{
    ESP_LOGI(TAG, "=== ST7789P3 Rotation-Aware Initialization ===");
    
    if (rotation < 0 || rotation > 3) {
        ESP_LOGE(TAG, "Invalid rotation: %d. Using rotation 0.", rotation);
        rotation = 0;
    }
    
    _current_rotation = rotation;
    const auto& config = rotation_configs[rotation];
    
    ESP_LOGI(TAG, "Target rotation: %d - %s", rotation, config.name);
    ESP_LOGI(TAG, "Expected resolution: %dx%d", config.width, config.height);
    ESP_LOGI(TAG, "Using offsets: X=%d, Y=%d", config.offset_x, config.offset_y);
    
    // 基本初期化
    init();
    
    // 回転設定
    setRotation(rotation);
    ESP_LOGI(TAG, "Rotation set to %d, reported size: %ldx%ld", rotation, width(), height());
    
    // 回転対応のカスタム初期化実行
    performRotationAwareInitialization(rotation);
    
    ESP_LOGI(TAG, "Final resolution: %ldx%ld", width(), height());
    ESP_LOGI(TAG, "=== Rotation-Aware Initialization Complete ===");
}

/**
 * 従来互換のカスタム初期化（rotation=0専用）
 */
void LGFX_ST7789P3_76x284::performCustomInitialization()
{
    ESP_LOGI(TAG, "=== Legacy Custom Initialization (rotation=0) ===");
    performRotationAwareInitialization(0);
}

/**
 * 回転対応カスタム初期化
 */
void LGFX_ST7789P3_76x284::performRotationAwareInitialization(int rotation)
{
    ESP_LOGI(TAG, "=== Starting Rotation-Aware Custom Initialization ===");
    
    if (rotation < 0 || rotation > 3) rotation = 0;
    
    const auto& config = rotation_configs[rotation];
    _current_rotation = rotation;
    
    startWrite();
    
    // Memory Data Access Control (MADCTL) - 回転に応じて設定
    ESP_LOGI(TAG, "Setting MADCTL for rotation %d...", rotation);
    writeCommand(0x36);  // MADCTL
    writeData(config.madctl);
    ESP_LOGI(TAG, "✓ MADCTL set to 0x%02X", config.madctl);
    
    // Color Mode - 16bit RGB565
    ESP_LOGI(TAG, "Setting Color Mode...");
    writeCommand(0x3A);  // COLMOD
    writeData(0x05);     // 16-bit/pixel
    ESP_LOGI(TAG, "✓ COLMOD set to RGB565");
    
    // Column Address Set - 回転対応
    ESP_LOGI(TAG, "Setting Column Address (CASET) for rotation %d...", rotation);
    uint16_t x_start = config.offset_x;
    uint16_t x_end = x_start + config.width - 1;
    
    writeCommand(0x2A);  // CASET
    writeData(x_start >> 8);
    writeData(x_start & 0xFF);
    writeData(x_end >> 8);
    writeData(x_end & 0xFF);
    ESP_LOGI(TAG, "✓ CASET set to 0x%04X-0x%04X (%d-%d, width=%d)", 
             x_start, x_end, x_start, x_end, config.width);
    
    // Row Address Set - 回転対応
    ESP_LOGI(TAG, "Setting Row Address (RASET) for rotation %d...", rotation);
    uint16_t y_start = config.offset_y;
    uint16_t y_end = y_start + config.height - 1;
    
    writeCommand(0x2B);  // RASET
    writeData(y_start >> 8);
    writeData(y_start & 0xFF);
    writeData(y_end >> 8);
    writeData(y_end & 0xFF);
    ESP_LOGI(TAG, "✓ RASET set to 0x%04X-0x%04X (%d-%d, height=%d)", 
             y_start, y_end, y_start, y_end, config.height);
    
    // その他のST7789P3設定
    setupST7789P3Registers();
    
    endWrite();
    
    ESP_LOGI(TAG, "=== Rotation-Aware Custom Initialization Complete ===");
}

/**
 * ST7789P3レジスタ設定（回転非依存）
 */
void LGFX_ST7789P3_76x284::setupST7789P3Registers()
{
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
    lgfx::v1::delay(120);  // 安定化待ち
    
    ESP_LOGI(TAG, "✓ ST7789P3 display turned on");
}

/**
 * 現在の回転角度名を取得
 */
const char* LGFX_ST7789P3_76x284::getCurrentRotationName() const
{
    if (_current_rotation >= 0 && _current_rotation <= 3) {
        return rotation_configs[_current_rotation].name;
    }
    return "Unknown";
}

/*
使用方法：

1. app_main.cppでの修正例：

// 従来のコード（縦向き）
void initST7789P3_old() {
    tft.init();
    tft.setRotation(0);
    tft.performCustomInitialization();
}

// 新しいコード（横向き）
void initST7789P3() {
    tft.initWithRotation(1);  // 横向きで初期化
    ESP_LOGI(TAG, "Display mode: %s", tft.getCurrentRotationName());
}

2. パレット画像システムとの組み合わせ：

PaletteImageRenderer renderer(&tft, tft.width(), tft.height());

3. 回転角度：
- 0: 76×284 (縦向き) - 従来通り
- 1: 284×76 (横向き、右回り) - 推奨
- 2: 76×284 (縦向き反転)
- 3: 284×76 (横向き、左回り)

互換性：
- 既存のperformCustomInitialization()はrotation=0で動作
- initWithRotation()を使えば任意の回転角度で初期化可能
*/