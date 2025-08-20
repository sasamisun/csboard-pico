//#include <M5Unified.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// C++でapp_main関数をC言語として扱うための宣言にゃ
extern "C" void app_main(void)
{
    // M5Unifiedの初期化にゃ
    auto cfg = M5.config();
    
    // ST7789ディスプレイの設定にゃ
    cfg.external_display.module_display = true;
    cfg.external_display.pin_cs = 18;      // CS1ピンにゃ
    cfg.external_display.pin_dc = 19;      // DCピンにゃ
    cfg.external_display.pin_rst = 21;     // RSTピンにゃ
    cfg.external_display.pin_sda = 22;     // SDA1ピンにゃ
    cfg.external_display.pin_scl = 25;     // SCL1ピンにゃ
    cfg.external_display.pin_bl = 36;      // BLピンにゃ
    
    // M5Stackを初期化するにゃ
    M5.begin(cfg);
    
    // ディスプレイの初期化にゃ
    M5.Display.begin();
    M5.Display.setRotation(1);  // 画面の向きを調整にゃ
    M5.Display.setBrightness(128); // バックライトの明るさにゃ
    
    // 背景を黒にするにゃ
    M5.Display.fillScreen(BLACK);
    
    // テキストの設定にゃ
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(2);
    
    int counter = 0;
    
    // メインループにゃ
    while (1) {
        // M5Stackの更新にゃ
        M5.update();
        
        // 画面をクリアにゃ
        M5.Display.fillScreen(BLACK);
        
        // Hello Worldメッセージを表示にゃ
        M5.Display.setCursor(10, 10);
        M5.Display.printf("Hello M5StampPico!");
        
        M5.Display.setCursor(10, 40);
        M5.Display.printf("Count: %d にゃ", counter);
        
        // 現在時刻も表示するにゃ
        M5.Display.setCursor(10, 70);
        M5.Display.printf("Time: %lu ms", millis());
        
        // カウンターを増加にゃ
        counter++;
        
        // コンソールにも出力するにゃ
        printf("[%d] Hello M5StampPico World! にゃ\n", counter);
        
        // 1秒待機するにゃ
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}