/**
 * app_main.cpp
 *
 * 機能：
 * - dot_landscape.h を背景にして、new_cat.h の猫画像をボタンで操作
 * - M5StampPico 39番ボタン: 押すと猫がのびる
 * - MCP23008のスイッチ状態をバイナリ (01) で画面表示
 * - 統一描画システム: 全ての描画をキャンバスに行い、ループの最後に一括転送
 *
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <M5Unified.h>
#include "driver/gpio.h"
#include "esp_random.h"
#include "driver/i2c.h"
#include "nvs_flash.h"
#include "marumoni.h"
#include "buzzer.hpp"
#include "cat_naming.h"
#include "game_menu.h"

// ST7789P3ディスプレイとレトロゲームシステム
#include "LGFX_ST7789P3_76x284.hpp"
#include "RetroGamePaletteImage.hpp"

// 画像データをインクルード
#include "new_cat_body1.h" // 猫画像データ
#include "new_cat_head1.h" // 猫画像データ
#include "new_cat_head2.h" // 猫画像データ
#include "new_cat_sipo1.h" // 猫画像データ
#include "new_cat_sipo2.h" // 猫画像データ
#include "dot_landscape.h" // 背景画像データ
#include "kuji.h"
#include "icon_000.h"
#include "icon_001.h"
#include "icon_002.h"
#include "icon_003.h"
#include "icon_004.h"
#include "icon_005.h"
#include "icon_006.h"
#include "icon_007.h"
#include "miniicon_000.h"
#include "miniicon_001.h"
#include "miniicon_002.h"
#include "miniicon_003.h"

// ドライバーをインクルード
#include "mcp23008_driver.h"
#include <esp_timer.h>

static const char *TAG = "CatMovingGame";
static LGFX_ST7789P3_76x284 tft;

// 【新】統一描画システム用キャンバス
static M5Canvas canvas(&tft);                    // 描画バッファ用キャンバス
static PaletteImageRenderer *renderer = nullptr; // パレット画像レンダラー

// ゲーム設定
const int BUTTON_PIN = 39;     // M5StampPicoオンボードボタン
const int CAT_WIDTH = 96;      // 猫画像の幅
const int CAT_HEIGHT = 48;     // 猫画像の高さ
const int MOVE_SPEED = 2;      // 移動速度（ピクセル/フレーム）
const int FRAME_DELAY_MS = 50; // フレーム間隔（20FPS）

// MCP23008 I2C設定
const int I2C_MASTER_SCL_IO = 33; // SCLピン
const int I2C_MASTER_SDA_IO = 32; // SDAピン
const i2c_port_t I2C_MASTER_NUM = I2C_NUM_1;
const int I2C_MASTER_FREQ_HZ = 100000; // 100kHz
const uint8_t MCP23008_ADDR = 0x20;    // MCP23008のI2Cアドレス

// 【新】MCP23008ドライバーオブジェクト
MCP23008 mcpExpander(I2C_MASTER_NUM, MCP23008_ADDR);

// システム状態
bool mcp_available = false; // MCP23008が使用可能かどうか

// ゲーム状態
int catX = 0;                        // 猫のX座標
int catY = 0;                        // 猫のY座標（固定：中央）
int cat_length = 0;                  // 猫の体長
int max_cat_length = 0;              // 猫の最大体長
bool lastButtonState = false;        // 前回のボタン状態
unsigned long lastUpdateTime = 0;    // 最後の更新時間
bool cat_sippo_toggle = false;       // しっぽアニメーション用フラグ
uint8_t lever_switch_state = 0;      // レバースイッチ状態
uint8_t last_lever_switch_state = 0; // 前回のレバースイッチ状態
bool last_press_lever = false;       // 前回のレバースイッチ状態

int saidai_cat_length = 0; // 猫の最大体長

// スコアなど管理
uint64_t score = 0;   // スコア
uint8_t cat_name[10]; // 猫の名前（数字1='ア', 2='イ', ... 0=終端）
nvs_handle nvsHandle; // NVSハンドル

// おみくじの結果配列
const char *omikuji_results[] = {
    "猫吉",
    "スーパー吉",
    "大吉",
    "中吉",
    "小吉",
    "吉",
    "末吉",
    "凶",
    "大凶"};
const int OMIKUJI_COUNT = sizeof(omikuji_results) / sizeof(omikuji_results[0]);
int omikuji_result = 0;     // おみくじ結果 (0=未抽選, 1=大吉, 2=中吉, ...)
bool omikuji_shown = false; // おみくじ表示フラグ

// フォント読み込み状態
bool font_loaded = false; // フォントが読み込まれたかどうか

/**
 * メニュー背景保存用のグローバル関数宣言
 * game_menu.cppで実装される関数の宣言
 */
esp_err_t saveMenuBackground();
bool hasMenuBackgroundSaved();

/**
 * @file buzzer_debug.hpp
 * @brief ブザー診断用デバッグコード
 * @author 猫エンジニア
 * @date 2025年9月21日
 *
 * ブザーが音を出さない問題を診断するためのコード
 * app_main.cppに一時的に追加して原因を特定する
 */

#pragma once

#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * ブザー診断テスト
 * 段階的にテストして問題箇所を特定する
 */
void buzzer_diagnostic_test()
{
    const char *TAG = "BUZZER_DEBUG";
    ESP_LOGI(TAG, "=== ブザー診断テスト開始 ===");

    // テスト1: ピン番号の確認
    ESP_LOGI(TAG, "テスト1: ピン設定確認");
    const int TEST_PIN = 25; // M5StampPicoのG25
    ESP_LOGI(TAG, "使用ピン: GPIO%d", TEST_PIN);

    // ピンをGPIO出力として設定
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TEST_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO設定失敗: %s", esp_err_to_name(err));
        return;
    }

    // テスト2: GPIO直接制御テスト（クリック音）
    ESP_LOGI(TAG, "テスト2: GPIO直接制御（3回クリック）");
    for (int i = 0; i < 3; i++)
    {
        gpio_set_level((gpio_num_t)TEST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level((gpio_num_t)TEST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "クリック %d/3", i + 1);
    }

    // テスト3: PWM設定詳細チェック
    ESP_LOGI(TAG, "テスト3: PWM設定の詳細確認");

    // 既存のPWMタイマーを削除（競合回避）
    ledc_timer_rst(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);

    // PWMタイマー設定
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_config.timer_num = LEDC_TIMER_0;
    timer_config.duty_resolution = LEDC_TIMER_10_BIT; // より高い解像度
    timer_config.freq_hz = 1000;                      // 1kHz
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    err = ledc_timer_config(&timer_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "PWMタイマー設定失敗: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "PWMタイマー設定成功");

    // PWMチャンネル設定
    ledc_channel_config_t channel_config = {};
    channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_config.channel = LEDC_CHANNEL_0;
    channel_config.timer_sel = LEDC_TIMER_0;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.gpio_num = TEST_PIN;
    channel_config.duty = 512; // 50% duty (0-1023)
    channel_config.hpoint = 0;

    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "PWMチャンネル設定失敗: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "PWMチャンネル設定成功");

    // テスト4: 異なる周波数でのPWMテスト
    ESP_LOGI(TAG, "テスト4: PWM周波数テスト");

    uint32_t test_frequencies[] = {100, 440, 1000, 2000, 4000};
    int num_tests = sizeof(test_frequencies) / sizeof(test_frequencies[0]);

    for (int i = 0; i < num_tests; i++)
    {
        uint32_t freq = test_frequencies[i];
        ESP_LOGI(TAG, "周波数テスト: %ld Hz", freq);

        // 周波数設定
        err = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "周波数%ld Hz設定失敗: %s", freq, esp_err_to_name(err));
            continue;
        }

        // PWM開始
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "PWM duty設定失敗: %s", esp_err_to_name(err));
            continue;
        }

        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "PWM duty更新失敗: %s", esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(TAG, "%ld Hz で 1秒間PWM出力中...", freq);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // PWM停止
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        vTaskDelay(pdMS_TO_TICKS(500)); // 間隔
    }

    // テスト5: 異なるピンでのテスト（ピン問題の確認）
    ESP_LOGI(TAG, "テスト5: 他のピンでのテスト");
    int alternative_pins[] = {26, 27, 14, 12}; // M5StampPicoで使用可能そうなピン
    int num_alt_pins = sizeof(alternative_pins) / sizeof(alternative_pins[0]);

    for (int i = 0; i < num_alt_pins; i++)
    {
        int alt_pin = alternative_pins[i];
        ESP_LOGI(TAG, "代替ピンテスト: GPIO%d", alt_pin);

        // ピン変更
        channel_config.gpio_num = alt_pin;
        err = ledc_channel_config(&channel_config);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "GPIO%d設定失敗: %s", alt_pin, esp_err_to_name(err));
            continue;
        }

        // 440Hz で短時間テスト
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 440);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ESP_LOGI(TAG, "GPIO%d で 440Hz 0.5秒出力", alt_pin);
        vTaskDelay(pdMS_TO_TICKS(500));

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        vTaskDelay(pdMS_TO_TICKS(300));
    }

    // PWM停止
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);

    ESP_LOGI(TAG, "=== ブザー診断テスト完了 ===");
    ESP_LOGI(TAG, "音が聞こえた場合：");
    ESP_LOGI(TAG, "  - どのテストで音が出たか確認してください");
    ESP_LOGI(TAG, "  - 特定のピンでのみ音が出る場合、配線を確認");
    ESP_LOGI(TAG, "音が全く聞こえない場合：");
    ESP_LOGI(TAG, "  - ブザーの配線を確認（VCC, GND, 信号線）");
    ESP_LOGI(TAG, "  - ブザーの種類を確認（アクティブ/パッシブ）");
    ESP_LOGI(TAG, "  - 電源電圧を確認（3.3V/5V）");
}

/**
 * 簡易ブザーテスト（app_main.cppで呼び出し用）
 * initGame()の後に一度だけ呼び出してください
 */
void test_buzzer_simple()
{
    const char *TAG = "BUZZER_TEST";
    ESP_LOGI(TAG, "簡易ブザーテスト開始...");

    // 直接PWMでテスト
    const int pin = 25;

    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.timer_num = LEDC_TIMER_1; // タイマー1を使用（競合回避）
    timer_conf.duty_resolution = LEDC_TIMER_8_BIT;
    timer_conf.freq_hz = 1000;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {};
    ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_conf.channel = LEDC_CHANNEL_1; // チャンネル1を使用（競合回避）
    ch_conf.timer_sel = LEDC_TIMER_1;
    ch_conf.intr_type = LEDC_INTR_DISABLE;
    ch_conf.gpio_num = pin;
    ch_conf.duty = 128;
    ch_conf.hpoint = 0;
    ledc_channel_config(&ch_conf);

    // 440Hz（ラの音）を2秒間
    ESP_LOGI(TAG, "440Hz ブザーテスト（2秒間）...");
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, 440);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    vTaskDelay(pdMS_TO_TICKS(2000));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ESP_LOGI(TAG, "ブザーテスト完了");
}

/**
 *
 * フォントデータをM5Canvasに読み込む
 */
bool initCustomFont()
{
    ESP_LOGI(TAG, "Loading custom VLW font...");

    // M5CanvasにVLWフォントを読み込み
    // marumoni配列からフォントデータを読み込む
    canvas.loadFont(marumoni); // VLWバイナリデータを直接指定
    canvas.setTextSize(1);     // フォントサイズ1を設定

    // TFT画面にも同じフォントを読み込む（念のため）
    tft.loadFont(marumoni);
    tft.setTextSize(1); // フォントサイズ1を設定

    font_loaded = true;
    ESP_LOGI(TAG, "Custom VLW font loaded successfully!");
    return true;
}

/**
 * カスタムテキスト描画（中央基準）
 * @param text 描画するテキスト
 * @param x 中央基準のX座標
 * @param y 中央基準のY座標
 * @param color テキストカラー（RGB565）
 */
void drawCustomTextCenter(const char *text, int x, int y, uint16_t color)
{
    if (!font_loaded)
    {
        canvas.setTextDatum(MC_DATUM);
        canvas.setTextColor(color);
        canvas.drawString(text, x, y);
        return;
    }

    canvas.setTextColor(color);
    canvas.setTextDatum(MC_DATUM); // 中央基準
    canvas.drawString(text, x, y);
}

/**
 * 統一描画システム初期化
 * 画面サイズのキャンバスを作成して描画バッファとして使用するにゃ
 */
bool initUnifiedDrawingSystem()
{
    ESP_LOGI(TAG, "Initializing unified drawing system...");

    // キャンバスを画面サイズで作成（16bit色）
    if (!canvas.createSprite(tft.width(), tft.height()))
    {
        ESP_LOGE(TAG, "Failed to create canvas sprite");
        return false;
    }

    // パレット画像レンダラーを初期化（キャンバス用）
    renderer = new PaletteImageRenderer(&tft, &canvas);
    if (!renderer)
    {
        ESP_LOGE(TAG, "Failed to create palette image renderer");
        return false;
    }

    // カスタムフォントを読み込み
    if (!initCustomFont())
    {
        ESP_LOGI(TAG, "Failed to load custom font, using default font");
        font_loaded = false;
    }

    ESP_LOGI(TAG, "Unified drawing system initialized: %ldx%ld canvas",
             canvas.width(), canvas.height());
    return true;
}

/**
 * キャンバスをクリアして描画準備
 * 毎フレームの最初に呼び出して描画バッファをクリアするにゃ
 */
void clearDrawingCanvas()
{
    // キャンバスを黒でクリア
    canvas.fillScreen(TFT_BLACK);

    // レンダラーのキャンバスもクリア
    if (renderer)
    {
        renderer->clearCanvas(0x0000);
    }
}

/**
 * キャンバスをLCDに転送
 * 全ての描画が完了した後、一度だけ呼び出してLCDに表示するにゃ
 */
void pushCanvasToLCD()
{
    // パレット画像をキャンバスに反映
    if (renderer)
    {
        renderer->pushCanvasToDisplayOpaque(0, 0);
    }

    // キャンバス全体をLCDに転送
    canvas.pushSprite(0, 0);
}

/**
 * I2Cバススキャン（診断用）
 * 接続されているI2Cデバイスのアドレスをすべて検出しますにゃ
 */
void scanI2CDevices()
{
    ESP_LOGI(TAG, "=== I2C Bus Scanner ===");
    ESP_LOGI(TAG, "Scanning I2C bus on SDA=%d, SCL=%d...", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    int found_devices = 0;

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Found I2C device at address 0x%02X", addr);
            found_devices++;

            // MCP23008の可能性をチェック
            if (addr >= 0x20 && addr <= 0x27)
            {
                uint8_t a2 = (addr - 0x20) >> 2;
                uint8_t a1 = ((addr - 0x20) >> 1) & 1;
                uint8_t a0 = (addr - 0x20) & 1;
                ESP_LOGI(TAG, "  -> Possible MCP23008 (A2=%d, A1=%d, A0=%d)", a2, a1, a0);
            }
        }
    }

    if (found_devices == 0)
    {
        ESP_LOGW(TAG, "No I2C devices found!");
        ESP_LOGW(TAG, "Check wiring: SDA, SCL, VCC, GND, and pull-up resistors");
    }
    else
    {
        ESP_LOGI(TAG, "I2C scan complete: %d device(s) found", found_devices);
    }
    ESP_LOGI(TAG, "=== End I2C Scanner ===");
}

esp_err_t initI2C()
{
    ESP_LOGI(TAG, "Initializing I2C for MCP23008...");

    // M5Unifiedが既にI2Cを初期化している可能性をチェック
    esp_err_t err = i2c_driver_delete(I2C_MASTER_NUM);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Existing I2C driver deleted");
    }

    // I2C設定
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0; // ESP32で推奨

    // パラメータ設定
    err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    // ドライバーインストール
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C initialized successfully (SCL=%d, SDA=%d)",
             I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    return ESP_OK;
}

/**
 * おみくじ抽選処理
 */
int drawOmikuji()
{
    uint32_t randomValue = esp_random();
    int result = (randomValue % OMIKUJI_COUNT) + 1;
    ESP_LOGI(TAG, "Omikuji drawn: %d (%s)", result, omikuji_results[result - 1]);
    return result;
}

/**
 * おみくじ結果をキャンバスに描画
 * 直接LCDではなく、キャンバスに描画するにゃ
 */
void drawOmikujiResultToCanvas()
{
    if (omikuji_result > 0 && omikuji_result <= OMIKUJI_COUNT)
    {
        u_int32_t draw_start_point = new_cat_sipo1_width + new_cat_body1_width + new_cat_head1_width + cat_length - 5;
        // おみくじ表示
        RetroColorPalette kujiPalette;
        kujiPalette.initBasicColors(); // 基本パレット

        // おみくじ画像データを作成
        PaletteImageData kujiImg(kuji_data, kuji_width, kuji_height, &kujiPalette);

        renderer->drawToCanvas(kujiImg, draw_start_point, 35, true);

        // キャンバス中央にテキスト表示
        canvas.setTextColor(TFT_BLACK);
        canvas.setTextDatum(MC_DATUM);
        canvas.setTextSize(1);

        // 結果をキャンバス中央に描画
        const char *result_text = omikuji_results[omikuji_result - 1];
        canvas.drawString(result_text, draw_start_point + (kuji_width / 2), 35 + (kuji_height / 2));
        canvas.setTextSize(1);
        ESP_LOGD(TAG, "Omikuji result drawn to canvas: %s", result_text);
    }
}

/**
 * MCP23008スイッチ状態をキャンバスにバイナリ表示
 * 直接LCDではなく、キャンバスにスイッチの状態を描画するにゃ
 * GP4 GP3 GP2 GP1 GP0 の順で表示 (例: "01101")
 */
void drawSwitchStateToCanvas()
{
    // MCP23008が使用可能かチェック
    if (!mcp_available)
    {
        // MCP23008が使用できない場合は "-----" 表示
        canvas.setTextColor(TFT_RED);
        canvas.setTextDatum(TL_DATUM);
        canvas.drawString("-err-", 230, 5);
        return;
    }

    lever_switch_state = 0;

    // 新しいMCP23008ドライバーを使用してスイッチ状態を読み取り
    esp_err_t err = mcpExpander.readSwitches(&lever_switch_state);

    if (err == ESP_OK)
    {
        // スイッチ状態をバイナリ文字列に変換
        // 注意：スイッチが押されている場合は0、離されている場合は1
        // 表示は押されている場合を1、離されている場合を0にするため反転
        char switch_text[6];
        for (int i = 4; i >= 0; i--)
        {
            // ビットの状態を取得して反転 (0→1, 1→0)
            bool bit_state = ((lever_switch_state >> i) & 1) == 0; // プルアップなので反転
            switch_text[4 - i] = bit_state ? '*' : '_';
        }
        switch_text[5] = '\0'; // 文字列終端

        // キャンバス左上にバイナリ表示
        canvas.setTextColor(TFT_YELLOW);
        canvas.setTextDatum(TL_DATUM);
        canvas.drawString(switch_text, 240, 5);

        // ESP_LOGI(TAG, "Switch binary display: %s (raw: 0x%02X)", switch_text, lever_switch_state);
    }
    else
    {
        // エラー時は "ERROR" 表示
        canvas.setTextColor(TFT_RED);
        canvas.setTextDatum(TL_DATUM);
        canvas.drawString("ERROR", 5, 5);

        ESP_LOGE(TAG, "Failed to read switch state: %s", esp_err_to_name(err));
    }
}

/**
 * ボタン初期化
 * M5StampPicoの39番ピンを入力プルアップに設定
 */
void initButton()
{
    // GPIO39をプルアップ入力として設定
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << BUTTON_PIN);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE; // 内部プルアップ有効
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);

    ESP_LOGI(TAG, "Button initialized on GPIO%d with internal pullup", BUTTON_PIN);
}

/**
 * ボタン状態読み取り
 * @return true: 押されている, false: 押されていない
 */
bool readButton()
{
    // GPIO39の状態を読み取り（プルアップなので、押下時は0、離した時は1）
    return (gpio_get_level((gpio_num_t)BUTTON_PIN) == 0);
}

/**
 * 背景画像をキャンバスに描画
 * dot_landscape.h を背景として描画するにゃ
 */
void drawBackgroundToCanvas()
{
    if (!renderer)
        return;

    // GameBoyカラーパレットで背景を描画
    RetroColorPalette bgPalette;
    bgPalette.initGameBoyColors();

    // 背景画像データを作成
    PaletteImageData bgImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, &bgPalette);

    // 背景をキャンバス中央に配置
    int bgX = (canvas.width() - dot_landscape_width) / 2;
    int bgY = (canvas.height() - dot_landscape_height) / 2;

    // 座標制限（画面からはみ出さないように）
    if (bgX < 0)
        bgX = 0;
    if (bgY < 0)
        bgY = 0;
    if (bgX > canvas.width() - dot_landscape_width)
    {
        bgX = canvas.width() - dot_landscape_width;
    }
    if (bgY > canvas.height() - dot_landscape_height)
    {
        bgY = canvas.height() - dot_landscape_height;
    }

    // 背景を描画（透明色対応）
    renderer->drawToCanvas(bgImage, bgX, bgY, true);
}

/**
 * 猫画像をキャンバスに描画
 * new_cat_sipo1, new_cat_body1, new_cat_head1 を横並びに描画するにゃ
 */
void drawCatToCanvas(int x, int y)
{
    if (!renderer)
        return;

    // 猫専用カラーパレット（緑系グラデーション）
    RetroColorPalette catPalette;
    catPalette.initGameBoyColors(); // 基本GameBoyパレット

    // 猫の各部位画像データを作成
    PaletteImageData catSipoImage(new_cat_sipo1_data, new_cat_sipo1_width, new_cat_sipo1_height, &catPalette);
    if (cat_sippo_toggle)
    {
        catSipoImage = PaletteImageData(new_cat_sipo2_data, new_cat_sipo2_width, new_cat_sipo2_height, &catPalette);
    }
    PaletteImageData catBodyImage(new_cat_body1_data, new_cat_body1_width, new_cat_body1_height, &catPalette);
    PaletteImageData catHeadImage(nullptr, 0, 0, &catPalette);
    if (cat_length == 0)
    {
        catHeadImage = PaletteImageData(new_cat_head1_data, new_cat_head1_width, new_cat_head1_height, &catPalette);
    }
    else
    {
        catHeadImage = PaletteImageData(new_cat_head2_data, new_cat_head2_width, new_cat_head2_height, &catPalette);
    }

    // 左から順番に描画（しっぽ、体、頭）
    int currentX = x;
    renderer->drawToCanvas(catSipoImage, currentX, y, true);
    currentX += new_cat_sipo1_width - 1;

    // bodyを中間部分に繰り返し描画してcat_lengthに応じて隙間を埋める
    int bodyRepeatCount = cat_length / (new_cat_body1_width - 1);
    for (int i = 0; i <= bodyRepeatCount; i++)
    {
        renderer->drawToCanvas(catBodyImage, currentX, y, true);
        currentX += new_cat_body1_width - 1;
    }
    renderer->drawToCanvas(catBodyImage, (new_cat_sipo1_width - 1) + cat_length, y, true);

    int cat_length_headless = (new_cat_sipo1_width - 1) + (new_cat_body1_width - 1) + cat_length;
    // 最後にheadを描画
    renderer->drawToCanvas(catHeadImage, cat_length_headless, y, true);
}

/**
 * 猫の伸び具合を更新
 * ボタン状態に応じて移動するにゃ
 */
void updateCatLength()
{
    bool buttonPressed = readButton();
    bool press_lever = (~lever_switch_state & 0b00000100) != 0;
    // ESP_LOGI(TAG, "lever state: %02x", lever_switch_state);
    //  ボタン状態の変化をログ出力（デバッグ用）
    if (press_lever != last_press_lever)
    {

        // ボタンが押された瞬間におみくじ抽選
        if (press_lever && omikuji_result == 0)
        {
            omikuji_result = drawOmikuji();
        }

        last_press_lever = press_lever;
    }

    // 移動処理
    if (press_lever)
    {
        // ボタンが押されている間：右に移動
        cat_length += MOVE_SPEED;
        if (cat_length > max_cat_length)
        {
            max_cat_length = cat_length;
        }

        if (cat_length > saidai_cat_length)
        {
            cat_length = saidai_cat_length;
            omikuji_shown = true; // 最大長になったらおみくじ表示
        }
    }
    else
    {
        // 縮んでいる最中ならtrue
        bool cat_length_was_aru = false;
        if (cat_length > 0)
        {
            cat_length_was_aru = true;
        }
        // ボタンが離されている間：左に移動
        cat_length -= MOVE_SPEED;

        // 左端制限（x=0より左に行かない）
        if (cat_length <= 0)
        {
            cat_length = 0;
            // 猫が完全に縮んだ時にリセット
            omikuji_result = 0;
            omikuji_shown = false;

            // スコア確定
            if (cat_length_was_aru)
            {
                score += max_cat_length;
                ESP_LOGI(TAG, "Score updated: %llu", score);
                max_cat_length = 0;

                // NVSにスコア保存
                esp_err_t ret = nvs_set_i64(nvsHandle, "score", score);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error saving score to NVS: %s", esp_err_to_name(ret));
                }
                else
                {
                    nvs_commit(nvsHandle);
                    ESP_LOGI(TAG, "Score saved to NVS");
                }
            }
        }
    }
}

/**
 * 【統一】ゲーム画面をキャンバスに描画
 * 全ての描画要素をキャンバスに描画してから、最後にLCDに転送するにゃ
 */
void drawGameScreen()
{
    // キャンバスをクリア
    clearDrawingCanvas();

    // 背景を描画
    drawBackgroundToCanvas();

    // 猫を描画（背景の上に重ねる）
    drawCatToCanvas(catX, catY);

    // スイッチの状態を描画
    drawSwitchStateToCanvas();

    // スコア表示
    char score_text[20];
    /*
    snprintf(score_text, sizeof(score_text), "[%llumm]", score);
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString(score_text, 0, 0);
    */

    // 猫の名前表示
    char display_name[64];
    CatNaming tempNaming(&tft, &canvas, &mcpExpander, nvsHandle, renderer);
    tempNaming.convertNameToDisplayString(cat_name, display_name, sizeof(display_name));

    char name_text[80];
    snprintf(name_text, sizeof(name_text), "%s[%llumm]", display_name, score);

    canvas.setTextColor(TFT_CYAN);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString(name_text, 0, 0); // スコアの下に表示

    // おみくじ表示中なら描画
    if (omikuji_shown)
    {
        drawOmikujiResultToCanvas();
    }

    // 最後にキャンバス全体をLCDに転送（一度だけ！）
    pushCanvasToLCD();

    // ESP_LOGD(TAG, "Game screen drawn: catX=%d, catY=%d, length=%d", catX, catY, cat_length);
}

/**
 * ゲーム初期化
 */
void initGame()
{
    ESP_LOGI(TAG, "Initializing game components...");

    // ディスプレイ初期化
    tft.init();
    tft.fillScreen(TFT_BLACK);

    ESP_LOGI(TAG, "Display initialized: %ldx%ld", tft.width(), tft.height());

    // 統一描画システム初期化
    if (!initUnifiedDrawingSystem())
    {
        ESP_LOGE(TAG, "Failed to initialize unified drawing system");
        return;
    }

    // ボタン初期化
    initButton();

    // 猫の初期位置設定（画面中央縦、左端横）
    catX = 0;                             // 左端からスタート
    catY = tft.height() - CAT_HEIGHT - 2; // 縦方向中央

    // 座標制限
    if (catY < 0)
        catY = 0;
    if (catY > tft.height() - CAT_HEIGHT)
    {
        catY = tft.height() - CAT_HEIGHT;
    }

    // I2C初期化 (MCP23008用)
    ESP_LOGI(TAG, "Attempting to initialize I2C for MCP23008...");
    esp_err_t err = initI2C();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "I2C initialization failed - MCP23008 will be disabled");
        ESP_LOGW(TAG, "Game will continue without switch input display");
        mcp_available = false;
    }
    else
    {
        // 【追加】I2Cバススキャンを実行
        scanI2CDevices();

        // 【更新】MCP23008初期化
        ESP_LOGI(TAG, "Attempting to initialize MCP23008...");
        err = mcpExpander.begin();
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "MCP23008 initialization failed - switch input disabled");
            ESP_LOGW(TAG, "Possible causes: wiring, power supply, or device address");
            ESP_LOGW(TAG, "Check the I2C scan results above for detected devices");
            mcp_available = false;
        }
        else
        {
            ESP_LOGI(TAG, "MCP23008 initialized successfully!");
            mcp_available = true;
        }
    }

    // ゲーム初期状態設定
    catX = 0;
    catY = tft.height() - CAT_HEIGHT - 2; // 画面中央のY座標
    cat_length = 0;
    lastButtonState = false;
    lastUpdateTime = 0;

    ESP_LOGI(TAG, "Game initialization completed (MCP23008: %s)",
             mcp_available ? "ENABLED" : "DISABLED");

    // 最大長制限（頭部が画面外に出ない範囲）
    saidai_cat_length = canvas.width() - (new_cat_sipo1_width - 1) - (new_cat_body1_width - 1);

    // ブザー初期化
    ESP_LOGI(TAG, "Initializing buzzer...");
    esp_err_t buzzer_err = buzzer_init();
    if (buzzer_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Buzzer initialization failed: %s", esp_err_to_name(buzzer_err));
        ESP_LOGW(TAG, "Game will continue without sound effects");
    }
    else
    {
        ESP_LOGI(TAG, "Buzzer initialized successfully");
        // 起動音を再生
        // buzzer_play_sound(BUZZER_SOUND_STARTUP);
    }
    // ブザーテスト（必要に応じてコメントアウト）
    // buzzer_play_sound(BUZZER_SOUND_STARTUP);

    // NVS初期化
    nvs_flash_init();

    // ボタンを押したまま起動したら、NVSの内容をリセット
    if (readButton()) // 初期状態読み取り
    {
        esp_err_t ret = nvs_open("save_data", NVS_READWRITE, &nvsHandle);
        nvs_erase_all(nvsHandle);
        score = 0;
    }
    // それからNVSオープン
    esp_err_t ret = nvs_open("save_data", NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(ret));
    }
    else
    {
        // スコア読み出し
        int64_t saved_score = 0;
        ret = nvs_get_i64(nvsHandle, "score", &saved_score);
        if (ret == ESP_OK)
        {
            score = saved_score;
            ESP_LOGI(TAG, "Loaded score from NVS: %llu", score);

            // 猫の名前も読み込み
            CatNaming tempNaming(&tft, &canvas, &mcpExpander, nvsHandle, renderer);
            esp_err_t name_result = tempNaming.loadNameFromNVS(cat_name);
            if (name_result == ESP_OK)
            {
                char display_name[64];
                tempNaming.convertNameToDisplayString(cat_name, display_name, sizeof(display_name));
                ESP_LOGI(TAG, "猫の名前を読み込み: %s", display_name);
            }
            else
            {
                ESP_LOGW(TAG, "猫の名前読み込み失敗、デフォルト名使用");
                cat_name[0] = 49; // ネ
                cat_name[1] = 15; // コ
                cat_name[2] = 0;  // 終端
            }
        }
        else if (ret == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "No saved score found in NVS, starting fresh");
            score = 0;

            // 初回起動時は名前を決める
            ESP_LOGI(TAG, "初回起動検出：猫の名前付けを開始します");

            // 猫名前付けシステム実行
            esp_err_t naming_result = runCatNamingSystem(&tft, &canvas, &mcpExpander, nvsHandle, cat_name);
            if (naming_result == ESP_OK)
            {
                char display_name[64];
                // CatNamingクラスのstatic関数として変換機能を使いたい場合は
                // 一時的にCatNamingオブジェクトを作成して変換
                CatNaming tempNaming(&tft, &canvas, &mcpExpander, nvsHandle, renderer);
                tempNaming.convertNameToDisplayString(cat_name, display_name, sizeof(display_name));
                ESP_LOGI(TAG, "猫の名前が決まりました: %s", display_name);
            }
            else
            {
                ESP_LOGW(TAG, "名前付けに失敗、デフォルト名を使用");
                cat_name[0] = 49; // ネ
                cat_name[1] = 15; // コ
                cat_name[2] = 0;  // 終端
            }
        }
        else
        {
            ESP_LOGE(TAG, "Error reading score from NVS: %s", esp_err_to_name(ret));
            score = 0;
        }
    }

    // メニューシステム初期化
    ESP_LOGI(TAG, "Initializing game menu system...");
    esp_err_t menu_err = initGameMenu(&tft, &canvas, &mcpExpander, renderer, nvsHandle);
    if (menu_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Menu system initialization failed: %s", esp_err_to_name(menu_err));
        ESP_LOGW(TAG, "Game will continue without menu functionality");
    }
    else
    {
        ESP_LOGI(TAG, "Menu system initialized successfully");
    }
}

/**
 * メインループ
 */
void gameLoop()
{
    ESP_LOGI(TAG, "Starting game loop...");

    // 初回描画
    drawGameScreen();

    while (true)
    {
        unsigned long currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // フレームレート制御
        if (currentTime - lastUpdateTime >= FRAME_DELAY_MS)
        {
            // メニュー起動判定
            if (checkMenuActivation(lever_switch_state))
            {
                ESP_LOGI(TAG, "Activating game menu...");

                // === 【重要】メニュー起動前に現在の画面を背景として保存 ===
                esp_err_t save_result = saveMenuBackground();
                if (save_result != ESP_OK)
                {
                    ESP_LOGI(TAG, "Failed to save menu background: %s", esp_err_to_name(save_result));
                    ESP_LOGI(TAG, "Menu will use default background");
                }
                else
                {
                    ESP_LOGI(TAG, "Menu background saved successfully");
                }

                menu_result_t menu_result = executeGameMenu();

                // メニュー結果の処理
                switch (menu_result)
                {
                case MENU_RESULT_USE_SPEED_UP:
                    // スピードアップ効果を適用
                    // 例：移動速度を一時的に増加
                    ESP_LOGI(TAG, "Speed up effect activated!");
                    break;

                case MENU_RESULT_USE_SPEED_DOWN:
                    // スピードダウン効果を適用
                    ESP_LOGI(TAG, "Speed down effect activated!");
                    break;

                case MENU_RESULT_USE_BONUS:
                    // ボーナス効果を適用（例：スコア倍増）
                    score += 1000; // ボーナスポイント
                    ESP_LOGI(TAG, "Bonus effect activated! Score: %llu", score);
                    break;

                default:
                    ESP_LOGI(TAG, "Menu closed without action");
                    break;
                }

                // メニュー終了後、少し待機してからゲーム再開
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            // 猫の伸び具合更新
            updateCatLength();

            // 【統一】画面描画（一箇所ですべての描画を行う）
            drawGameScreen();

            // 時間更新
            lastUpdateTime = currentTime;

            // デバッグ情報（1秒に1回）
            static unsigned long lastDebugTime = 0;
            if (currentTime - lastDebugTime >= 1000)
            {
                cat_sippo_toggle = !cat_sippo_toggle; // しっぽアニメーション切替
                // ESP_LOGI(TAG, "Cat position: (%d, %d), Button: %s",
                //         catX, catY, readButton() ? "PRESSED" : "RELEASED");
                lastDebugTime = currentTime;
            }
        }

        // CPU負荷軽減のため短時間待機
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * メイン関数
 */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Cat Moving Game with Unified Drawing System Starting ===");

    // 初期化待機
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ゲーム初期化 (エラーが発生してもゲームは続行)
    initGame();

    // 初期画面表示
    ESP_LOGI(TAG, "Drawing initial screen...");
    drawGameScreen();
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 状態表示
    ESP_LOGI(TAG, "Game status: Display=OK, Button=OK, MCP23008=%s",
             mcp_available ? "OK" : "DISABLED");

    // ゲームループ開始
    ESP_LOGI(TAG, "Starting unified drawing system game loop...");
    gameLoop();
}