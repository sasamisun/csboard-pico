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

// ST7789P3ディスプレイとレトロゲームシステム
#include "LGFX_ST7789P3_76x284.hpp"
#include "RetroGamePaletteImage.hpp"

// 【重要】画像データをインクルード
#include "new_cat_body1.h" // 猫画像データ
#include "new_cat_head1.h" // 猫画像データ
#include "new_cat_head2.h" // 猫画像データ
#include "new_cat_sipo1.h" // 猫画像データ
#include "new_cat_sipo2.h" // 猫画像データ
#include "dot_landscape.h" // 背景画像データ

// 【新】MCP23008ドライバーをインクルード
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

// スコアなど管理
uint64_t score = 0;   // スコア
nvs_handle nvsHandle; // NVSハンドル

// おみくじの結果配列
const char *omikuji_results[] = {
    "daikichi",
    "chuukichi",
    "shoukichi",
    "kichi",
    "suekichi",
    "kyou",
    "daikyou"};
const int OMIKUJI_COUNT = sizeof(omikuji_results) / sizeof(omikuji_results[0]);
int omikuji_result = 0;     // おみくじ結果 (0=未抽選, 1=大吉, 2=中吉, ...)
bool omikuji_shown = false; // おみくじ表示フラグ

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
 * 1-7の値を返す (1=大吉, 2=中吉, ... 7=大凶)
 */
int drawOmikuji()
{
    uint32_t randomValue = esp_random();
    int result = (randomValue % OMIKUJI_COUNT) + 1;
    ESP_LOGI(TAG, "Omikuji drawn: %d (%s)", result, omikuji_results[result - 1]);
    return result;
}

/**
 * 【統一】おみくじ結果をキャンバスに描画
 * 直接LCDではなく、キャンバスに描画するにゃ
 */
void drawOmikujiResultToCanvas()
{
    if (omikuji_result > 0 && omikuji_result <= OMIKUJI_COUNT)
    {
        // キャンバス中央にテキスト表示
        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.setTextDatum(MC_DATUM);
        canvas.setTextSize(2);

        // 結果をキャンバス中央に描画
        const char *result_text = omikuji_results[omikuji_result - 1];
        canvas.drawString(result_text, canvas.width() / 2, canvas.height() / 2);

        ESP_LOGD(TAG, "Omikuji result drawn to canvas: %s", result_text);
    }
}

/**
 * 【統一】MCP23008スイッチ状態をキャンバスにバイナリ表示
 * 直接LCDではなく、キャンバスにスイッチの状態を描画するにゃ
 * GP4 GP3 GP2 GP1 GP0 の順で表示 (例: "01101")
 */
void drawSwitchStateToCanvas()
{
    // MCP23008が使用可能かチェック
    if (!mcp_available)
    {
        // MCP23008が使用できない場合は "-----" 表示
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextSize(1);
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
        canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextSize(1);
        canvas.drawString(switch_text, 240, 5);

        // ESP_LOGI(TAG, "Switch binary display: %s (raw: 0x%02X)", switch_text, lever_switch_state);
    }
    else
    {
        // エラー時は "ERROR" 表示
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextSize(1);
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
        // 最大長制限（頭部が画面外に出ない範囲）
        int max_cat_length = canvas.width() - (new_cat_sipo1_width - 1) - (new_cat_body1_width - 1);
        if (cat_length > max_cat_length)
        {
            cat_length = max_cat_length;
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
    snprintf(score_text, sizeof(score_text), "[%llumm]", score);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.drawString(score_text, 5, 5);

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
    catY = tft.height() - CAT_HEIGHT - 5; // 縦方向中央

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
    catY = tft.height() - CAT_HEIGHT - 10; // 画面中央のY座標
    cat_length = 0;
    lastButtonState = false;
    lastUpdateTime = 0;

    ESP_LOGI(TAG, "Game initialization completed (MCP23008: %s)",
             mcp_available ? "ENABLED" : "DISABLED");

    // NVS初期化
    nvs_flash_init();
    esp_err_t ret = nvs_open("save_data", NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(ret));
    }else{
        // スコア読み出し
        int64_t saved_score = 0;
        ret = nvs_get_i64(nvsHandle, "score", &saved_score);
        if (ret == ESP_OK)
        {
            score = saved_score;
            ESP_LOGI(TAG, "Loaded score from NVS: %llu", score);
        }
        else if (ret == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "No saved score found in NVS, starting fresh");
            score = 0;
        }
        else
        {
            ESP_LOGE(TAG, "Error reading score from NVS: %s", esp_err_to_name(ret));
            score = 0;
        }
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