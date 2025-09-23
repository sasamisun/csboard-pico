/**
 * cat_naming.cpp
 *
 * 猫の名前付けインターフェース実装
 * カタカナ文字選択とレバー操作による名前入力システム
 *
 * 作成者: 猫エンジニア
 * 日付: 2025年9月21日
 */

#include "cat_naming.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <cmath>
#include "face.h"

static const char *TAG = "CatNaming";

// カタカナ文字セット定義（UTF-8エンコード）
const char *CatNaming::CHARACTER_SET =
    "ア,ァ,イ,ィ,ウ,ゥ,エ,ェ,オ,ォ,"
    "カ,キ,ク,ケ,コ,ガ,ギ,グ,ゲ,ゴ,"
    "サ,シ,ス,セ,ソ,ザ,ジ,ズ,ゼ,ゾ,"
    "タ,チ,ツ,ッ,テ,ト,ダ,ヂ,ヅ,デ,ド,"
    "ナ,ニ,ヌ,ネ,ノ,"
    "ハ,ヒ,フ,ヘ,ホ,バ,ビ,ブ,ベ,ボ,パ,ピ,プ,ペ,ポ,"
    "マ,ミ,ム,メ,モ,"
    "ヤ,ャ,ユ,ュ,ヨ,ョ,"
    "ラ,リ,ル,レ,ロ,"
    "ワ,ヲ,ン,-,~,!,?,"
    "del,end";

const int CatNaming::TOTAL_CHARACTERS = 86; // 特殊文字含む総数

// 画像用パレット
RetroColorPalette catPalette;
static PaletteImageRenderer *renderer = nullptr; // パレット画像レンダラー

/**
 * コンストラクタ
 */
CatNaming::CatNaming(LGFX_ST7789P3_76x284 *display,
                     M5Canvas *draw_canvas,
                     MCP23008 *mcp,
                     nvs_handle_t nvs_handle,
                     PaletteImageRenderer *pirenderer)
    : tft(display),
      canvas(draw_canvas),
      mcpExpander(mcp),
      nvsHandle(nvs_handle),
      last_lever_state(0),
      last_input_time(0),
      decide_press_start(0),
      decide_button_pressed(false),
      cursor_visible(true),
      last_cursor_blink(0),
      current_name_length(0),
      current_char_index(0),
      naming_state(CAT_NAMING_ACTIVE)
{
    renderer = pirenderer;
    // 名前配列を初期化
    memset(cat_name, 0, sizeof(cat_name));
}

/**
 * 名前付けシステム開始（ブロッキング）
 */
esp_err_t CatNaming::startNaming(uint8_t *result_name)
{
    ESP_LOGI(TAG, "Give me a name!");

    initializeNaming();

    // メインループ（名前が決定されるまで継続）
    while (naming_state == CAT_NAMING_ACTIVE)
    {
        // レバー入力処理
        handleLeverInput();

        // 決定ボタン処理
        handleDecideButton();

        // UI描画
        drawNamingInterface();

        // CPU負荷軽減
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // 結果の名前をコピー
    if (result_name)
    {
        memcpy(result_name, cat_name, CAT_NAME_MAX_LENGTH + 1);
    }

    // NVSに保存
    esp_err_t save_result = saveNameToNVS(cat_name);
    return ESP_OK;
}

/**
 * 名前付け初期化
 */
void CatNaming::initializeNaming()
{
    // 状態リセット
    memset(cat_name, 0, sizeof(cat_name));
    current_name_length = 0;
    current_char_index = 0;
    naming_state = CAT_NAMING_ACTIVE;
    last_lever_state = 0;
    decide_button_pressed = false;
    cursor_visible = true;

    // 時間初期化
    unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    last_input_time = current_time;
    last_cursor_blink = current_time;

    // レバーナガオシ状態初期化
    lever_press_start_time = 0;
    lever_currently_pressed = false;
    current_lever_direction = 0;
    last_continuous_move_time = 0;

    // パレット初期化
    catPalette.initGameBoyColors();
    renderer = new PaletteImageRenderer(tft, canvas);

    ESP_LOGI(TAG, "名前付けシステム初期化完了");
}

/**
 * レバー入力処理
 */
void CatNaming::handleLeverInput()
{
    uint8_t current_lever_state = 0;

    // MCP23008からレバー状態読み取り
    esp_err_t err = mcpExpander->readSwitches(&current_lever_state);
    if (err != ESP_OK)
    {
        return;
    }

    // 01反転処理（プルアップ対応）
    current_lever_state = ~current_lever_state;

    unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 現在のレバー状態を判定（左右のみ）
    bool left_pressed = (current_lever_state & (LEVER_LEFT_LIGHT | LEVER_LEFT_STRONG)) != 0;
    bool right_pressed = (current_lever_state & (LEVER_RIGHT_LIGHT | LEVER_RIGHT_STRONG)) != 0;

    // 新しく押下された場合
    if ((left_pressed || right_pressed) && !lever_currently_pressed)
    {
        lever_currently_pressed = true;
        lever_press_start_time = current_time;
        last_continuous_move_time = current_time;

        // 即座に1文字移動
        if (left_pressed)
        {
            current_lever_direction = 1; // 左方向
            updateCharacterSelection(1, 1);
        }
        else
        {
            current_lever_direction = -1; // 右方向
            updateCharacterSelection(-1, 1);
        }

        ESP_LOGD(TAG, "レバー初回押下: 方向 %d", current_lever_direction);
    }
    // 継続して押下中の場合
    else if ((left_pressed || right_pressed) && lever_currently_pressed)
    {
        unsigned long hold_duration = current_time - lever_press_start_time;

        // 長押し判定時間を超えた場合、連続移動
        if (hold_duration >= LONG_HOLD_TIME_MS)
        {
            unsigned long time_since_last_move = current_time - last_continuous_move_time;

            if (time_since_last_move >= CONTINUOUS_MOVE_INTERVAL_MS)
            {
                // 5文字ずつ連続移動
                updateCharacterSelection(current_lever_direction, 5);
                last_continuous_move_time = current_time;

                ESP_LOGD(TAG, "連続移動: 方向 %d, 5文字", current_lever_direction);
            }
        }
    }
    // レバーが離された場合
    else if (!left_pressed && !right_pressed && lever_currently_pressed)
    {
        lever_currently_pressed = false;
        current_lever_direction = 0;

        ESP_LOGD(TAG, "レバー離された");
    }
}

/**
 * 決定ボタン処理
 */
void CatNaming::handleDecideButton()
{
    uint8_t current_lever_state = 0;

    // MCP23008からレバー状態読み取り
    esp_err_t err = mcpExpander->readSwitches(&current_lever_state);
    if (err != ESP_OK)
    {
        return;
    }

    // 01反転処理
    current_lever_state = ~current_lever_state;

    unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool decide_currently_pressed = (current_lever_state & LEVER_DECIDE) != 0;

    // 決定ボタン押下開始検出
    if (decide_currently_pressed && !decide_button_pressed)
    {
        decide_button_pressed = true;
        decide_press_start = current_time;
        ESP_LOGD(TAG, "決定ボタン押下開始");
    }
    // 決定ボタン離された時の処理
    else if (!decide_currently_pressed && decide_button_pressed)
    {
        decide_button_pressed = false;
        unsigned long press_duration = current_time - decide_press_start;

        if (press_duration >= LONG_PRESS_TIME_MS)
        {
            // 長押し：即座に名前確定
            ESP_LOGI(TAG, "長押し検出：名前を即座に確定");
            finishNaming();
        }
        else
        {
            // 短押し：文字追加または特殊操作
            ESP_LOGD(TAG, "短押し検出：文字操作実行");

            if (current_char_index == SPECIAL_CHAR_DELETE)
            {
                deleteLastCharacter();
            }
            else if (current_char_index == SPECIAL_CHAR_FINISH)
            {
                finishNaming();
            }
            else
            {
                addCharacterToName();
            }
        }
    }
    // 長押し中の処理（リアルタイム確定）
    else if (decide_currently_pressed)
    {
        unsigned long press_duration = current_time - decide_press_start;

        if (press_duration >= LONG_PRESS_TIME_MS)
        {
            // 長押し確定（一度だけ実行）
            static bool long_press_executed = false;
            if (!long_press_executed)
            {
                ESP_LOGI(TAG, "長押し時間到達：名前確定");
                finishNaming();
                long_press_executed = true;
            }
        }
    }
    else
    {
        // ボタンが離されたら長押しフラグリセット
        static bool long_press_executed = false;
        long_press_executed = false;
    }
}

/**
 * 文字選択更新（円環状）
 */
void CatNaming::updateCharacterSelection(int direction, int step)
{
    int move_amount = direction * step;
    current_char_index += move_amount;

    // 円環状のラップアラウンド
    while (current_char_index < 0)
    {
        current_char_index += TOTAL_CHARACTERS;
    }
    while (current_char_index >= TOTAL_CHARACTERS)
    {
        current_char_index -= TOTAL_CHARACTERS;
    }

    ESP_LOGD(TAG, "文字選択更新: %s (インデックス: %d)",
             getCharacterAt(current_char_index), current_char_index);
}

/**
 * 名前に文字追加
 */
void CatNaming::addCharacterToName()
{

    // 現在の文字を数字に変換して追加
    const char *current_char = getCharacterAt(current_char_index);
    uint8_t char_number = convertCharToNumber(current_char);

    if (char_number > 0)
    {
        cat_name[current_name_length] = char_number;
        current_name_length++;
        cat_name[current_name_length] = 0; // 終端文字

        ESP_LOGI(TAG, "文字追加: %s (番号: %d, 長さ: %d)",
                 current_char, char_number, current_name_length);
    }

    if (current_name_length >= CAT_NAME_MAX_LENGTH)
    {
        ESP_LOGW(TAG, "名前が最大長に到達（%d文字）", CAT_NAME_MAX_LENGTH);
        finishNaming();
        return;
    }
}

/**
 * 最後の文字削除
 */
void CatNaming::deleteLastCharacter()
{
    if (current_name_length > 0)
    {
        current_name_length--;
        cat_name[current_name_length] = 0;
        ESP_LOGI(TAG, "文字削除: 残り長さ %d", current_name_length);
    }
    else
    {
        ESP_LOGD(TAG, "削除対象の文字がありません");
    }
}

/**
 * 名前付け完了
 */
void CatNaming::finishNaming()
{
    if (current_name_length == 0)
    {
        ESP_LOGW(TAG, "名前が空です：デフォルト名を設定");
        // デフォルト名「ネコ」を設定（ネ=45, コ=15）
        cat_name[0] = 45; // ネ
        cat_name[1] = 15; // コ
        cat_name[2] = 0;  // 終端
        current_name_length = 2;
    }

    naming_state = CAT_NAMING_COMPLETED;

    // デバッグ用：最終的な名前をログ出力
    char display_name[64];
    convertNameToDisplayString(cat_name, display_name, sizeof(display_name));
    ESP_LOGI(TAG, "名前確定: %s", display_name);
}

/**
 * 名前付けUI描画
 */
void CatNaming::drawNamingInterface()
{
    // キャンバスクリア
    canvas->fillScreen(TFT_BLACK);

    // タイトル表示
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextDatum(TL_DATUM);
    canvas->setTextSize(1);
    canvas->drawString("Give me a name!", 5, 5);

    // 現在選択中の文字表示
    drawCurrentCharacter();

    // 現在の名前表示
    drawCurrentName();

    // カーソル描画
    drawCursor();

    // 操作説明表示
    drawInstructions();

    // キャンバスをLCDに転送
    canvas->pushSprite(0, 0);
}

/**
 * 現在選択中の文字表示
 */
void CatNaming::drawCurrentCharacter()
{
    const char *current_char = getCharacterAt(current_char_index);

    canvas->setTextColor(TFT_YELLOW);
    canvas->setTextDatum(TL_DATUM);
    canvas->setTextSize(1);

    int char_y = canvas->height() / 2 - 20;
    canvas->drawString(current_char, 120 + (current_name_length * 13), 5);

    // インデックス表示（デバッグ用）
    /*
    canvas->setTextSize(1);
    canvas->setTextColor(TFT_BLUE);
    char index_str[32];
    snprintf(index_str, sizeof(index_str), "(%d/%d)", current_char_index + 1, TOTAL_CHARACTERS);
    canvas->drawString(index_str, canvas->width() / 2, char_y + 25);
    */
}

/**
 * 現在の名前表示
 */
void CatNaming::drawCurrentName()
{
    if (current_name_length == 0)
    {
        return;
    }

    // 名前を表示用文字列に変換
    char display_name[64];
    convertNameToDisplayString(cat_name, display_name, sizeof(display_name));

    // 名前表示
    canvas->setTextColor(TFT_CYAN);
    canvas->setTextDatum(TL_DATUM);
    canvas->setTextSize(1);

    char name_label[80];
    snprintf(name_label, sizeof(name_label), "%s", display_name);
    canvas->drawString(name_label, 119, 5);

    // 長さ表示
    canvas->setTextColor(TFT_WHITE);
    char length_info[32];
    snprintf(length_info, sizeof(length_info), "(%d/%d)", current_name_length, CAT_NAME_MAX_LENGTH);
    canvas->drawString(length_info, 239, 5);
}

/**
 * カーソル点滅描画
 */
void CatNaming::drawCursor()
{
    unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // カーソル点滅タイミング
    if (current_time - last_cursor_blink > CURSOR_BLINK_INTERVAL_MS)
    {
        cursor_visible = !cursor_visible;
        last_cursor_blink = current_time;
    }

    if (cursor_visible)
    {
        canvas->setTextColor(TFT_RED);
        canvas->setTextDatum(TL_DATUM);
        canvas->setTextSize(1);
        canvas->drawString("^", 122 + (current_name_length * 13), 21);
    }
}

/**
 * 操作説明表示
 */
void CatNaming::drawInstructions()
{
    canvas->setTextColor(TFT_GREEN);
    canvas->setTextDatum(BL_DATUM);
    canvas->setTextSize(1);

    int y_base = canvas->height() - 60;
    int line_height = 12;

    canvas->drawString("サユウ:センタク / ナガオシ:end", 5, 42);
    /*
        canvas->drawString("強: 5文字ジャンプ", 5, y_base + line_height);
        canvas->drawString("決定: 文字確定", 5, y_base + line_height * 2);
        canvas->drawString("長押し: 名前確定", 5, y_base + line_height * 3);
    */
    // 決定ボタン長押し進捗表示
    int bar_width = 0;
    float progress = 0.0f;
    if (decide_button_pressed)
    {
        unsigned long current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        unsigned long press_duration = current_time - decide_press_start;
        progress = (float)press_duration / LONG_PRESS_TIME_MS;

        if (progress > 1.0f)
            progress = 1.0f;

        // プログレスバー描画
        bar_width = canvas->width() - 10;
        int bar_height = 4;
        int bar_x = 5;
        int bar_y = canvas->height() - 10;

        // canvas->drawRect(bar_x, bar_y, bar_width, bar_height, TFT_WHITE);
        canvas->fillRect(bar_x, bar_y, (int)(bar_width * progress), bar_height, TFT_WHITE);
    }

    PaletteImageData catHeadImage(face_data, face_width, face_height, &catPalette);
    renderer->drawToCanvas(catHeadImage, 5 + (int)(bar_width * progress), canvas->height() - 15);
}

/**
 * 特殊文字判定
 */
bool CatNaming::isSpecialCharacter(int index)
{
    return (index == SPECIAL_CHAR_DELETE || index == SPECIAL_CHAR_FINISH);
}

/**
 * 指定位置の文字取得
 */
const char *CatNaming::getCharacterAt(int index)
{
    static char buffer[16];

    if (index < 0 || index >= TOTAL_CHARACTERS)
    {
        return "?";
    }

    // 特殊文字処理
    if (index == SPECIAL_CHAR_DELETE)
    {
        return "del";
    }
    else if (index == SPECIAL_CHAR_FINISH)
    {
        return "end";
    }

    // カンマ区切りの文字セットから指定文字を抽出
    const char *pos = CHARACTER_SET;
    int current_index = 0;

    while (*pos && current_index < index)
    {
        if (*pos == ',')
        {
            current_index++;
        }
        pos++;
    }

    if (!*pos)
    {
        return "?";
    }

    // カンマまたは終端までコピー
    int i = 0;
    while (*pos && *pos != ',' && i < 15)
    {
        buffer[i++] = *pos++;
    }
    buffer[i] = '\0';

    return buffer;
}

/**
 * カタカナを数字に変換
 */
uint8_t CatNaming::convertCharToNumber(const char *katakana)
{
    // 文字セット内での位置を返す（1から開始）
    const char *pos = CHARACTER_SET;
    int index = 1;

    while (*pos)
    {
        const char *char_start = pos;

        // 次のカンマまたは終端まで進む
        while (*pos && *pos != ',')
        {
            pos++;
        }

        // 現在の文字と比較
        int char_len = pos - char_start;
        if (strncmp(katakana, char_start, char_len) == 0 && katakana[char_len] == '\0')
        {
            return (uint8_t)index;
        }

        index++;
        if (*pos == ',')
        {
            pos++; // カンマをスキップ
        }
    }

    return 0; // 見つからない場合
}

/**
 * 名前をNVSに保存
 */
esp_err_t CatNaming::saveNameToNVS(const uint8_t *name)
{
    if (!name)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_blob(nvsHandle, "cat_name", name, CAT_NAME_MAX_LENGTH + 1);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvsHandle);
    }

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Successful save to NVS");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to save to NVS: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * NVSから名前を読み込み
 */
esp_err_t CatNaming::loadNameFromNVS(uint8_t *name)
{
    if (!name)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = CAT_NAME_MAX_LENGTH + 1;
    esp_err_t err = nvs_get_blob(nvsHandle, "cat_name", name, &required_size);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "猫の名前をNVSから読み込みました");
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "NVSに猫の名前が保存されていません");
    }
    else
    {
        ESP_LOGE(TAG, "猫の名前のNVS読み込みに失敗: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * 名前を表示用文字列に変換
 */
void CatNaming::convertNameToDisplayString(const uint8_t *name_array, char *output_str, size_t buffer_size)
{
    if (!name_array || !output_str || buffer_size == 0)
    {
        if (output_str && buffer_size > 0)
        {
            output_str[0] = '\0';
        }
        return;
    }

    output_str[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < CAT_NAME_MAX_LENGTH && name_array[i] != 0; i++)
    {
        uint8_t char_num = name_array[i];

        if (char_num == 0)
            break;

        // 数字をカタカナに変換
        const char *katakana = getCharacterAt(char_num - 1);

        size_t katakana_len = strlen(katakana);
        if (pos + katakana_len + 1 < buffer_size)
        {
            strcat(output_str, katakana);
            pos += katakana_len;
        }
        else
        {
            break; // バッファオーバーフロー防止
        }
    }
}

/**
 * デバッグ情報出力
 */
void CatNaming::printDebugInfo()
{
    ESP_LOGI(TAG, "=== 猫名前付けデバッグ情報 ===");
    ESP_LOGI(TAG, "現在の状態: %d", naming_state);
    ESP_LOGI(TAG, "現在の文字インデックス: %d (%s)", current_char_index, getCharacterAt(current_char_index));
    ESP_LOGI(TAG, "現在の名前長: %d", current_name_length);

    char display_name[64];
    convertNameToDisplayString(cat_name, display_name, sizeof(display_name));
    ESP_LOGI(TAG, "現在の名前: %s", display_name);

    ESP_LOGI(TAG, "決定ボタン状態: %s", decide_button_pressed ? "押下中" : "離されている");
    ESP_LOGI(TAG, "========================");
}

// グローバル関数実装

/**
 * 猫名前付けシステムの初期化と実行
 */
esp_err_t runCatNamingSystem(LGFX_ST7789P3_76x284 *display,
                             M5Canvas *canvas,
                             MCP23008 *mcp,
                             nvs_handle_t nvs_handle,
                             uint8_t *result_name)
{
    if (!display || !canvas || !mcp || !result_name)
    {
        ESP_LOGE(TAG, "無効な引数が渡されました");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "猫名前付けシステム開始");

    // CatNamingオブジェクト作成
    CatNaming catNaming(display, canvas, mcp, nvs_handle, renderer);

    // 名前付け実行
    esp_err_t result = catNaming.startNaming(result_name);

    if (result == ESP_OK)
    {
        char display_name[64];
        catNaming.convertNameToDisplayString(result_name, display_name, sizeof(display_name));
        ESP_LOGI(TAG, "猫名前付け完了: %s", display_name);
    }
    else
    {
        ESP_LOGE(TAG, "猫名前付けに失敗: %s", esp_err_to_name(result));
    }

    return result;
}