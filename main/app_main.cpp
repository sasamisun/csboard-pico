/**
 * app_main.cpp
 *
 * 機能：
 * - dot_landscape.h を背景にして、new_cat.h の猫画像をボタンで操作
 * - M5StampPico 39番ボタン: 押すと猫がのびる
 * - MCP23008のスイッチ状態をバイナリ (01) で画面表示
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

// 【新】MCP23008ドライバーをインクルード にゃ
#include "mcp23008_driver.h"
#include <esp_timer.h>

static const char *TAG = "CatMovingGame";
static LGFX_ST7789P3_76x284 tft;

// ゲーム設定
const int BUTTON_PIN = 39;     // M5StampPicoオンボードボタン
const int CAT_WIDTH = 96;      // 猫画像の幅
const int CAT_HEIGHT = 48;     // 猫画像の高さ
const int MOVE_SPEED = 2;      // 移動速度（ピクセル/フレーム）
const int FRAME_DELAY_MS = 50; // フレーム間隔（20FPS）

// MCP23008 I2C設定 にゃ
const int I2C_MASTER_SCL_IO = 33; // SCLピン
const int I2C_MASTER_SDA_IO = 32; // SDAピン
const i2c_port_t I2C_MASTER_NUM = I2C_NUM_1;
const int I2C_MASTER_FREQ_HZ = 100000; // 100kHz
const uint8_t MCP23008_ADDR = 0x20;    // MCP23008のI2Cアドレス

// 【新】MCP23008ドライバーオブジェクト にゃ
MCP23008 mcpExpander(I2C_MASTER_NUM, MCP23008_ADDR);

// システム状態 にゃ
bool mcp_available = false; // MCP23008が使用可能かどうか

// ゲーム状態
int catX = 0;                     // 猫のX座標
int catY = 0;                     // 猫のY座標（固定：中央）
int cat_length = 0;               // 猫の体長
bool lastButtonState = false;     // 前回のボタン状態
unsigned long lastUpdateTime = 0; // 最後の更新時間
bool cat_sippo_toggle = false;    // しっぽアニメーション用フラグ
uint8_t lever_switch_state = 0;   // レバースイッチ状態

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

    // M5Unifiedが既にI2Cを初期化している可能性をチェック にゃ
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
 * おみくじ結果を画面に表示
 */
void displayOmikujiResult()
{
    if (omikuji_result > 0 && omikuji_result <= OMIKUJI_COUNT)
    {
        // 画面中央にテキスト表示
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);

        // 結果を画面中央に表示
        const char *result_text = omikuji_results[omikuji_result - 1];
        tft.drawString(result_text, tft.width() / 2, tft.height() / 2);

        ESP_LOGI(TAG, "Omikuji result displayed: %s", result_text);
    }
}

/**
 * 【更新】MCP23008スイッチ状態をバイナリ (01) で表示
 * 画面左上にスイッチの状態を01のバイナリで表示しますにゃ
 * GP4 GP3 GP2 GP1 GP0 の順で表示 (例: "01101")
 */
void displaySwitchState()
{
    // MCP23008が使用可能かチェック にゃ
    if (!mcp_available)
    {
        // MCP23008が使用できない場合は "-----" 表示
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextSize(1);
        tft.drawString("-----", 5, 5);
        return;
    }

    uint8_t switch_state = 0;

    // 新しいMCP23008ドライバーを使用してスイッチ状態を読み取り にゃ
    esp_err_t err = mcpExpander.readSwitches(&switch_state);

    if (err == ESP_OK)
    {
        // スイッチ状態をバイナリ文字列に変換
        // 注意：スイッチが押されている場合は0、離されている場合は1
        // 表示は押されている場合を1、離されている場合を0にするため反転 にゃ
        char switch_text[6];
        for (int i = 4; i >= 0; i--)
        {
            // ビットの状態を取得して反転 (0→1, 1→0)
            bool bit_state = ((switch_state >> i) & 1) == 0; // プルアップなので反転
            switch_text[4 - i] = bit_state ? '1' : '0';
        }
        switch_text[5] = '\0'; // 文字列終端

        // 画面左上にバイナリ表示 にゃ
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextSize(1);
        tft.drawString(switch_text, 5, 5);

        ESP_LOGD(TAG, "Switch binary display: %s (raw: 0x%02X)", switch_text, switch_state);
    }
    else
    {
        // エラー時は "ERROR" 表示
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextSize(1);
        tft.drawString("ERROR", 5, 5);

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
 * 背景画像を描画
 * dot_landscape.h を背景として描画
 */
void drawBackground(PaletteImageRenderer &renderer)
{
    // GameBoyカラーパレットで背景を描画
    RetroColorPalette bgPalette;
    bgPalette.initGameBoyColors();

    // 背景画像データを作成
    PaletteImageData bgImage(dot_landscape_data, dot_landscape_width, dot_landscape_height, &bgPalette);

    // 背景をキャンバス中央に配置
    int bgX = (tft.width() - dot_landscape_width) / 2;
    int bgY = (tft.height() - dot_landscape_height) / 2;

    // 座標制限（画面からはみ出さないように）
    if (bgX < 0)
        bgX = 0;
    if (bgY < 0)
        bgY = 0;
    if (bgX > tft.width() - dot_landscape_width)
    {
        bgX = tft.width() - dot_landscape_width;
    }
    if (bgY > tft.height() - dot_landscape_height)
    {
        bgY = tft.height() - dot_landscape_height;
    }

    // 背景を描画（透明色対応）
    renderer.drawToCanvas(bgImage, bgX, bgY, true);
}

/**
 * 猫画像を描画
 * new_cat_sipo1, new_cat_body1, new_cat_head1 を横並びに描画
 */
void drawCat(PaletteImageRenderer &renderer, int x, int y)
{
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
    renderer.drawToCanvas(catSipoImage, currentX, y, true);
    currentX += new_cat_sipo1_width - 1;

    // bodyを中間部分に繰り返し描画してcat_lengthに応じて隙間を埋める
    int bodyRepeatCount = cat_length / (new_cat_body1_width - 1);
    for (int i = 0; i <= bodyRepeatCount; i++)
    {
        renderer.drawToCanvas(catBodyImage, currentX, y, true);
        currentX += new_cat_body1_width - 1;
    }
    renderer.drawToCanvas(catBodyImage, (new_cat_sipo1_width - 1) + cat_length, y, true);

    int cat_length_headless = (new_cat_sipo1_width - 1) + (new_cat_body1_width - 1) + cat_length;
    // 最後にheadを描画
    renderer.drawToCanvas(catHeadImage, cat_length_headless, y, true);

}

/**
 * 猫の位置を更新
 * ボタン状態に応じて移動
 */
void updateCatPosition()
{
    bool buttonPressed = readButton();

    // ボタン状態の変化をログ出力（デバッグ用）
    if (buttonPressed != lastButtonState)
    {
        ESP_LOGI(TAG, "Button %s", buttonPressed ? "PRESSED" : "RELEASED");

        // ボタンが押された瞬間におみくじ抽選
        if (buttonPressed && omikuji_result == 0)
        {
            omikuji_result = drawOmikuji();
        }

        lastButtonState = buttonPressed;
    }

    // 移動処理
    if (buttonPressed)
    {
        // ボタンが押されている間：右に移動
        cat_length += MOVE_SPEED;

        // 最大長制限（頭部が画面外に出ない範囲）
        int max_cat_length = tft.width() - (new_cat_sipo1_width - 1) - (new_cat_body1_width - 1);
        if (cat_length > max_cat_length) {
            cat_length = max_cat_length;
            omikuji_shown = true; // 最大長になったらおみくじ表示
        }
    }
    else
    {
        // ボタンが離されている間：左に移動
        cat_length -= MOVE_SPEED;

        // 左端制限（x=0より左に行かない）
        if (cat_length < 0)
        {
            cat_length = 0;
            // 猫が完全に縮んだ時にリセット
            omikuji_result = 0;
            omikuji_shown = false;
        }
    }
    // おみくじ表示中なら表示
    if (omikuji_shown)
    {
        displayOmikujiResult();
    }
}

/**
 * ゲーム画面を描画
 */
void drawGameScreen()
{
    // レンダラーを作成
    PaletteImageRenderer renderer(&tft, tft.width(), tft.height());

    // キャンバスをクリア（黒背景）
    renderer.clearCanvas(0x0000);

    // 1. 背景を描画
    drawBackground(renderer);

    // 2. 猫を描画（背景の上に重ねる）
    drawCat(renderer, catX, catY);

    // 3. スイッチの状態を描画
    displaySwitchState();

    // 4. キャンバスをディスプレイに表示
    renderer.pushCanvasToDisplayOpaque(0, 0);

    ESP_LOGD(TAG, "Game screen drawn: catX=%d, catY=%d, length=%d", catX, catY, cat_length);
}

/**
 * ゲーム初期化
 */
void initGame()
{
    ESP_LOGI(TAG, "Initializing game components...");

    // ディスプレイ初期化
    tft.init();
    //    tft.setRotation(1);  // 横向き (284x76)
    tft.fillScreen(TFT_BLACK);

    ESP_LOGI(TAG, "Display initialized: %ldx%ld", tft.width(), tft.height());

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
    // I2C初期化 (MCP23008用) にゃ
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
        // 【追加】I2Cバススキャンを実行 にゃ
        scanI2CDevices();

        // 【更新】MCP23008初期化 にゃ
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
    catY = (tft.height() - CAT_HEIGHT) / 2; // 画面中央のY座標
    cat_length = 0;
    lastButtonState = false;
    lastUpdateTime = 0;

    ESP_LOGI(TAG, "Game initialization completed (MCP23008: %s)",
             mcp_available ? "ENABLED" : "DISABLED");
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
            // レバースイッチ状態読み取り
            //readLeverSwitch();

            // 猫の位置更新
            updateCatPosition();

            // 画面描画
            drawGameScreen();

            // レバースイッチ状態表示
            //displayLeverSwitchState();

            // 時間更新
            lastUpdateTime = currentTime;

            // デバッグ情報（1秒に1回）
            static unsigned long lastDebugTime = 0;
            if (currentTime - lastDebugTime >= 1000)
            {
                cat_sippo_toggle = !cat_sippo_toggle; // しっぽアニメーション切替
                ESP_LOGI(TAG, "Cat position: (%d, %d), Button: %s",
                         catX, catY, readButton() ? "PRESSED" : "RELEASED");
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
    ESP_LOGI(TAG, "=== Cat Moving Game with MCP23008 Switch Display Starting ===");

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
    ESP_LOGI(TAG, "Starting game loop...");
    gameLoop();
}

/*
【プログラム説明】

🎮 **ゲーム内容**
- dot_landscape.h を背景画像として表示
- new_cat.h の猫画像がその上を左右に移動
- M5StampPicoの39番ボタンで操作
- MCP23008のスイッチ状態を画面左上にバイナリ表示

🕹️ **操作方法**
- ボタン押し続け：猫が右に移動
- ボタンを離す：猫が左に移動
- 左端（x=0）で停止、右端で停止

📟 **スイッチ表示**
- 画面左上に5桁のバイナリ表示 (例: "01101")
- GP4 GP3 GP2 GP1 GP0 の順で表示
- 1=スイッチ押下、0=スイッチ開放

⚙️ **技術仕様**
- 解像度：284×76ピクセル（横向き）
- フレームレート：20FPS
- 移動速度：2ピクセル/フレーム
- GPIO39：内部プルアップ有効
- I2C: SCL=22, SDA=21, Address=0x20

🎨 **描画システム**
- 背景とキャラクターを合成描画
- 透明色対応で重ね合わせ
- GameBoyカラーパレット使用

🔌 **MCP23008機能**
- 新しいドライバーライブラリを使用
- GP0-GP4の5つのスイッチ入力に対応
- 自動プルアップ設定
- エラーハンドリング機能付き

📝 **コメント充実**
- 全ての関数に詳細な説明
- 設定値の意味を明記
- デバッグ情報も豊富

✨ **実装の特徴**
- ボタン状態変化の検出とログ出力
- 座標制限でキャラクターが画面外に出ない
- フレームレート制御で滑らかな動作
- メモリ効率を考慮した描画システム
- 新しいMCP23008ドライバーでクリーンな制御
*/