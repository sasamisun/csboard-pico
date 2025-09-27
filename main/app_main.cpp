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
#include "new_cat_head3.h" // 猫画像データ
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
uint8_t move_speed = 2;        // 移動速度（ピクセル/フレーム）
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
// item_counts_t item_counts = {0, 0, 0}; // アイテム所持数
bool omiku_sound_played = false; // おみくじ音再生フラグ
// bool sound_enabled = false;            // サウンド有効フラグ
uint8_t score_bonus_a = 0; // スコアボーナスA
uint8_t score_bonus_b = 0; // スコアボーナスB

int saidai_cat_length = 0; // 猫の最大体長

// スコアなど管理
uint64_t score = 0;   // スコア
uint8_t cat_name[10]; // 猫の名前（数字1='ア', 2='イ', ... 0=終端）
nvs_handle nvsHandle; // NVSハンドル

// おみくじの結果配列
// 演出パターンの定義
typedef enum
{
    PATTERN_A_GEKIATSU,   // 激アツ演出（あたり）
    PATTERN_B_NORMAL_WIN, // 演出なし（あたり）
    PATTERN_C_ATSUI,      // ちょっとアツい演出（外れ）
    PATTERN_D_NORMAL_LOSE // 演出なし（外れ）
} omikuji_pattern_t;

// おみくじ結果の構造体
typedef struct
{
    int result;                // 結果番号（1-9）
    omikuji_pattern_t pattern; // 演出パターン
    bool is_win;               // アイテムゲット有無
    const char *pattern_name;  // 演出名（デバッグ用）
    uint8_t item_index;        // アイテムインデックス（0=なし, 1=スピードアップ, 2=スピードダウン, 3=ボーナス）
    uint8_t sound_index;       // サウンドインデックス（0=なし, 1=あたり音, 2=外れ音, 3=すごい音）
    uint8_t effect_index;      // エフェクトインデックス（0=なし, 1=エフェクト1, 2=エフェクト2, 3=エフェクト3）
    uint8_t cat_face;          // 猫顔インデックス（0=通常, 1=喜び）
} omikuji_draw_result_t;

// おみくじの結果配列（既存のものをそのまま使用）
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

// 演出パターン名の配列
const char *pattern_names[] = {
    "激アツ演出",
    "演出なし(あたり)",
    "ちょっとアツい演出",
    "演出なし(外れ)"};

// パターンA用の確率テーブル（猫吉30%, スーパー吉70%）
const int pattern_a_weights[] = {30, 70}; // 猫吉, スーパー吉
const int pattern_a_results[] = {1, 2};   // インデックス（1始まり）
const int pattern_a_count = 2;

// パターンB用の確率テーブル（猫吉100%）
const int pattern_b_weights[] = {100}; // 猫吉
const int pattern_b_results[] = {1};   // インデックス（1始まり）
const int pattern_b_count = 1;

// パターンC用の確率テーブル（大吉10%, 中吉10%, 小吉30%, 吉30%, 末吉20%）
const int pattern_c_weights[] = {10, 10, 30, 30, 20}; // 大吉, 中吉, 小吉, 吉, 末吉
const int pattern_c_results[] = {3, 4, 5, 6, 7};      // インデックス（1始まり）
const int pattern_c_count = 5;

// パターンD用の確率テーブル（小吉20%, 吉20%, 末吉20%, 凶20%, 大凶20%）
const int pattern_d_weights[] = {20, 20, 20, 20, 20}; // 小吉, 吉, 末吉, 凶, 大凶
const int pattern_d_results[] = {5, 6, 7, 8, 9};      // インデックス（1始まり）
const int pattern_d_count = 5;

omikuji_draw_result_t omikuji_result = {0};
bool omikuji_shown = false; // おみくじ表示フラグ

// フォント読み込み状態
bool font_loaded = false; // フォントが読み込まれたかどうか

unsigned long omikuji_hide_timer = 0;                // おみくじ非表示タイマー
bool omikuji_delay_hiding = false;                   // 遅延非表示モード中かどうか
const unsigned long OMIKUJI_DISPLAY_DELAY_MS = 1000; // 1秒間表示継続
uint16_t omikuji_animation_frame = 0;                // 宝箱オープンアニメーションフレーム

/**
 * メニュー背景保存用のグローバル関数宣言
 * game_menu.cppで実装される関数の宣言
 */
esp_err_t saveMenuBackground();
bool hasMenuBackgroundSaved();

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
 * 重み付き抽選を行う関数
 * @param weights 重みの配列
 * @param results 結果の配列
 * @param count 配列の要素数
 * @return 選択された結果
 */
int drawWeightedRandom(const int *weights, const int *results, int count)
{
    // 重みの合計を計算
    int total_weight = 0;
    for (int i = 0; i < count; i++)
    {
        total_weight += weights[i];
    }

    // 0からtotal_weight-1の範囲で乱数生成
    uint32_t random_value = esp_random() % total_weight;

    // 重みに基づいて選択
    int current_weight = 0;
    for (int i = 0; i < count; i++)
    {
        current_weight += weights[i];
        if (random_value < current_weight)
        {
            return results[i];
        }
    }

    // 念のため最後の要素を返す
    return results[count - 1];
}

/**
 * 1段階目：当たり外れ抽選
 * @return true:あたり(5%), false:外れ(95%)
 */
bool checkWinLose()
{
    uint32_t random_value = esp_random() % 100;
    bool is_win = (random_value < 5); // 5%で当たり

    ESP_LOGI(TAG, "Win/Lose lottery: %s (roll: %ld/100)",
             is_win ? "WIN" : "LOSE", random_value);

    return is_win;
}

/**
 * 2段階目：演出抽選
 * @param is_win 当たりかどうか
 * @return 演出パターン
 */
omikuji_pattern_t drawPattern(bool is_win)
{
    uint32_t random_value = esp_random() % 100;
    omikuji_pattern_t pattern;

    if (is_win)
    {
        // あたりの場合：99%で激アツ演出、1%で演出なし
        if (random_value < 99)
        {
            pattern = PATTERN_A_GEKIATSU;
        }
        else
        {
            pattern = PATTERN_B_NORMAL_WIN;
        }
        ESP_LOGI(TAG, "Win pattern: %s (roll: %ld/100)",
                 pattern_names[pattern], random_value);
    }
    else
    {
        // 外れの場合：50%でちょっとアツい演出、50%で演出なし
        if (random_value < 50)
        {
            pattern = PATTERN_C_ATSUI;
        }
        else
        {
            pattern = PATTERN_D_NORMAL_LOSE;
        }
        ESP_LOGI(TAG, "Lose pattern: %s (roll: %ld/100)",
                 pattern_names[pattern], random_value);
    }

    return pattern;
}

/**
 * 3段階目：結果抽選
 * @param pattern 演出パターン
 * @return おみくじ結果（1-9）
 */
int drawResult(omikuji_pattern_t pattern)
{
    int result;

    switch (pattern)
    {
    case PATTERN_A_GEKIATSU:
        // パターンA：猫吉30%、スーパー吉70%
        result = drawWeightedRandom(pattern_a_weights, pattern_a_results, pattern_a_count);
        break;

    case PATTERN_B_NORMAL_WIN:
        // パターンB：猫吉100%
        result = drawWeightedRandom(pattern_b_weights, pattern_b_results, pattern_b_count);
        break;

    case PATTERN_C_ATSUI:
        // パターンC：大吉10%、中吉10%、小吉30%、吉30%、末吉20%
        result = drawWeightedRandom(pattern_c_weights, pattern_c_results, pattern_c_count);
        break;

    case PATTERN_D_NORMAL_LOSE:
        // パターンD：小吉20%、吉20%、末吉20%、凶20%、大凶20%
        result = drawWeightedRandom(pattern_d_weights, pattern_d_results, pattern_d_count);
        break;

    default:
        // 念のためのデフォルト値
        result = 8; // 凶
        break;
    }

    ESP_LOGI(TAG, "Result lottery: %s (pattern: %s)",
             omikuji_results[result - 1], pattern_names[pattern]);

    return result;
}

/**
 * アイテムインデックスを抽選する
 * @param pattern 演出パターン
 * @param is_win 当たりかどうか
 * @return アイテムインデックス（0=なし, 1-3=各種アイテム）
 */
uint8_t drawItemIndex(omikuji_pattern_t pattern, bool is_win)
{
    if (pattern == PATTERN_A_GEKIATSU || pattern == PATTERN_B_NORMAL_WIN)
    {
        // パターンA、Bの場合：1-3のアイテムをランダム選択
        uint32_t random_value = esp_random() % 3;
        return (uint8_t)(random_value + 1); // 1, 2, 3のいずれか
    }

    // その他のパターンではアイテムなし
    return 0;
}

/**
 * サウンドインデックスを抽選する
 * @param pattern 演出パターン
 * @return サウンドインデックス（0=なし, 1=あたり音, 2=外れ音, 3=すごい音）
 */
uint8_t drawSoundIndex(omikuji_pattern_t pattern)
{
    switch (pattern)
    {
    case PATTERN_A_GEKIATSU:
        return 1; // あたり音

    case PATTERN_B_NORMAL_WIN:
        // 10%で3（すごい音）、それ以外（90%）は1（あたり音）
        {
            uint32_t random_value = esp_random() % 100;
            if (random_value < 10)
            {
                return 3; // すごい音
            }
            else
            {
                return 1; // あたり音
            }
        }

    case PATTERN_C_ATSUI:
        return 1; // あたり音

    case PATTERN_D_NORMAL_LOSE:
        return 2; // 外れ音

    default:
        return 0; // なし
    }
}

/**
 * エフェクトインデックスを抽選する
 * @param pattern 演出パターン
 * @return エフェクトインデックス（0=なし, 1-3=各種エフェクト）
 */
uint8_t drawEffectIndex(omikuji_pattern_t pattern)
{
    uint32_t random_value = esp_random() % 100;

    switch (pattern)
    {
    case PATTERN_A_GEKIATSU:
        // 30%で3、30%で2、40%で1
        if (random_value < 30)
        {
            return 3;
        }
        else if (random_value < 60)
        {
            return 2;
        }
        else
        {
            return 1;
        }

    case PATTERN_B_NORMAL_WIN:
        // 90%で3、10%で2
        if (random_value < 90)
        {
            return 3;
        }
        else
        {
            return 2;
        }

    case PATTERN_C_ATSUI:
        // 20%で2、20%で1、60%で0
        if (random_value < 20)
        {
            return 2;
        }
        else if (random_value < 40)
        {
            return 1;
        }
        else
        {
            return 0;
        }

    case PATTERN_D_NORMAL_LOSE:
        // 30%で1、70%で0
        if (random_value < 30)
        {
            return 1;
        }
        else
        {
            return 0;
        }

    default:
        return 0;
    }
}

/**
 * 猫の表情を決定する
 * @param is_win 当たりかどうか
 * @return 猫顔インデックス（0=通常, 1=喜び）
 */
uint8_t drawCatFace(bool is_win)
{
    uint32_t random_value = esp_random() % 100;
    if (is_win && random_value < 40)
    {
        return 1; // 喜び（80%）
    }
    return 0; // 当たりの場合は喜び、外れは通常
}

/**
 * 新しいおみくじ抽選処理（メイン関数）
 * 3段階の抽選を順番に実行
 * @return おみくじ結果構造体
 */
omikuji_draw_result_t drawOmikuji()
{
    ESP_LOGI(TAG, "=== Starting 3-stage omikuji lottery ===");

    // 1段階目：当たり外れ抽選
    bool is_win = checkWinLose();

    // 2段階目：演出抽選
    omikuji_pattern_t pattern = drawPattern(is_win);

    // 3段階目：結果抽選
    int result = drawResult(pattern);

    // 4段階目：追加要素の抽選
    uint8_t item_index = drawItemIndex(pattern, is_win);
    uint8_t sound_index = drawSoundIndex(pattern);
    uint8_t effect_index = drawEffectIndex(pattern);
    uint8_t cat_face = drawCatFace(is_win);

    // 結果をまとめて構造体で返す
    omikuji_result = {
        .result = result,
        .pattern = pattern,
        .is_win = is_win,
        .pattern_name = pattern_names[pattern],
        .item_index = item_index,
        .sound_index = sound_index,
        .effect_index = effect_index,
        .cat_face = cat_face};

    // ログ出力（デバッグ用）
    ESP_LOGI(TAG, "=== Omikuji complete ===");
    ESP_LOGI(TAG, "Result: %s | Pattern: %s | Item: %s",
             omikuji_results[result - 1],
             omikuji_result.pattern_name,
             is_win ? "GET!" : "None");
    ESP_LOGI(TAG, "Details - Item:%d, Sound:%d, Effect:%d, Cat:%d",
             omikuji_result.item_index, omikuji_result.sound_index, omikuji_result.effect_index, omikuji_result.cat_face);

    return omikuji_result;
}

/**
 * おみくじ結果をキャンバスに描画
 * 直接LCDではなく、キャンバスに描画するにゃ
 */
void drawOmikujiResultToCanvas()
{
    if (omikuji_result.result > 0 && omikuji_result.result <= OMIKUJI_COUNT)
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
        const char *result_text = omikuji_results[omikuji_result.result - 1];
        canvas.drawString(result_text, draw_start_point + (kuji_width / 2), 35 + (kuji_height / 2));
        canvas.setTextSize(1);
        ESP_LOGD(TAG, "Omikuji result drawn to canvas: %s", result_text);

        // アイテムがある場合はアイテムも表示
        // omikuji_result.item_index = 1; // デバッグ用に強制的にアイテムありに設定
        if (omikuji_result.item_index > 0)
        {
            RetroColorPalette itemPalette;
            itemPalette.initBasicColors();                                                          // 基本パレット
            PaletteImageData itemImg(icon_006_data, icon_006_width, icon_006_height, &itemPalette); // 閉じた宝箱
            if (omikuji_delay_hiding)
            {
                itemImg = PaletteImageData(icon_005_data, icon_005_width, icon_005_height, &itemPalette); // 開いた宝箱
            }
            renderer->drawToCanvas(itemImg, draw_start_point + kuji_width, 35, true);
            if (omikuji_delay_hiding)
            {
                switch (omikuji_result.item_index)
                {
                case 1:
                    itemImg = PaletteImageData(icon_004_data, icon_004_width, icon_004_height, &itemPalette);
                    break;
                case 2:
                    itemImg = PaletteImageData(icon_003_data, icon_003_width, icon_003_height, &itemPalette);
                    break;
                default:
                    itemImg = PaletteImageData(icon_002_data, icon_002_width, icon_002_height, &itemPalette);
                    break;
                }
                renderer->drawToCanvas(itemImg, draw_start_point + kuji_width, 20 - omikuji_animation_frame, true);
            }
        }
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
        canvas.drawString("swerr", 230, 5);
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

        /* キャンバス左上にバイナリ表示debug
        canvas.setTextColor(TFT_YELLOW);
        canvas.setTextDatum(TL_DATUM);
        canvas.drawString(switch_text, 240, 5);
        //*/
        // ESP_LOGI(TAG, "Switch binary display: %s (raw: 0x%02X)", switch_text, lever_switch_state);
    }
    else
    {
        // エラー時は "ERROR" 表示
        canvas.setTextColor(TFT_RED);
        canvas.setTextDatum(TL_DATUM);
        canvas.drawString("ERROR", 240, 5);

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
        if (omikuji_result.cat_face == 0)
        {
            catHeadImage = PaletteImageData(new_cat_head2_data, new_cat_head2_width, new_cat_head2_height, &catPalette);
        }
        else
        {
            catHeadImage = PaletteImageData(new_cat_head3_data, new_cat_head3_width, new_cat_head3_height, &catPalette);
        }
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

// ゲーム状態をリセットする関数を追加
void resetGameState()
{
    // おみくじ関連をリセット
    if (cat_length <= 0)
    {
        omikuji_result = {0};
        omikuji_shown = false;
        omikuji_animation_frame = 0;
    }

    ESP_LOGI(TAG, "Game state reset completed");
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
        if (press_lever && omikuji_result.result == 0)
        {
            omikuji_result = drawOmikuji();
            if (omikuji_shown)
            {
                resetGameState();
                omikuji_shown = false;
            }
        }

        last_press_lever = press_lever;
    }

    // 移動処理
    if (press_lever)
    {
        int old_cat_length = cat_length;
        // ボタンが押されている間：右に移動
        cat_length += move_speed;
        if (cat_length > max_cat_length)
        {
            max_cat_length = cat_length;
        }

        if (cat_length > saidai_cat_length)
        {
            if (!omiku_sound_played)
            {
                switch (omikuji_result.sound_index)
                {
                case 1:
                    buzzer_play_sound(BUZZER_SOUND_OMIKUJI_HAZURE);
                    break;
                case 2:
                    buzzer_play_sound(BUZZER_SOUND_OMIKUJI_ARATI);
                    break;
                case 3:
                    buzzer_play_sound(BUZZER_SOUND_OMIKUJI_SPECIAL);
                    break;
                default:
                    buzzer_play_sound(BUZZER_SOUND_OMIKUJI_NO);
                    break;
                }
                omiku_sound_played = true;
            }
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
        cat_length -= 10;

        // 左端制限（x=0より左に行かない）
        if (cat_length <= 0)
        {
            cat_length = 0;

            omikuji_animation_frame++;
            // 猫が完全に縮んだ時の処理
            if (cat_length_was_aru)
            {
                omiku_sound_played = false;
                omikuji_animation_frame = 0;
                // おみくじが表示されている場合は遅延非表示モードに移行
                if (omikuji_shown)
                {
                    omikuji_delay_hiding = true;
                    omikuji_hide_timer = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    ESP_LOGI(TAG, "Starting omikuji display delay timer");
                }
                else
                {
                    // おみくじが表示されていない場合は即座にリセット
                    resetGameState();
                }

                // スコア確定
                score += max_cat_length;
                ESP_LOGI(TAG, "Score updated: %llu", score);
                max_cat_length = 0;

                // NVSにスコア保存
                esp_err_t ret = nvs_set_u64(nvsHandle, "score", score);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error saving score to NVS: %s", esp_err_to_name(ret));
                }
                else
                {
                    nvs_commit(nvsHandle);
                    ESP_LOGI(TAG, "Score saved to NVS");
                }
                /*
                // アイテム数保存
                char *key = "";
                uint8_t item_count = 0;
                switch (omikuji_result.item_index)
                {
                case 1:
                    key = "item1";
                    item_counts.speed_up_count++;
                    item_count = item_counts.speed_up_count;
                    break;
                case 2:
                    key = "item2";
                    item_counts.speed_down_count++;
                    item_count = item_counts.speed_down_count;
                    break;
                case 3:
                    key = "item3";
                    item_counts.bonus_count++;
                    item_count = item_counts.bonus_count;
                    break;
                }
                ret = nvs_set_u8(nvsHandle, key, item_count);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error saving item count to NVS: %s", esp_err_to_name(ret));
                }
                else
                {
                    nvs_commit(nvsHandle);
                    ESP_LOGI(TAG, "Item count saved to NVS");
                }
                */
               switch (omikuji_result.item_index)
                {
                    case MENU_SPEED_UP:
                        addGameItem(MENU_SPEED_UP, 1);
                        break;
                    case MENU_SPEED_DOWN:
                        addGameItem(MENU_SPEED_DOWN, 1);
                        break;
                    case MENU_BONUS_ITEM:
                        addGameItem(MENU_BONUS_ITEM, 1);
                        break;
                }
            }
        }
    }
    // 遅延非表示タイマーの処理
    if (omikuji_delay_hiding)
    {
        unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - omikuji_hide_timer >= OMIKUJI_DISPLAY_DELAY_MS)
        {
            // 1秒経過したのでおみくじを非表示にしてリセット
            ESP_LOGI(TAG, "Omikuji display delay timer expired, hiding results");
            resetGameState();
            omikuji_delay_hiding = false;
            omikuji_hide_timer = 0;
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

    // エフェクト表示
    if (cat_length >= saidai_cat_length)
    {

        if (omikuji_result.effect_index != 0)
        {
            // error:ログは表示されるが、アイコンが表示されない
            ESP_LOGI(TAG, "Effect index: %d", omikuji_result.effect_index);
            int effect_x = canvas.width() - miniicon_000_width;
            int effect_y = (canvas.height() / 2) - miniicon_000_height;
            // エフェクト表示
            RetroColorPalette effectPalette;
            effectPalette.initClassicRetroColors(); // 基本パレット
            if (omikuji_result.effect_index == 1)
            {
                PaletteImageData effectImage(miniicon_000_data, miniicon_000_width, miniicon_000_height, &effectPalette);
                renderer->drawToCanvas(effectImage, effect_x, effect_y, true);
            }
            else if (omikuji_result.effect_index == 2)
            {
                PaletteImageData effectImage(miniicon_001_data, miniicon_001_width, miniicon_001_height, &effectPalette);
                renderer->drawToCanvas(effectImage, effect_x, effect_y, true);
            }
            else if (omikuji_result.effect_index == 3)
            {
                PaletteImageData effectImage(miniicon_002_data, miniicon_002_width, miniicon_002_height, &effectPalette);
                renderer->drawToCanvas(effectImage, effect_x, effect_y, true);
            }
        }
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
        buzzer_play_sound(BUZZER_SOUND_STARTUP);
    }

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
        /*
                // アイテム数読み出し
                uint8_t item_count = 0;
                ret = nvs_get_u8(nvsHandle, "speed_up", &item_count);
                if (ret == ESP_OK)
                {
                    item_counts.speed_up_count = item_count;
                }
                else
                {
                    item_counts.speed_up_count = 0;
                }
                ret = nvs_get_u8(nvsHandle, "speed_down", &item_count);
                if (ret == ESP_OK)
                {
                    item_counts.speed_down_count = item_count;
                }
                else
                {
                    item_counts.speed_down_count = 0;
                }
                ret = nvs_get_u8(nvsHandle, "bonus", &item_count);
                if (ret == ESP_OK)
                {
                    item_counts.bonus_count = item_count;
                }
                else
                {
                    item_counts.bonus_count = 0;
                }

                // サウンド設定読み出し
                uint8_t get_sound_setting = 0;
                //sound_enabled;
                ret = nvs_get_u8(nvsHandle, "sound_enabled", &get_sound_setting);
                if(ret == ESP_OK) {
                    sound_enabled = (get_sound_setting != 0);
                }
                buzzer_set_enabled(sound_enabled);
         */
        // スコアボーナス読み出し
        uint8_t get_score_bonus = 0;
        ret = nvs_get_u8(nvsHandle, "score_bonus_a", &get_score_bonus);
        if (ret == ESP_OK)
        {
            score_bonus_a = get_score_bonus;
        }

        // スコアボーナスB読み出し
        ret = nvs_get_u8(nvsHandle, "score_bonus_b", &get_score_bonus);
        if (ret == ESP_OK)
        {
            score_bonus_b = get_score_bonus;
        }

        // スピード読み出し
        uint8_t get_speed = move_speed;
        ret = nvs_get_u8(nvsHandle, "move_speed", &get_speed);
        if (ret == ESP_OK)
        {
            move_speed = get_speed;
        }

        // スコア読み出し
        u_int64_t saved_score = 0;
        ret = nvs_get_u64(nvsHandle, "score", &saved_score);
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
    buzzer_set_enabled(getGameSoundEnabled());
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
                buzzer_play_sound(BUZZER_SOUND_DECISION);
                if (save_result != ESP_OK)
                {
                    ESP_LOGI(TAG, "Failed to save menu background: %s", esp_err_to_name(save_result));
                    ESP_LOGI(TAG, "Menu will use default background");
                }
                else
                {
                    ESP_LOGI(TAG, "Menu background saved successfully");
                }

                // メニュー実行
                menu_result_t menu_result = executeGameMenu();
                // メニュー結果の処理
                esp_err_t ret;
                switch (menu_result)
                {
                case MENU_RESULT_SOUND_ON:
                case MENU_RESULT_SOUND_OFF:
                    buzzer_set_enabled(getGameSoundEnabled());
                    break;
                case MENU_RESULT_USE_SPEED_UP:
                    move_speed++;
                    ret = nvs_set_u8(nvsHandle, "move_speed", move_speed);
                    if (ret == ESP_OK)
                    {
                        nvs_commit(nvsHandle);
                    }
                    buzzer_play_sound(BUZZER_SOUND_SCORE_GET);
                    break;

                case MENU_RESULT_USE_SPEED_DOWN:
                    move_speed--;
                    ret = nvs_set_u8(nvsHandle, "move_speed", move_speed);
                    if (ret == ESP_OK)
                    {
                        nvs_commit(nvsHandle);
                    }
                    buzzer_play_sound(BUZZER_SOUND_SCORE_GET);
                    break;

                case MENU_RESULT_USE_BONUS:
                    score_bonus_a++;
                    score_bonus_b++;
                    ret = nvs_set_u8(nvsHandle, "score_bonus_a", score_bonus_a);
                    if (ret == ESP_OK)
                    {
                        nvs_commit(nvsHandle);
                    }
                    buzzer_play_sound(BUZZER_SOUND_SCORE_GET);
                    break;

                default:
                    ESP_LOGI(TAG, "Menu closed without action");
                    buzzer_play_sound(BUZZER_SOUND_CANCEL);
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
            /*
            static unsigned long lastDebugTime = 0;
            if (currentTime - lastDebugTime >= 1000)
            {
                cat_sippo_toggle = !cat_sippo_toggle; // しっぽアニメーション切替
                // ESP_LOGI(TAG, "Cat position: (%d, %d), Button: %s",
                //         catX, catY, readButton() ? "PRESSED" : "RELEASED");
                lastDebugTime = currentTime;
            }
                */
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