/*
 * M5StampPico with Custom ST7789 Display Configuration
 * ESP32-PICO-D4 + ST7789 ディスプレイ完全対応版
 * M5GFXでST7789を正しく設定するにゃ！🎨
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

// ST7789ピン定義（あなたの配線に合わせて）
constexpr int PIN_SCL = 18; // SCLK (SPI Clock)
constexpr int PIN_SDA = 26; // MOSI (SDA)
constexpr int PIN_RST = 22; // Reset
constexpr int PIN_DC = 21;  // Data/Command
constexpr int PIN_CS = 19;  // Chip Select
constexpr int PIN_BLK = -1; // Backlight

// カスタムST7789ディスプレイクラス（LovyanGFXベース）
class LGFX_StampPico_ST7789 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance; // ST7789パネルインスタンス
    lgfx::Bus_SPI _bus_instance;        // SPIバスインスタンス
    lgfx::Light_PWM _light_instance;    // バックライト制御インスタンス

public:
    LGFX_StampPico_ST7789(void)
    {
        // SPIバス設定
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST; // VSPIホストを使用
            if (PIN_CS == -1)
            {
                cfg.spi_mode = 3;
            }
            else
            {
                cfg.spi_mode = 0;
            }
            cfg.spi_mode = 0;          // SPI mode 0
            cfg.freq_write = 40000000; // 書き込み時のクロック周波数 40MHz
            cfg.freq_read = 15000000;  // 読み込み時のクロック周波数 15MHz
            cfg.spi_3wire = true;      // 3線式SPIを使用しない
            cfg.use_lock = true;       // トランザクションロックを使用
            cfg.dma_channel = 1;       // DMAチャンネル自動選択
            cfg.pin_sclk = PIN_SCL;    // SCLKピン番号
            cfg.pin_mosi = PIN_SDA;    // MOSIピン番号
            cfg.pin_miso = -1;         // MISOピン（未使用）
            cfg.pin_dc = PIN_DC;       // D/Cピン番号
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ST7789パネル設定
        {
            uint16_t width = 240;
            uint16_t height = 240;
            auto cfg = _panel_instance.config();

            cfg.pin_cs = PIN_CS;
            cfg.pin_rst = PIN_RST;
            cfg.pin_busy = -1;

            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = width;
            cfg.panel_height = height;
            if (width == 80 && height == 160)
            {
                // 0.96
                cfg.offset_x = 52;
                cfg.offset_y = 40;
            }
            else
            {
                cfg.offset_x = 0;
                cfg.offset_y = 0;
            }
            cfg.offset_rotation = 2;
            cfg.dummy_read_pixel = 16;
            cfg.dummy_read_bits = 1;
            if (PIN_CS == -1)
            {
                cfg.readable = false;
            }
            else
            {
                cfg.readable = true;
            }
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        // バックライト設定
        {
        auto cfg = _light_instance.config();

        cfg.pin_bl = PIN_BLK;
        cfg.invert = false;
        cfg.freq   = 44100;
        cfg.pwm_channel = 7;

        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// カスタムディスプレイインスタンス
static LGFX_StampPico_ST7789 tft;

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

// ST7789ディスプレイの初期化
void initCustomST7789()
{
    ESP_LOGI(TAG, "Initializing Custom ST7789 Display...");

    // カスタムディスプレイを初期化
    tft.init();

    // 基本設定
    tft.setRotation(1);     // 横向き
    tft.setBrightness(128); // 明るさ調整
    tft.fillScreen(0x0000); // 背景を黒に

    ESP_LOGI(TAG, "Custom ST7789 Display initialized successfully!");
}

// ウェルカムメッセージを表示
void showWelcomeMessage()
{
    ESP_LOGI(TAG, "Displaying welcome message on ST7789...");

    tft.fillScreen(0x0000);           // 背景を黒に
    tft.setTextColor(0x07FF, 0x0000); // シアン文字、黒背景
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("M5StampPico");

    tft.setTextColor(0xFFE0, 0x0000); // 黄色文字、黒背景
    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.println("ESP32-PICO-D4");
    tft.setCursor(10, 55);
    tft.println("with ST7789");

    tft.setTextColor(0x07E0, 0x0000); // 緑文字、黒背景
    tft.setCursor(10, 75);
    tft.println("Custom Config!");

    tft.setTextColor(0xFFFF, 0x0000); // 白文字、黒背景
    tft.setCursor(10, 95);
    tft.println("Display Working!");

    vTaskDelay(3000 / portTICK_PERIOD_MS); // 3秒表示
}

// カラフルなアニメーション表示
void showAnimation(int counter)
{
    // 画面クリア
    tft.fillScreen(0x0000); // 黒

    // カウンター表示
    tft.setTextColor(0xFFFF, 0x0000); // 白文字、黒背景
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.printf("Count: %d", counter);

    // カラフルな円を描画（数学関数で滑らかなアニメーション）
    int center_x = 120;
    int center_y = 80;
    float angle = animation_frame * 0.1f;
    int x = center_x + (int)(30 * sin(angle));
    int y = center_y + (int)(20 * cos(angle));
    int radius = 8 + (int)(4 * sin(animation_frame * 0.2f));

    tft.fillCircle(x, y, radius, colors[color_index]);

    // 進行バー風の表示
    tft.drawRect(10, 140, 220, 10, 0xFFFF); // 白枠
    int progress = (counter * 22) % 220;
    tft.fillRect(10, 140, progress, 10, 0x001F); // 青進行バー

    // ステータス情報
    tft.setTextSize(1);
    tft.setTextColor(0xC618, 0x0000); // グレー文字、黒背景
    tft.setCursor(10, 160);
    tft.printf("Frame: %d", animation_frame);

    // 時間表示
    tft.setCursor(120, 160);
    tft.printf("Time: %lus", getSystemTimeMs() / 1000);

    // 追加の装飾：四角形のアニメーション
    int rect_size = 10 + (int)(5 * sin(animation_frame * 0.15f));
    tft.drawRect(180, 50, rect_size, rect_size, colors[(color_index + 2) % 8]);

    // アニメーション変数更新
    animation_frame++;
    if (animation_frame % 30 == 0)
    {
        color_index = (color_index + 1) % 8;
    }
}

// メイン関数
extern "C" void app_main(void)
{
    int counter = 0;

    ESP_LOGI(TAG, "=== M5StampPico with Custom ST7789 ===");
    ESP_LOGI(TAG, "Starting custom ST7789 initialization...");

    // M5Unified基本初期化（ディスプレイ無しモード）
    auto cfg = M5.config();
    cfg.clear_display = false; // ディスプレイクリアしない
    cfg.output_power = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    cfg.external_imu = false;
    cfg.external_rtc = false;
    M5.begin(cfg);

    // カスタムST7789ディスプレイ初期化
    initCustomST7789();

    // ウェルカムメッセージ
    showWelcomeMessage();

    ESP_LOGI(TAG, "Entering main animation loop...");

    // メインループ
    while (true)
    {
        // M5の更新処理
        M5.update();

        // カラフルなアニメーション表示
        showAnimation(counter);

        // シリアル出力
        ESP_LOGI(TAG, "[%d] M5StampPico + ST7789 running perfectly! ✨", counter);

        counter++;

        // 200ms間隔で更新（約5FPS、滑らかなアニメーション）
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}