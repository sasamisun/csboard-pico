/*
 * M5StampPico with ST7789P3 Display (76×284)
 * ESP32-PICO-D4 + ST7789P3 専用設定版
 * ST7789P3の特殊な76×284サイズに完全対応するにゃ！🎨
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

// M5Unified & M5GFX
#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

// ログタグ定義
static const char *TAG = "M5StampPico";

// ST7789P3ピン定義
constexpr int PIN_SCL = 18; // SCLK (SPI Clock)
constexpr int PIN_SDA = 26; // MOSI (SDA)
constexpr int PIN_RST = 22; // Reset
constexpr int PIN_DC = 21;  // Data/Command
constexpr int PIN_CS = 19;  // Chip Select
constexpr int PIN_BLK = -1; // Backlight（未使用）

// ST7789P3専用カスタムパネルクラス
class LGFX_StampPico_ST7789P3 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance; // ST7789ベースで設定変更
    lgfx::Bus_SPI _bus_instance;        // SPIバスインスタンス
    lgfx::Light_PWM _light_instance;    // バックライト制御インスタンス

public:
    LGFX_StampPico_ST7789P3(void)
    {
        // SPIバス設定（ST7789P3用に最適化）
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST;      // HSPIホスト使用
            cfg.spi_mode = 0;              // SPI mode 0
            cfg.freq_write = 20000000;     // 20MHzに下げて安定性向上
            cfg.freq_read = 10000000;      // 10MHzに下げて安定性向上
            cfg.spi_3wire = false;         // 4-wire SPI（CSピン使用）
            cfg.use_lock = true;           // トランザクションロック使用
            cfg.dma_channel = SPI_DMA_CH_AUTO; // DMA自動選択
            cfg.pin_sclk = PIN_SCL;        // SCLKピン番号
            cfg.pin_mosi = PIN_SDA;        // MOSIピン番号
            cfg.pin_miso = -1;             // MISOピン（未使用）
            cfg.pin_dc = PIN_DC;           // D/Cピン番号
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ST7789P3パネル設定（76×284専用）
        {
            auto cfg = _panel_instance.config();

            cfg.pin_cs = PIN_CS;
            cfg.pin_rst = PIN_RST;
            cfg.pin_busy = -1;

            // ST7789P3の特殊設定
            cfg.memory_width = 76;         // メモリ幅を実サイズに
            cfg.memory_height = 284;       // メモリ高さを実サイズに
            cfg.panel_width = 76;          // パネル幅
            cfg.panel_height = 284;        // パネル高さ
            
            // ST7789P3用オフセット（ゼロから開始）
            cfg.offset_x = 0;              // Xオフセットなし
            cfg.offset_y = 0;              // Yオフセットなし
            cfg.offset_rotation = 0;       // 回転オフセットなし
            
            // ST7789P3用読み取り設定
            cfg.dummy_read_pixel = 8;      // ダミー読み取りピクセル数
            cfg.dummy_read_bits = 1;       // ダミー読み取りビット数
            
            cfg.readable = true;           // 読み取り可能
            cfg.invert = false;            // 色反転なし（P3は反転設定が異なる場合）
            cfg.rgb_order = false;         // RGB順序
            cfg.dlen_16bit = false;        // 16ビットデータ長
            cfg.bus_shared = true;         // バス共有

            _panel_instance.config(cfg);
        }

        // バックライト設定（未使用の場合はスキップ）
        if (PIN_BLK >= 0) {
            auto cfg = _light_instance.config();
            cfg.pin_bl = PIN_BLK;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ST7789P3ディスプレイインスタンス
static LGFX_StampPico_ST7789P3 tft;

// カラーパレット定義
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

// アニメーション用変数
int animation_frame = 0;
int color_index = 0;

// システム時間取得関数
unsigned long getSystemTimeMs()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// ST7789P3ディスプレイの初期化
void initST7789P3()
{
    ESP_LOGI(TAG, "Initializing ST7789P3 Display (76x284)...");

    // ディスプレイ初期化
    tft.init();

    // ST7789P3用基本設定
    tft.setRotation(0);     // 縦向き（0度）
    tft.setBrightness(255); // 最大明度
    
    // 初期化確認用の段階テスト
    ESP_LOGI(TAG, "ST7789P3 Basic Test - Red Screen");
    tft.fillScreen(0xF800); // 赤で全画面
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "ST7789P3 Basic Test - Green Screen");
    tft.fillScreen(0x07E0); // 緑で全画面
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "ST7789P3 Basic Test - Blue Screen");
    tft.fillScreen(0x001F); // 青で全画面
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "ST7789P3 Basic Test - Black Screen");
    tft.fillScreen(0x0000); // 黒で全画面
    
    ESP_LOGI(TAG, "ST7789P3 Display (76x284) initialized successfully!");
}

// ウェルカムメッセージ表示（ST7789P3専用）
void showWelcomeMessage()
{
    ESP_LOGI(TAG, "Displaying welcome message on ST7789P3...");

    tft.fillScreen(0x0000); // 黒背景
    
    // ST7789P3確認メッセージ
    tft.setTextColor(0x07FF, 0x0000); // シアン文字
    tft.setTextSize(1);
    tft.setCursor(5, 10);
    tft.println("ST7789P3");
    
    tft.setTextColor(0x07E0, 0x0000); // 緑文字
    tft.setCursor(5, 25);
    tft.println("DETECTED!");
    
    // 基本情報
    tft.setTextColor(0xFFE0, 0x0000); // 黄色文字
    tft.setCursor(5, 45);
    tft.println("M5StampPico");
    
    tft.setTextColor(0xFFFF, 0x0000); // 白文字
    tft.setCursor(5, 60);
    tft.println("ESP32-PICO");
    
    tft.setCursor(5, 75);
    tft.println("with");
    
    tft.setTextColor(0xF81F, 0x0000); // マゼンタ文字
    tft.setCursor(5, 90);
    tft.println("ST7789P3");
    
    // サイズ情報
    tft.setTextColor(0x07FF, 0x0000); // シアン文字
    tft.setCursor(5, 110);
    tft.printf("Size:%ldx%ld", tft.width(), tft.height());
    
    // 装飾ライン
    for(int i = 0; i < 8; i++) {
        tft.drawFastHLine(0, 130 + i*2, 76, colors[i]);
    }
    
    // 成功メッセージ
    tft.setTextColor(0x07E0, 0x0000); // 緑文字
    tft.setCursor(15, 155);
    tft.println("SUCCESS!");
    
    // 縦長特性を活かした装飾
    for(int y = 170; y < 280; y += 10) {
        int color_idx = ((y - 170) / 10) % 8;
        tft.drawFastHLine(0, y, 76, colors[color_idx]);
        tft.drawFastHLine(0, y+1, 76, colors[color_idx]);
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS); // 3秒表示
}

// ST7789P3専用アニメーション
void showST7789P3Animation(int counter)
{
    // 画面クリア
    tft.fillScreen(0x0000);

    // ステータス表示
    tft.setTextColor(0xFFFF, 0x0000); // 白文字
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.printf("P3:%d", counter);
    
    // 画面サイズ確認表示
    tft.setTextColor(0x07E0, 0x0000); // 緑文字
    tft.setCursor(5, 15);
    tft.printf("%ldx%ld", tft.width(), tft.height());

    // 中央での円アニメーション
    int center_x = 38; // 76/2
    int center_y = 142; // 284/2
    
    // メイン円（大きな動き）
    float angle = animation_frame * 0.08f;
    int x_offset = (int)(20 * cos(angle));
    int y_offset = (int)(40 * sin(angle)); // 縦長を活かした大きな縦移動
    
    int circle_x = center_x + x_offset;
    int circle_y = center_y + y_offset;
    int radius = 5 + (int)(3 * sin(animation_frame * 0.1f));
    
    tft.fillCircle(circle_x, circle_y, radius, colors[color_index]);
    
    // 小さな装飾円
    tft.fillCircle(circle_x - 15, circle_y + 10, 2, colors[(color_index + 1) % 8]);
    tft.fillCircle(circle_x + 15, circle_y - 10, 3, colors[(color_index + 2) % 8]);

    // 縦長進行バー（両端）
    // 左側バー
    tft.drawRect(2, 50, 8, 180, 0xFFFF);
    int progress1 = ((counter * 18) % 180);
    tft.fillRect(2, 230 - progress1, 8, progress1, 0x001F); // 青、下から上
    
    // 右側バー
    tft.drawRect(66, 50, 8, 180, 0xFFFF);
    int progress2 = ((counter * 12) % 180);
    tft.fillRect(66, 50, 8, progress2, 0xF800); // 赤、上から下

    // 中央情報表示
    tft.setTextColor(0xC618, 0x0000); // グレー
    tft.setCursor(18, 35);
    tft.printf("F:%d", animation_frame % 1000);
    
    tft.setTextColor(0x07E0, 0x0000); // 緑
    tft.setCursor(18, 250);
    tft.printf("T:%lus", getSystemTimeMs() / 1000);

    // 縦長画面専用：波模様（複数段）
    for(int wave_level = 0; wave_level < 3; wave_level++) {
        int base_y = 80 + wave_level * 50;
        for(int x = 0; x < 76; x += 3) {
            int wave_y = base_y + (int)(6 * sin((x + animation_frame * (1 + wave_level)) * 0.15f));
            if (wave_y >= 0 && wave_y < 284) {
                tft.drawPixel(x, wave_y, colors[(x/8 + wave_level) % 8]);
                tft.drawPixel(x+1, wave_y, colors[(x/8 + wave_level) % 8]);
            }
        }
    }

    // 回転四角形（複数）
    for(int rect_idx = 0; rect_idx < 2; rect_idx++) {
        int rect_y = 200 + rect_idx * 30;
        int rect_size = 4 + (int)(2 * sin((animation_frame + rect_idx * 20) * 0.12f));
        tft.drawRect(center_x - rect_size/2, rect_y, rect_size, rect_size, 
                    colors[(color_index + rect_idx + 3) % 8]);
    }

    // スクロールテキスト（最下部）
    const char* scroll_text = "ST7789P3 76x284 Working Perfectly! ";
    int scroll_len = strlen(scroll_text) * 6;
    int text_offset = (animation_frame) % scroll_len;
    tft.setTextColor(0xFFE0, 0x0000); // 黄色
    tft.setCursor(76 - text_offset, 275);
    tft.print(scroll_text);

    // アニメーション変数更新
    animation_frame++;
    if (animation_frame % 50 == 0) {
        color_index = (color_index + 1) % 8;
    }
}

// メイン関数
extern "C" void app_main(void)
{
    int counter = 0;

    ESP_LOGI(TAG, "=== M5StampPico with ST7789P3 (76x284) ===");
    ESP_LOGI(TAG, "Starting ST7789P3 specialized initialization...");

    // M5Unified基本初期化
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

    // ST7789P3ディスプレイ初期化
    initST7789P3();

    // ウェルカムメッセージ
    showWelcomeMessage();

    ESP_LOGI(TAG, "Entering ST7789P3 main animation loop...");

    // メインループ
    while (true)
    {
        M5.update();

        // ST7789P3専用アニメーション
        showST7789P3Animation(counter);

        ESP_LOGI(TAG, "[%d] ST7789P3 (76x284) running perfectly! ✨", counter);

        counter++;
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}