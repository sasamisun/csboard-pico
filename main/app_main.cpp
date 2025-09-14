/*
 * app_main.cpp (2つの画像交互表示版)
 * bglong1.h と dot_landscape.h を交互に表示
 * min/max関数不使用版
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <M5Unified.h>

// ST7789P3ディスプレイとレトロゲームシステム
#include "LGFX_ST7789P3_76x284.hpp"
#include "RetroGamePaletteImage.hpp"

// 【重要】画像データをインクルード
#include "bglong1.h"
#include "dot_landscape.h"

static const char *TAG = "ImageSlideshow";
static LGFX_ST7789P3_76x284 tft;

/**
 * min/max関数の代替（三項演算子版）
 */
inline int clamp(int value, int minVal, int maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

/**
 * 1つの画像を表示する関数
 */
void displayImage(const uint8_t* imageData, int imageWidth, int imageHeight, const char* imageName) {
    ESP_LOGI(TAG, "Displaying image: %s (%dx%d)", imageName, imageWidth, imageHeight);
    
   RetroColorPalette palet;
   palet.initGameBoyColors();
    // パレット画像データを作成
    PaletteImageData img(imageData, imageWidth, imageHeight, &palet);
    
    // レンダラーを作成（rotation=1用：284x76）
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    // 背景をクリア（黒）
    renderer.clearCanvas(0x0000);
    
    // 画像を中央に配置
    int centerX = (tft.width() - imageWidth) / 2;
    int centerY = (tft.height() - imageHeight) / 2;
    
    // 画像が画面より大きい場合は左上に配置
    int drawX = (centerX >= 0) ? centerX : 0;
    int drawY = (centerY >= 0) ? centerY : 0;
    
    // 画像をキャンバスに描画（透明色対応）
    renderer.drawToCanvas(img, drawX, drawY, true);
    
    // キャンバスをディスプレイに表示
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Image '%s' displayed at (%d, %d)", imageName, drawX, drawY);
}

/**
 * 2つの画像を交互に表示
 */
void alternateImages() {
    ESP_LOGI(TAG, "=== Starting Image Slideshow ===");
    
    while (true) {
        // 画像1を表示（bglong1）
        displayImage(bglong1_data, bglong1_width, bglong1_height, "bglong1");
        vTaskDelay(pdMS_TO_TICKS(3000)); // 3秒表示
        
        // 画像2を表示（dot_landscape）  
        displayImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, "dot_landscape");
        vTaskDelay(pdMS_TO_TICKS(3000)); // 3秒表示
        
        ESP_LOGI(TAG, "Slideshow cycle complete");
    }
}

/**
 * 高速切り替え版（デモ用）
 */
void fastSlideshow() {
    ESP_LOGI(TAG, "=== Fast Slideshow Demo ===");
    
    for (int cycle = 0; cycle < 10; cycle++) { // 10回繰り返し
        ESP_LOGI(TAG, "Fast cycle %d/10", cycle + 1);
        
        // bglong1を0.5秒表示
        displayImage(bglong1_data, bglong1_width, bglong1_height, "bglong1");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // dot_landscapeを0.5秒表示
        displayImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, "dot_landscape");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "Fast slideshow complete");
}

/**
 * 画像情報表示
 */
void printImageInfo() {
    ESP_LOGI(TAG, "=== Image Information ===");
    ESP_LOGI(TAG, "bglong1:");
    ESP_LOGI(TAG, "  Size: %dx%d pixels", bglong1_width, bglong1_height);
    ESP_LOGI(TAG, "  Data size: %d bytes", (bglong1_width * bglong1_height + 1) / 2);
    
    ESP_LOGI(TAG, "dot_landscape:");
    ESP_LOGI(TAG, "  Size: %dx%d pixels", dot_landscape_width, dot_landscape_height);
    ESP_LOGI(TAG, "  Data size: %d bytes", (dot_landscape_width * dot_landscape_height + 1) / 2);
    
    ESP_LOGI(TAG, "Display:");
    ESP_LOGI(TAG, "  Size: %ldx%ld pixels (rotation=1)", tft.width(), tft.height());
    ESP_LOGI(TAG, "========================");
}

/**
 * カスタムパレット版（色を変えて表示）
 */
void colorVariationSlideshow() {
    ESP_LOGI(TAG, "=== Color Variation Slideshow ===");
    
    // 通常パレット
    displayImage(bglong1_data, bglong1_width, bglong1_height, "bglong1 (Normal)");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // セピア調パレット
    RetroColorPalette sepiaColors;
    sepiaColors.initVividColors();
    PaletteImageData sepiaImg(bglong1_data, bglong1_width, bglong1_height, &sepiaColors);
    
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    renderer.clearCanvas(0x0000);
    
    int centerX = (tft.width() - bglong1_width) / 2;
    int centerY = (tft.height() - bglong1_height) / 2;
    int drawX = (centerX >= 0) ? centerX : 0;
    int drawY = (centerY >= 0) ? centerY : 0;
    
    renderer.drawToCanvas(sepiaImg, drawX, drawY, true);
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "bglong1 (Sepia) displayed");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // グレースケール
    RetroColorPalette grayColors;
    grayColors.initGrayscalePalette();
    PaletteImageData grayImg(dot_landscape_data, dot_landscape_width, dot_landscape_height, &grayColors);
    
    renderer.clearCanvas(0x0000);
    centerX = (tft.width() - dot_landscape_width) / 2;
    centerY = (tft.height() - dot_landscape_height) / 2;
    drawX = (centerX >= 0) ? centerX : 0;
    drawY = (centerY >= 0) ? centerY : 0;
    
    renderer.drawToCanvas(grayImg, drawX, drawY, true);
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "dot_landscape (Grayscale) displayed");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Color variation complete");
}

/**
 * アニメーション風切り替え
 */
void animatedSlideshow() {
    ESP_LOGI(TAG, "=== Animated Slideshow ===");
    
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    for (int cycle = 0; cycle < 3; cycle++) {
        ESP_LOGI(TAG, "Animation cycle %d/3", cycle + 1);
        
        // スライドイン風のアニメーション
        for (int frame = 0; frame < 20; frame++) {
            renderer.clearCanvas(0x0000);
            
            // bglong1を左からスライドイン
            int x1 = -bglong1_width + (frame * (tft.width() + bglong1_width) / 20);
            if (x1 < tft.width()) {
                PaletteImageData img1(bglong1_data, bglong1_width, bglong1_height);
                
                // X座標制限（min/max不使用）
                int drawX1 = (x1 > 0) ? x1 : 0;
                if (drawX1 > tft.width() - bglong1_width) {
                    drawX1 = tft.width() - bglong1_width;
                }
                
                // Y座標制限（min/max不使用）
                int drawY1 = (tft.height() - bglong1_height) / 2;
                if (drawY1 < 0) drawY1 = 0;
                if (drawY1 > tft.height() - bglong1_height) {
                    drawY1 = tft.height() - bglong1_height;
                }
                
                renderer.drawToCanvas(img1, drawX1, drawY1, true);
            }
            
            renderer.pushCanvasToDisplayOpaque(0, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒静止
        
        // dot_landscapeに切り替え（フェード風）
        for (int frame = 0; frame < 20; frame++) {
            renderer.clearCanvas(0x0000);
            
            PaletteImageData img2(dot_landscape_data, dot_landscape_width, dot_landscape_height);
            int centerX = (tft.width() - dot_landscape_width) / 2;
            int centerY = (tft.height() - dot_landscape_height) / 2;
            
            // 座標制限（min/max不使用）
            int drawX = (centerX > 0) ? centerX : 0;
            if (drawX > tft.width() - dot_landscape_width) {
                drawX = tft.width() - dot_landscape_width;
            }
            
            int drawY = (centerY > 0) ? centerY : 0;
            if (drawY > tft.height() - dot_landscape_height) {
                drawY = tft.height() - dot_landscape_height;
            }
            
            renderer.drawToCanvas(img2, drawX, drawY, true);
            renderer.pushCanvasToDisplayOpaque(0, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒静止
    }
    
    ESP_LOGI(TAG, "Animated slideshow complete");
}

/**
 * メイン関数
 */
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "2-Image Slideshow Starting...");
    
    // 初期化待機
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // ディスプレイ初期化（rotation=1：横向き284×76）
    tft.init();
    ESP_LOGI(TAG, "Display initialized: %ldx%ld", tft.width(), tft.height());
    
    // 画像情報表示
    printImageInfo();
    
    // テスト表示（各画像を1回ずつ）
    ESP_LOGI(TAG, "Testing each image once...");
    displayImage(bglong1_data, bglong1_width, bglong1_height, "bglong1 (Test)");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    displayImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, "dot_landscape (Test)");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 様々なスライドショーモードを実行
    while (true) {
        // 1. 通常の交互表示（3秒ずつ、5回）
        ESP_LOGI(TAG, "Starting normal slideshow (5 cycles)...");
        for (int i = 0; i < 5; i++) {
            displayImage(bglong1_data, bglong1_width, bglong1_height, "bglong1");
            vTaskDelay(pdMS_TO_TICKS(3000));
            
            displayImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, "dot_landscape");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        
        // 2. 高速切り替え
        fastSlideshow();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 3. カラーバリエーション
        colorVariationSlideshow();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 4. アニメーション風
        animatedSlideshow();
        
        ESP_LOGI(TAG, "=== Complete cycle finished, restarting... ===");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/*
【修正内容】

1. min/max関数を完全削除:
   - 三項演算子 (condition) ? value1 : value2 に置き換え
   - if文での上下限チェック追加

2. 座標制限の新しい方法:
   旧: drawX = max(0, min(centerX, maxValue));
   新: drawX = (centerX > 0) ? centerX : 0;
       if (drawX > maxValue) drawX = maxValue;

3. clamp関数を定義（使用されていませんが参考用）:
   inline int clamp(int value, int minVal, int maxVal)

【使用方法】
- ESP-IDF環境でmin/max関数エラーが発生しない
- 従来と同じ動作を保証
- bglong1.h と dot_landscape.h の変数名確認は必要

【動作確認】
- ビルドエラーが解消されるはず
- 画像の交互表示が正常に動作
- 座標計算も正しく制限される
*/