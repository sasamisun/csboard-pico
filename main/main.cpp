/*
 * M5StampPico with ST7789P3 Display (76×284)
 * バックライト問題解決版 + 楽しいアニメーション付きにゃ！🌟
 * DAC制御で2.0V～2.6Vの範囲で最適な明るさを実現！
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/dac.h"  // DAC制御用

// M5Unified & M5GFX
#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

// ログタグ定義
static const char *TAG = "M5StampPico";

// ST7789P3ピン定義
constexpr int PIN_SCL = 18;  // SCLK (SPI Clock)
constexpr int PIN_SDA = 26;  // MOSI (SDA)
constexpr int PIN_RST = 22;  // Reset
constexpr int PIN_DC = 21;   // Data/Command
constexpr int PIN_CS = 19;   // Chip Select
constexpr int PIN_BLK = 25;  // Backlight - GPIO25（DAC1）で確定にゃ！

// バックライト制御パラメータ（実験結果から最適値を設定）
constexpr float BL_VOLTAGE_MIN = 2.0;  // 最小電圧（V）- 暗め
constexpr float BL_VOLTAGE_MAX = 2.6;  // 最大電圧（V）- 明るめ
constexpr float BL_VOLTAGE_OPTIMAL = 2.4;  // 最適電圧（V）- ちょうど良い明るさ

// ST7789P3専用カスタムパネルクラス
class LGFX_StampPico_ST7789P3 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    LGFX_StampPico_ST7789P3(void)
    {
        // SPIバス設定（安定動作確認済み）
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 20000000;  // 20MHz
            cfg.freq_read = 10000000;   // 10MHz
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_SCL;
            cfg.pin_mosi = PIN_SDA;
            cfg.pin_miso = -1;
            cfg.pin_dc = PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ST7789P3パネル設定（76×284専用）
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = PIN_CS;
            cfg.pin_rst = PIN_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 76;
            cfg.memory_height = 284;
            cfg.panel_width = 76;
            cfg.panel_height = 284;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};

// ディスプレイインスタンス
static LGFX_StampPico_ST7789P3 tft;

// 現在のバックライト状態
static bool backlight_enabled = false;
static uint8_t current_brightness = 100;  // 0-100%

/**
 * バックライト制御関数（DAC制御版）
 * @param brightness 明るさ（0-100%）
 */
void setBacklight(uint8_t brightness)
{
    if (PIN_BLK != 25) {
        ESP_LOGW(TAG, "Backlight pin must be GPIO25 for DAC control!");
        return;
    }
    
    current_brightness = brightness;
    
    if (brightness == 0) {
        dac_output_voltage(DAC_CHANNEL_1, 0);
        dac_output_disable(DAC_CHANNEL_1);
        backlight_enabled = false;
    } else {
        if (!backlight_enabled) {
            dac_output_enable(DAC_CHANNEL_1);
            backlight_enabled = true;
        }
        
        float target_voltage = BL_VOLTAGE_MIN + 
                              (BL_VOLTAGE_MAX - BL_VOLTAGE_MIN) * brightness / 100.0f;
        uint8_t dac_value = (uint8_t)((target_voltage / 3.3f) * 255);
        
        if (dac_value < 155) dac_value = 155;  // 約2.0V
        if (dac_value > 201) dac_value = 201;  // 約2.6V
        
        dac_output_voltage(DAC_CHANNEL_1, dac_value);
    }
}

// カラーパレット（虹色+α）
const uint16_t rainbow[] = {
    0xF800, // 赤
    0xFD20, // オレンジ
    0xFFE0, // 黄色
    0x07E0, // 緑
    0x07FF, // シアン
    0x001F, // 青
    0x781F, // 紫
    0xF81F  // マゼンタ
};

// パーティクル構造体（花火用）
struct Particle {
    float x, y;
    float vx, vy;
    uint16_t color;
    int life;
    bool active;
};

// 花火パーティクル配列
const int MAX_PARTICLES = 20;
Particle particles[MAX_PARTICLES];

// ゲーム的な要素用
struct GameChar {
    float x, y;
    float speed;
    int frame;
    uint16_t color;
};

GameChar neko = {38, 200, 0.5f, 0, 0xFFFF};  // ネコキャラ

// ディスプレイ初期化
void initST7789P3()
{
    ESP_LOGI(TAG, "Initializing ST7789P3 Display...");
    
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(0x0000);
    setBacklight(80);  // 80%の明るさ
    
    // パーティクル初期化
    for(int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].active = false;
    }
    
    ESP_LOGI(TAG, "Display initialized にゃ！");
}

// 花火エフェクト生成
void createFirework(int x, int y)
{
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;
            float angle = (float)(rand() % 360) * M_PI / 180.0f;
            float speed = 1.0f + (rand() % 30) / 10.0f;
            particles[i].vx = speed * cos(angle);
            particles[i].vy = speed * sin(angle);
            particles[i].color = rainbow[rand() % 8];
            particles[i].life = 20 + rand() % 20;
            particles[i].active = true;
            if(i >= 15) break;  // 15個まで
        }
    }
}

// パーティクル更新
void updateParticles()
{
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.2f;  // 重力
            particles[i].life--;
            
            if(particles[i].life <= 0 || 
               particles[i].x < 0 || particles[i].x > 76 ||
               particles[i].y < 0 || particles[i].y > 284) {
                particles[i].active = false;
            }
        }
    }
}

// パーティクル描画
void drawParticles()
{
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(particles[i].active) {
            int size = (particles[i].life > 10) ? 2 : 1;
            tft.fillCircle(particles[i].x, particles[i].y, size, particles[i].color);
        }
    }
}

// ネコキャラ描画（シンプルな顔）
    void drawNeko(int x, int y, int frame)
{
    // 顔
    tft.fillCircle(x, y, 8, 0xFFE0);  // 黄色い顔
    
    // 耳
    tft.fillTriangle(x-6, y-5, x-3, y-10, x, y-5, 0xFFE0);
    tft.fillTriangle(x+6, y-5, x+3, y-10, x, y-5, 0xFFE0);
    
    // 目（アニメーション）
    if(frame % 30 < 25) {
        tft.fillCircle(x-3, y-2, 1, 0x0000);
        tft.fillCircle(x+3, y-2, 1, 0x0000);
    } else {
        tft.drawFastHLine(x-4, y-2, 3, 0x0000);
        tft.drawFastHLine(x+2, y-2, 3, 0x0000);
    }
    
    // 口
    tft.drawCircle(x-2, y+2, 2, 0xF800);
    tft.drawCircle(x+2, y+2, 2, 0xF800);
    
    // ひげ
    tft.drawFastHLine(x-12, y, 5, 0x0000);
    tft.drawFastHLine(x+7, y, 5, 0x0000);
}

// 虹の波エフェクト
void drawRainbowWave(int offset)
{
    for(int x = 0; x < 76; x++) {
        float wave1 = sin((x + offset) * 0.1f) * 10;
        float wave2 = cos((x + offset * 1.5f) * 0.08f) * 8;
        int y1 = 50 + wave1;
        int y2 = 50 + wave2;
        
        uint16_t color1 = rainbow[(x/10 + offset/20) % 8];
        uint16_t color2 = rainbow[((x/10 + offset/20) + 4) % 8];
        
        if(y1 >= 0 && y1 < 284) tft.drawPixel(x, y1, color1);
        if(y2 >= 0 && y2 < 284) tft.drawPixel(x, y2, color2);
    }
}

// 星空背景
void drawStarfield(int frame)
{
    // 静的な星
    for(int i = 0; i < 15; i++) {
        int x = (i * 17 + 7) % 76;
        int y = (i * 31 + 13) % 100;
        int brightness = (frame + i * 20) % 100;
        if(brightness > 50) {
            tft.drawPixel(x, y, 0xFFFF);
        }
    }
    
    // 流れ星
    if(frame % 60 == 0) {
        int startX = rand() % 76;
        for(int i = 0; i < 10; i++) {
            int x = startX + i * 2;
            int y = 10 + i;
            if(x < 76 && y < 100) {
                tft.drawPixel(x, y, 0xFFFF);
            }
        }
    }
}

// 踊る棒人間
void drawDancingStickman(int x, int y, int frame)
{
    // アニメーションフレーム
    int dance = (frame / 10) % 4;
    
    // 頭
    tft.drawCircle(x, y, 3, 0x07E0);
    
    // 体
    tft.drawFastVLine(x, y+3, 8, 0x07E0);
    
    // 手足のアニメーション
    switch(dance) {
        case 0:  // 両手上げ
            tft.drawLine(x, y+5, x-4, y+2, 0x07E0);
            tft.drawLine(x, y+5, x+4, y+2, 0x07E0);
            tft.drawLine(x, y+11, x-3, y+15, 0x07E0);
            tft.drawLine(x, y+11, x+3, y+15, 0x07E0);
            break;
        case 1:  // 右手上げ
            tft.drawLine(x, y+5, x-4, y+8, 0x07E0);
            tft.drawLine(x, y+5, x+4, y+2, 0x07E0);
            tft.drawLine(x, y+11, x-2, y+15, 0x07E0);
            tft.drawLine(x, y+11, x+2, y+15, 0x07E0);
            break;
        case 2:  // 両手横
            tft.drawLine(x, y+5, x-5, y+5, 0x07E0);
            tft.drawLine(x, y+5, x+5, y+5, 0x07E0);
            tft.drawLine(x, y+11, x-3, y+15, 0x07E0);
            tft.drawLine(x, y+11, x+3, y+15, 0x07E0);
            break;
        case 3:  // 左手上げ
            tft.drawLine(x, y+5, x-4, y+2, 0x07E0);
            tft.drawLine(x, y+5, x+4, y+8, 0x07E0);
            tft.drawLine(x, y+11, x-2, y+15, 0x07E0);
            tft.drawLine(x, y+11, x+2, y+15, 0x07E0);
            break;
    }
}

// メッセージスクロール
void drawScrollMessage(const char* msg, int offset, int y)
{
    tft.setTextSize(1);
    tft.setTextColor(0xF81F, 0x0000);
    
    int msg_len = strlen(msg);
    int total_width = msg_len * 6;
    int scroll_pos = offset % (total_width + 76);
    
    tft.setCursor(76 - scroll_pos, y);
    tft.print(msg);
    
    // ループ用に2回目も描画
    if(scroll_pos > 76) {
        tft.setCursor(76 + total_width - scroll_pos, y);
        tft.print(msg);
    }
}

// 楽しいメインループアニメーション
void funAnimation(int frame)
{
    // 背景を少しずつフェード（残像効果）
    tft.fillRect(0, 0, 76, 284, 0x0000);
    
    // 星空背景
    drawStarfield(frame);
    
    // 虹の波
    drawRainbowWave(frame);
    
    // ネコキャラの移動
    neko.x = 38 + 20 * sin(frame * 0.05f);
    neko.y = 200 + 10 * cos(frame * 0.08f);
    drawNeko(neko.x, neko.y, frame);
    
    // 踊る棒人間たち
    drawDancingStickman(15, 120, frame);
    drawDancingStickman(38, 125, frame + 10);
    drawDancingStickman(60, 120, frame + 20);
    
    // パーティクル更新と描画
    updateParticles();
    drawParticles();
    
    // 定期的に花火
    if(frame % 60 == 0) {
        createFirework(rand() % 76, 80 + rand() % 50);
        // バックライトも少し明滅
        setBacklight(90);
    } else if(frame % 60 == 5) {
        setBacklight(80);
    }
    
    // 回転する図形
    float angle = frame * 0.1f;
    for(int i = 0; i < 6; i++) {
        float a = angle + (i * M_PI / 3);
        int x1 = 38 + 15 * cos(a);
        int y1 = 250 + 15 * sin(a);
        int x2 = 38 + 15 * cos(a + M_PI / 3);
        int y2 = 250 + 15 * sin(a + M_PI / 3);
        tft.drawLine(x1, y1, x2, y2, rainbow[i % 8]);
    }
    
    // バウンスするボール
    int ball_y = 160 + abs((int)(30 * sin(frame * 0.1f)));
    tft.fillCircle(55, ball_y, 4, 0xFFE0);
    tft.drawCircle(55, ball_y, 5, 0xF800);
    
    // スクロールメッセージ
    drawScrollMessage("ST7789P3 Working Perfect! Meow~ ", frame * 2, 270);
    
    // FPSとフレーム表示
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF, 0x0000);
    tft.setCursor(2, 2);
    tft.printf("F:%d", frame % 1000);
    
    // ハートアニメーション（右上）
    if(frame % 20 < 10) {
        // ハート描画
        tft.fillCircle(65, 15, 2, 0xF800);
        tft.fillCircle(69, 15, 2, 0xF800);
        tft.fillTriangle(63, 16, 71, 16, 67, 20, 0xF800);
    }
}

// メイン関数
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Fun Animation Demo Start にゃ〜！ ===");
    
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
    
    // ディスプレイ初期化
    initST7789P3();
    
    // オープニング画面
    tft.fillScreen(0x0000);
    tft.setTextColor(0x07FF, 0x0000);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.println("FUN!");
    tft.setCursor(10, 120);
    tft.println("DEMO");
    tft.setTextSize(1);
    tft.setTextColor(0xFFE0, 0x0000);
    tft.setCursor(10, 150);
    tft.println("Starting...");
    
    // オープニングでバックライトフェードイン
    for(int i = 0; i <= 80; i += 5) {
        setBacklight(i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // メインループ
    ESP_LOGI(TAG, "Entering fun loop にゃ！");
    int frame = 0;
    
    while (true) {
        M5.update();
        
        // ボタンで花火発射（もしボタンがあれば）
        if (M5.BtnA.wasPressed()) {
            createFirework(38, 150);
            ESP_LOGI(TAG, "花火発射にゃ！");
        }
        
        // 楽しいアニメーション実行
        funAnimation(frame);
        
        frame++;
        
        // 約30FPS
        vTaskDelay(33 / portTICK_PERIOD_MS);
    }
}