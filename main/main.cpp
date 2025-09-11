/*
 * M5StampPico with ST7789P3 Display (76×284)
 * クラス分離版：LGFX_ST7789P3_76x284を外部ファイル化
 * ランダムドット問題解決版にゃ！🎯
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

// M5Unified & M5GFX
#include <M5Unified.h>

// 分離されたST7789P3クラス
#include "LGFX_ST7789P3_76x284.hpp"

// ログタグ定義
static const char *TAG = "ST7789P3_Main";

// ディスプレイインスタンス
static LGFX_ST7789P3_76x284 tft;

// 無効化されたバックライト制御関数
void setBacklight(uint8_t brightness)
{
    ESP_LOGI(TAG, "Backlight control requested: %d%% (Hardware controlled)", brightness);
}

// ディスプレイ初期化
void initST7789P3()
{
    ESP_LOGI(TAG, "=== ST7789P3 (76×284) Display Initialization ===");
    
    ESP_LOGI(TAG, "Pin Configuration:");
    ESP_LOGI(TAG, "  SCL  : GPIO%d", LGFX_ST7789P3_76x284::getPinSCL());
    ESP_LOGI(TAG, "  SDA  : GPIO%d", LGFX_ST7789P3_76x284::getPinSDA());
    ESP_LOGI(TAG, "  RST  : GPIO%d", LGFX_ST7789P3_76x284::getPinRST());
    ESP_LOGI(TAG, "  DC   : GPIO%d", LGFX_ST7789P3_76x284::getPinDC());
    ESP_LOGI(TAG, "  CS   : GPIO%d", LGFX_ST7789P3_76x284::getPinCS());
    ESP_LOGI(TAG, "  BLK  : Disabled (%d)", LGFX_ST7789P3_76x284::getPinBLK());
    ESP_LOGI(TAG, "Offset Configuration:");
    ESP_LOGI(TAG, "  X_OFFSET: %d", LGFX_ST7789P3_76x284::getOffsetX());
    ESP_LOGI(TAG, "  Y_OFFSET: %d (Random dot fix)", LGFX_ST7789P3_76x284::getOffsetY());
    
    // 標準初期化実行
    ESP_LOGI(TAG, "Calling standard tft.init()...");
    tft.init();
    
    ESP_LOGI(TAG, "Setting rotation to 0...");
    tft.setRotation(0);
    
    ESP_LOGI(TAG, "Display after standard init: %ldx%ld", tft.width(), tft.height());
    
    // 76×284専用カスタム初期化実行
    ESP_LOGI(TAG, "Performing custom initialization for 76×284...");
    tft.performCustomInitialization();
    
    ESP_LOGI(TAG, "Display initialized successfully!");
    ESP_LOGI(TAG, "Final resolution: %ldx%ld", tft.width(), tft.height());
    
    setBacklight(80);
    
    ESP_LOGI(TAG, "=== Initialization Complete ===");
}

// 詳細カラーテスト（76×284専用）
void performColorTest()
{
    ESP_LOGI(TAG, "=== Color Test Start ===");
    
    // テスト1: 完全な黒
    ESP_LOGI(TAG, "Test 1: Black screen");
    tft.fillScreen(0x0000);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // テスト2: 純粋な赤
    ESP_LOGI(TAG, "Test 2: Pure Red");
    tft.fillScreen(0xF800);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // テスト3: 純粋な緑
    ESP_LOGI(TAG, "Test 3: Pure Green");
    tft.fillScreen(0x07E0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // テスト4: 純粋な青
    ESP_LOGI(TAG, "Test 4: Pure Blue");
    tft.fillScreen(0x001F);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // テスト5: 白
    ESP_LOGI(TAG, "Test 5: White");
    tft.fillScreen(0xFFFF);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // テスト6: 境界テスト（重要）
    ESP_LOGI(TAG, "Test 6: Boundary test");
    tft.fillScreen(0x0000);  // 黒背景
    
    // 4つの角に色付きピクセル（実際のパネルサイズに基づく）
    int max_x = tft.width() - 1;   // 75 (76-1)
    int max_y = tft.height() - 1;  // 283 (284-1)
    
    ESP_LOGI(TAG, "Drawing boundary pixels at corners (0,0) to (%d,%d)", max_x, max_y);
    
    tft.drawPixel(0, 0, 0xF800);           // 左上：赤
    tft.drawPixel(max_x, 0, 0x07E0);       // 右上：緑
    tft.drawPixel(0, max_y, 0x001F);       // 左下：青
    tft.drawPixel(max_x, max_y, 0xFFFF);   // 右下：白
    
    // 中央十字
    int center_x = tft.width() / 2;   // 38
    int center_y = tft.height() / 2;  // 142
    
    ESP_LOGI(TAG, "Drawing center cross at (%d,%d)", center_x, center_y);
    tft.drawFastHLine(0, center_y, tft.width(), 0xFFE0);     // 水平線：黄色
    tft.drawFastVLine(center_x, 0, tft.height(), 0xF81F);    // 垂直線：マゼンタ
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "=== Color Test Complete ===");
}

// ストライプパターンテスト
void performStripeTest()
{
    ESP_LOGI(TAG, "=== Stripe Pattern Test ===");
    
    // 垂直ストライプ（実際の幅用）
    ESP_LOGI(TAG, "Drawing vertical stripes...");
    for(int x = 0; x < tft.width(); x++) {
        uint16_t color = (x % 8 < 4) ? 0xFFFF : 0x0000;  // 4ピクセルごとに白黒
        tft.drawFastVLine(x, 0, tft.height(), color);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    // 水平ストライプ（実際の高さ用）
    ESP_LOGI(TAG, "Drawing horizontal stripes...");
    for(int y = 0; y < tft.height(); y++) {
        uint16_t color = (y % 16 < 8) ? 0xF800 : 0x07E0;  // 8ピクセルごとに赤緑
        tft.drawFastHLine(0, y, tft.width(), color);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "=== Stripe Test Complete ===");
}

// テキスト表示テスト（実際の解像度に最適化）
void performTextTest()
{
    ESP_LOGI(TAG, "=== Text Display Test ===");
    
    tft.fillScreen(0x0000);  // 黒背景
    tft.setTextColor(0xFFFF, 0x0000);  // 白文字、黒背景
    
    // 実際の幅に最適化したテキスト配置
    tft.setTextSize(1);
    tft.setCursor(2, 10);
    tft.println("ST7789P3");
    tft.setCursor(2, 25);
    tft.println("76x284");
    tft.setCursor(2, 40);
    tft.println("SEPARATED");
    tft.setCursor(2, 55);
    tft.println("CLASS!");
    
    tft.setTextSize(2);
    tft.setCursor(5, 75);
    tft.println("WORKS");
    
    // カラフルなテキスト
    tft.setTextSize(1);
    tft.setTextColor(0xF800, 0x0000);  // 赤
    tft.setCursor(2, 105);
    tft.println("Red Text");
    
    tft.setTextColor(0x07E0, 0x0000);  // 緑
    tft.setCursor(2, 120);
    tft.println("Green Text");
    
    tft.setTextColor(0x001F, 0x0000);  // 青
    tft.setCursor(2, 135);
    tft.println("Blue Text");
    
    // 実際の解像度情報表示
    tft.setTextColor(0xFFE0, 0x0000);  // 黄色
    tft.setTextSize(1);
    tft.setCursor(2, 155);
    tft.printf("W:%ld H:%ld", tft.width(), tft.height());
    
    // オフセット情報表示
    tft.setTextColor(0xF81F, 0x0000);  // マゼンタ
    tft.setCursor(2, 170);
    tft.printf("OFS:%d,%d", LGFX_ST7789P3_76x284::getOffsetX(), LGFX_ST7789P3_76x284::getOffsetY());
    
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "=== Text Test Complete ===");
}

// 楽しいアニメーションテスト
void performAnimationTest()
{
    ESP_LOGI(TAG, "=== Animation Test ===");
    
    const uint16_t colors[] = {
        0xF800, // 赤
        0xFD20, // オレンジ
        0xFFE0, // 黄色
        0x07E0, // 緑
        0x07FF, // シアン
        0x001F, // 青
        0x781F, // 紫
        0xF81F  // マゼンタ
    };
    
    // 回転する色の輪
    for(int frame = 0; frame < 60; frame++) {
        tft.fillScreen(0x0000);
        
        // 中央点
        int center_x = tft.width() / 2;
        int center_y = tft.height() / 2;
        
        // 回転する点群
        for(int i = 0; i < 8; i++) {
            float angle = (frame * 0.1f) + (i * M_PI / 4);
            int x = center_x + 25 * cos(angle);
            int y = center_y + 25 * sin(angle);
            
            if(x >= 0 && x < tft.width() && y >= 0 && y < tft.height()) {
                tft.fillCircle(x, y, 3, colors[i]);
            }
        }
        
        // フレーム情報
        tft.setTextColor(0xFFFF, 0x0000);
        tft.setTextSize(1);
        tft.setCursor(2, 2);
        tft.printf("Frame:%d", frame);
        
        // クラス情報
        tft.setCursor(2, tft.height() - 15);
        tft.setTextColor(0x07FF, 0x0000);
        tft.println("Class OK!");
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "=== Animation Test Complete ===");
}

// メイン関数
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== ST7789P3 (76×284) Class Separated Version Start ===");
    
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.clear_display = false;
    cfg.output_power = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    cfg.external_imu = false;
    cfg.external_rtc = false;
    M5.begin(cfg);
    
    ESP_LOGI(TAG, "M5Unified initialized");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // ディスプレイ初期化（分離されたクラス使用）
    initST7789P3();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "Starting tests with separated class...");
    
    // テストシーケンス実行
    while (true) {
        // 1. 基本カラーテスト
        performColorTest();
        
        // 2. ストライプパターンテスト
        performStripeTest();
        
        // 3. テキスト表示テスト
        performTextTest();
        
        // 4. アニメーションテスト
        performAnimationTest();
        
        // 完了メッセージ
        ESP_LOGI(TAG, "=== All tests completed successfully! ===");
        tft.fillScreen(0x0000);
        tft.setTextColor(0x07FF, 0x0000);  // シアン
        tft.setTextSize(1);
        tft.setCursor(5, tft.height()/2 - 30);
        tft.println("SEPARATED");
        tft.setCursor(5, tft.height()/2 - 15);
        tft.println("CLASS");
        tft.setCursor(5, tft.height()/2);
        tft.println("WORKING!");
        tft.setCursor(5, tft.height()/2 + 15);
        tft.println("76x284 OK!");
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "=== Restarting Test Sequence ===");
    }
}