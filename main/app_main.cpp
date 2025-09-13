/*
 * パレット画像システム完全使用例 for M5StampPico （横向き対応版）
 * dot_landscape.hで生成された画像データを使用する方法
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <M5Unified.h>

// ST7789P3ディスプレイとレトロゲームシステム
#include "LGFX_ST7789P3_76x284.hpp"
#include "RetroGamePaletteImage.hpp"

// 【重要】パレット変換ツールで生成されたヘッダーをインクルード
#include "dot_landscape.h"

static const char *TAG = "PaletteImageExample";

// ディスプレイインスタンス
static LGFX_ST7789P3_76x284 tft;

// 基本的な描画例（横向き対応）
void drawImageBasic() {
    ESP_LOGI(TAG, "=== Basic Image Drawing ===");
    
    // 1. 画像データからPaletteImageDataオブジェクトを作成
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    
    // 2. レンダラーを作成（横向きキャンバス：284x76）
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    // 3. 背景をクリア（黒）
    renderer.clearCanvas(0x0000);
    
    // 4. 画像をキャンバスに描画
    renderer.drawToCanvas(img, 0, 0, true);  // 左上から透明色対応で描画
    
    // 5. キャンバスをディスプレイに表示
    renderer.pushCanvasToDisplayOpaque(0, 0);  // 不透明描画
    
    ESP_LOGI(TAG, "Image drawn: %dx%d at (0,0)", dot_landscape_width, dot_landscape_height);
}

// 中央表示の例（横向き対応）
void drawImageCentered() {
    ESP_LOGI(TAG, "=== Centered Image Drawing ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    // 背景を青にクリア
    renderer.clearCanvas(0x001F);  // RGB565で青
    
    // 画像を中央に配置（横向きサイズで計算）
    int centerX = (tft.width() - dot_landscape_width) / 2;
    int centerY = (tft.height() - dot_landscape_height) / 2;
    
    renderer.drawToCanvas(img, centerX, centerY, true);
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Centered image at (%d, %d)", centerX, centerY);
}

// スケール描画の例（横向き対応）
void drawImageScaled() {
    ESP_LOGI(TAG, "=== Scaled Image Drawing ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    renderer.clearCanvas(0x0400);  // ダークグリーン背景
    
    // 2倍スケールで描画
    float scaleX = 2.0f;
    float scaleY = 2.0f;
    
    // スケール後のサイズを考慮して中央配置
    int scaledWidth = (int)(dot_landscape_width * scaleX);
    int scaledHeight = (int)(dot_landscape_height * scaleY);
    int x = (tft.width() - scaledWidth) / 2;
    int y = (tft.height() - scaledHeight) / 2;
    
    renderer.drawToCanvasScaled(img, x, y, scaleX, scaleY, true);
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Scaled image %.1fx%.1f at (%d, %d)", scaleX, scaleY, x, y);
}

// 複数画像を配置する例（横向き対応）
void drawMultipleImages() {
    ESP_LOGI(TAG, "=== Multiple Images Drawing ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    renderer.clearCanvas(0x8410);  // グレー背景
    
    // 小さなサイズで複数配置
    float scale = 0.5f;
    int scaledW = (int)(dot_landscape_width * scale);
    int scaledH = (int)(dot_landscape_height * scale);
    
    // 横向きレイアウトで配置（横に多く、縦に少なく）
    int screenW = tft.width();   // 284
    int screenH = tft.height();  // 76
    
    // 左右に配置
    renderer.drawToCanvasScaled(img, 5, 5, scale, scale, true);                           // 左上
    renderer.drawToCanvasScaled(img, screenW - scaledW - 5, 5, scale, scale, true);      // 右上
    renderer.drawToCanvasScaled(img, 5, screenH - scaledH - 5, scale, scale, true);      // 左下
    renderer.drawToCanvasScaled(img, screenW - scaledW - 5, screenH - scaledH - 5, scale, scale, true); // 右下
    
    // 中央にも配置
    int centerX = (screenW - scaledW) / 2;
    int centerY = (screenH - scaledH) / 2;
    renderer.drawToCanvasScaled(img, centerX, centerY, scale, scale, true);              // 中央
    
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Five scaled images drawn in landscape layout");
}

// アニメーション例（横向き対応）
void animateImage() {
    ESP_LOGI(TAG, "=== Image Animation ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    // 60フレームのアニメーション
    for (int frame = 0; frame < 60; frame++) {
        renderer.clearCanvas(0x0000);  // 黒背景
        
        // 正弦波で左右に動かす（横向きなので左右移動の方が効果的）
        int x = (tft.width() / 2) + (int)(50.0f * sin(frame * 0.2f));  // 中央±50ピクセル
        int y = (tft.height() - dot_landscape_height) / 2;             // 垂直中央
        
        // 画面外にはみ出さないように制限
        x = max(0, min(x, tft.width() - dot_landscape_width));
        
        renderer.drawToCanvas(img, x, y, true);
        renderer.pushCanvasToDisplayOpaque(0, 0);
        
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms待機
    }
    
    ESP_LOGI(TAG, "Animation complete");
}

// カスタムパレット使用例（横向き対応）
void drawWithCustomPalette() {
    ESP_LOGI(TAG, "=== Custom Palette Drawing ===");
    
    // カスタムパレットを作成（セピア調）
    RetroColorPalette sepiaColors;
    sepiaColors.initSepiaPalette();
    
    // カスタムパレットで画像データを作成
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height, &sepiaColors);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    renderer.clearCanvas(0x0000);
    
    int centerX = (tft.width() - dot_landscape_width) / 2;
    int centerY = (tft.height() - dot_landscape_height) / 2;
    
    renderer.drawToCanvas(img, centerX, centerY, true);
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Sepia-toned image drawn");
}

// パレットカラーエフェクト例（横向き対応）
void colorCycleEffect() {
    ESP_LOGI(TAG, "=== Color Cycle Effect ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    int centerX = (tft.width() - dot_landscape_width) / 2;
    int centerY = (tft.height() - dot_landscape_height) / 2;
    
    // 120フレームで色相を変化
    for (int frame = 0; frame < 120; frame++) {
        renderer.clearCanvas(0x0000);
        
        // 動的パレットを作成
        RetroColorPalette dynamicPalette;
        for (int i = 1; i < 16; i++) {  // 透明色(0)は変更しない
            uint16_t hue = (frame * 3 + i * 24) % 360;  // 色相を時間とともに変化
            dynamicPalette.setColor(i, RetroColorPalette::hsvToRgb565(hue, 80, 90));
        }
        
        // パレットを変更して描画
        img.setPalette(dynamicPalette);
        renderer.drawToCanvas(img, centerX, centerY, true);
        renderer.pushCanvasToDisplayOpaque(0, 0);
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "Color cycle complete");
}

// 横向きレイアウト専用デモ
void drawLandscapeDemo() {
    ESP_LOGI(TAG, "=== Landscape Layout Demo ===");
    
    PaletteImageData img(dot_landscape_data, dot_landscape_width, dot_landscape_height);
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());
    
    renderer.clearCanvas(0x0010);  // ダークブルー背景
    
    // ヘッダーバー（上部）
    renderer.getCanvas()->fillRect(0, 0, tft.width(), 15, 0x4208);  // グレー
    renderer.getCanvas()->setTextColor(0xFFFF, 0x4208);
    renderer.getCanvas()->setTextSize(1);
    renderer.getCanvas()->setCursor(5, 4);
    renderer.getCanvas()->println("M5StampPico - Landscape Mode");
    
    // サイドバー（右端）
    renderer.getCanvas()->fillRect(tft.width()-40, 15, 40, tft.height()-15, 0x0400);  // ダークグリーン
    renderer.getCanvas()->setTextColor(0xFFFF, 0x0400);
    renderer.getCanvas()->setCursor(tft.width()-35, 25);
    renderer.getCanvas()->println("STA");
    renderer.getCanvas()->setCursor(tft.width()-35, 35);
    renderer.getCanvas()->println("TUS");
    renderer.getCanvas()->setCursor(tft.width()-35, 55);
    renderer.getCanvas()->println("OK!");
    
    // メイン画像（左側）
    int imgX = 10;
    int imgY = (tft.height() - dot_landscape_height) / 2;
    renderer.drawToCanvas(img, imgX, imgY, true);
    
    renderer.pushCanvasToDisplayOpaque(0, 0);
    
    ESP_LOGI(TAG, "Landscape layout demo complete");
}

// メイン関数（横向き対応版）
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== Palette Image System Demo (Landscape) ===");
    
    // M5Unified初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // 【重要】ディスプレイ初期化（横向き）
    tft.initWithRotation(1);  // 横向き（284×76）
    
    ESP_LOGI(TAG, "Display initialized: %ldx%ld (landscape)", tft.width(), tft.height());
    ESP_LOGI(TAG, "Image size: %dx%d", dot_landscape_width, dot_landscape_height);
    
    while (true) {
        // 基本描画
        drawImageBasic();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        // 中央描画
        drawImageCentered();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        // スケール描画
        drawImageScaled();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        // 複数配置（横向きレイアウト）
        drawMultipleImages();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        // アニメーション（左右移動）
        animateImage();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        // カスタムパレット
        drawWithCustomPalette();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        // カラーサイクルエフェクト
        colorCycleEffect();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        // 横向けレイアウトデモ
        drawLandscapeDemo();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        ESP_LOGI(TAG, "=== Demo cycle complete ===");
    }
}

/*
主な変更点：

1. ディスプレイ初期化：
   tft.init();
   tft.setRotation(0);
   ↓
   tft.initWithRotation(1);  // 横向き

2. キャンバスサイズ：
   PaletteImageRenderer renderer(&tft, 76, 284);
   ↓
   PaletteImageRenderer renderer(&tft, tft.width(), tft.height());

3. 座標計算：
   (76 - width) / 2  →  (tft.width() - width) / 2
   (284 - height) / 2  →  (tft.height() - height) / 2

4. アニメーション：
   縦移動 → 横移動（横向きの方が効果的）

5. レイアウト：
   横向きに最適化されたUI要素配置

結果：
- 284×76の横向きディスプレイで動作
- すべての描画が適切にセンタリング
- 横向きに最適化されたレイアウト
*/