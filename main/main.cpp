/*
 * M5StampPico with ST7789P3 Display (76×284) - 完全独自制御版
 * ESP32-PICO-D4 + ST7789P3 生SPI制御版
 * 
 * 🔧 完全独自制御内容:
 * 1. M5GFXライブラリを使わず生のESP-IDF SPI制御
 * 2. ST7789P3専用初期化シーケンス完全実装
 * 3. バックライトGPIO25直接制御
 * 4. 76×284表示領域の正確なメモリマッピング
 * 5. 段階的デバッグ機能付き
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

// ログタグ定義
static const char *TAG = "ST7789P3_RAW_CONTROL";

// 🔧 ピン定義
#define PIN_SCL     18  // SPI Clock
#define PIN_SDA     26  // SPI MOSI
#define PIN_RST     22  // Reset
#define PIN_DC      21  // Data/Command
#define PIN_CS      19  // Chip Select  
//#define PIN_BLK     25  // Backlight

// ST7789P3専用コマンド定義
#define ST7789P3_SWRESET    0x01  // Software Reset
#define ST7789P3_SLPOUT     0x11  // Sleep Out
#define ST7789P3_NORON      0x13  // Normal Display Mode On
#define ST7789P3_INVOFF     0x20  // Display Inversion Off
#define ST7789P3_INVON      0x21  // Display Inversion On
#define ST7789P3_DISPOFF    0x28  // Display Off
#define ST7789P3_DISPON     0x29  // Display On
#define ST7789P3_CASET      0x2A  // Column Address Set
#define ST7789P3_RASET      0x2B  // Row Address Set
#define ST7789P3_RAMWR      0x2C  // Memory Write
#define ST7789P3_MADCTL     0x36  // Memory Access Control
#define ST7789P3_COLMOD     0x3A  // Interface Pixel Format

// 表示サイズ定義
#define LCD_WIDTH   76
#define LCD_HEIGHT  284
#define MEMORY_WIDTH  240
#define MEMORY_HEIGHT 320
#define OFFSET_X    ((MEMORY_WIDTH - LCD_WIDTH) / 2)   // 82
#define OFFSET_Y    ((MEMORY_HEIGHT - LCD_HEIGHT) / 2) // 18

// SPIハンドル
static spi_device_handle_t spi_handle;

// カラーパレット
const uint16_t colors[] = {
    0xF800, // 赤
    0x07E0, // 緑  
    0x001F, // 青
    0xFFE0, // 黄色
    0xF81F, // マゼンタ
    0x07FF, // シアン
    0xFFFF, // 白
    0xFD20  // オレンジ
};

// 🔧 GPIO初期化
void init_gpio()
{
    ESP_LOGI(TAG, "🔧 Initializing GPIO pins...");
    
    // Reset pin
    gpio_set_direction((gpio_num_t)PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_RST, 1);
    
    // DC pin  
    gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_DC, 0);
    
    // CS pin
    gpio_set_direction((gpio_num_t)PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_CS, 1);
    
    // Backlight pin
    //gpio_set_direction((gpio_num_t)PIN_BLK, GPIO_MODE_OUTPUT);
    //gpio_set_level((gpio_num_t)PIN_BLK, 0); // 初期は消灯
    
    ESP_LOGI(TAG, "✅ GPIO initialization completed");
}

// 🔧 SPI初期化
void init_spi()
{
    ESP_LOGI(TAG, "🔧 Initializing SPI interface...");
    
    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SDA,
        .miso_io_num = -1,  // 未使用
        .sclk_io_num = PIN_SCL,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0
    };
    
    // SPI device configuration
    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,  // SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 8000000,  // 8MHz (安定性重視)
        .input_delay_ns = 0,
        .spics_io_num = PIN_CS,
        .flags = 0,
        .queue_size = 7,
        .pre_cb = NULL,
        .post_cb = NULL
    };
    
    // SPI bus initialize
    esp_err_t ret = spi_bus_initialize(HSPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SPI bus initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // SPI device add
    ret = spi_bus_add_device(HSPI_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SPI device add failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "✅ SPI initialization completed");
}

// 🔧 コマンド送信
void send_command(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)PIN_DC, 0);  // Command mode
    
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
        .rx_buffer = NULL
    };
    
    spi_device_transmit(spi_handle, &trans);
}

// 🔧 データ送信
void send_data(uint8_t data)
{
    gpio_set_level((gpio_num_t)PIN_DC, 1);  // Data mode
    
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &data,
        .rx_buffer = NULL
    };
    
    spi_device_transmit(spi_handle, &trans);
}

// 🔧 マルチバイトデータ送信
void send_data_multi(uint8_t* data, size_t len)
{
    gpio_set_level((gpio_num_t)PIN_DC, 1);  // Data mode
    
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL
    };
    
    spi_device_transmit(spi_handle, &trans);
}

// 🔧 ハードウェアリセット
void hardware_reset()
{
    ESP_LOGI(TAG, "🔄 Hardware reset...");
    gpio_set_level((gpio_num_t)PIN_RST, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level((gpio_num_t)PIN_RST, 1);
    vTaskDelay(120 / portTICK_PERIOD_MS);  // 120ms待機
    ESP_LOGI(TAG, "✅ Hardware reset completed");
}

// 🔧 ST7789P3専用初期化シーケンス
void init_st7789p3()
{
    ESP_LOGI(TAG, "🚀 Starting ST7789P3 initialization sequence...");
    
    // Step 1: Software Reset
    ESP_LOGI(TAG, "📡 Step 1: Software Reset (0x01)");
    send_command(ST7789P3_SWRESET);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    
    // Step 2: Sleep Out (最重要!)
    ESP_LOGI(TAG, "📡 Step 2: Sleep Out (0x11) - CRITICAL!");
    send_command(ST7789P3_SLPOUT);
    vTaskDelay(120 / portTICK_PERIOD_MS);  // 仕様通り120ms待機
    
    // Step 3: Normal Display Mode On
    ESP_LOGI(TAG, "📡 Step 3: Normal Display Mode On (0x13)");
    send_command(ST7789P3_NORON);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 4: Display Inversion Off
    ESP_LOGI(TAG, "📡 Step 4: Display Inversion Off (0x20)");
    send_command(ST7789P3_INVOFF);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 5: Interface Pixel Format (16-bit RGB565)
    ESP_LOGI(TAG, "📡 Step 5: Interface Pixel Format (0x3A)");
    send_command(ST7789P3_COLMOD);
    send_data(0x55);  // 16-bit RGB565
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 6: Memory Access Control
    ESP_LOGI(TAG, "📡 Step 6: Memory Access Control (0x36)");
    send_command(ST7789P3_MADCTL);
    send_data(0x00);  // 標準設定
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 7: Column Address Set (76x284領域設定)
    ESP_LOGI(TAG, "📡 Step 7: Column Address Set (0x2A)");
    send_command(ST7789P3_CASET);
    send_data(0x00);
    send_data(OFFSET_X);  // Start X = 82
    send_data(0x00);
    send_data(OFFSET_X + LCD_WIDTH - 1);  // End X = 157
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 8: Row Address Set (76x284領域設定)
    ESP_LOGI(TAG, "📡 Step 8: Row Address Set (0x2B)");
    send_command(ST7789P3_RASET);
    send_data(0x00);
    send_data(OFFSET_Y);  // Start Y = 18
    send_data(0x01);
    send_data((OFFSET_Y + LCD_HEIGHT - 1) & 0xFF);  // End Y = 301
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Step 9: Display On (最重要!)
    ESP_LOGI(TAG, "📡 Step 9: Display On (0x29) - CRITICAL!");
    send_command(ST7789P3_DISPON);
    vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms待機
    
    ESP_LOGI(TAG, "🎯 ST7789P3 initialization sequence completed!");
    ESP_LOGI(TAG, "📺 *** SCREEN SHOULD NOW BE READY FOR DISPLAY! ***");
    vTaskDelay(500 / portTICK_PERIOD_MS);  // 少し待機して確認
}

// 🔧 バックライト制御
/*
void set_backlight(bool on)
{
    gpio_set_level((gpio_num_t)PIN_BLK, on ? 1 : 0);
    ESP_LOGI(TAG, "💡 Backlight: %s", on ? "ON" : "OFF");
}
*/

// 🔧 画面全体を単色で塗りつぶし
void fill_screen(uint16_t color)
{
    ESP_LOGI(TAG, "🎨 Filling screen with color 0x%04X", color);
    
    // Memory Write command
    send_command(ST7789P3_RAMWR);
    
    // 色データをビッグエンディアンに変換
    uint8_t color_bytes[2];
    color_bytes[0] = (color >> 8) & 0xFF;  // High byte
    color_bytes[1] = color & 0xFF;         // Low byte
    
    // 全ピクセルに色データを送信
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        send_data_multi(color_bytes, 2);
    }
    
    ESP_LOGI(TAG, "✅ Screen fill completed");
}

// 🔧 段階的表示テスト
void display_test()
{
    ESP_LOGI(TAG, "🧪 Starting display functionality tests...");
    
    ESP_LOGI(TAG, "🔴 Test 1: Red screen");
    fill_screen(0xF800);  // 赤
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "🟢 Test 2: Green screen");
    fill_screen(0x07E0);  // 緑
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "🔵 Test 3: Blue screen");
    fill_screen(0x001F);  // 青
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "🟡 Test 4: Yellow screen");
    fill_screen(0xFFE0);  // 黄色
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "⚫ Test 5: Black screen");
    fill_screen(0x0000);  // 黒
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "🎯 All display tests completed successfully!");
}

// 🔧 カラフルパターン表示
void display_pattern(int frame)
{
    ESP_LOGI(TAG, "🌈 Displaying pattern frame %d", frame);
    
    // Memory Write command
    send_command(ST7789P3_RAMWR);
    
    // カラフルパターンを生成
    for (int y = 0; y < LCD_HEIGHT; y++) {
        for (int x = 0; x < LCD_WIDTH; x++) {
            // 位置ベースでカラーを決定
            uint16_t color;
            int color_index = ((x / 10) + (y / 20) + frame) % 8;
            color = colors[color_index];
            
            // 色データをビッグエンディアンで送信
            uint8_t color_bytes[2];
            color_bytes[0] = (color >> 8) & 0xFF;
            color_bytes[1] = color & 0xFF;
            send_data_multi(color_bytes, 2);
        }
    }
}

// 🔧 メイン関数
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "🚀 === ST7789P3 RAW CONTROL - M5StampPico (76x284) ===");
    ESP_LOGI(TAG, "🎯 Starting complete independent ST7789P3 control...");
    
    // GPIO初期化
    init_gpio();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // SPI初期化  
    init_spi();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // ハードウェアリセット
    hardware_reset();
    
    // ST7789P3初期化
    init_st7789p3();
    
    // バックライトON
    //set_backlight(true);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // 表示テスト実行
    display_test();
    
    ESP_LOGI(TAG, "🎮 Starting continuous pattern display...");
    
    // 連続パターン表示
    int frame = 0;
    while (true) {
        display_pattern(frame);
        
        ESP_LOGI(TAG, "🌟 Frame %d: ST7789P3 displaying perfectly! 🎉", frame);
        
        frame = (frame + 1) % 100;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}