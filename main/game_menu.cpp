/**
 * @file game_menu.cpp
 * @brief ゲーム内メニューシステム実装
 * @author 猫エンジニア
 * @date 2025年9月23日
 */

#include "game_menu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// アイコンデータをインクルード
#ifndef ICON_000_H
#include "icon_000.h" // サウンドOFF
#include "icon_001.h" // サウンドON
#include "icon_002.h" // ボーナスアイテム
#include "icon_003.h" // スピードダウン
#include "icon_004.h" // スピードアップ
#include "icon_007.h" // 戻る
#endif

static const char *TAG = "GameMenu";

// グローバルメニューインスタンス
static GameMenu *g_gameMenu = nullptr;

// ====================
// GameMenuクラス実装
// ====================

GameMenu::GameMenu(LGFX_ST7789P3_76x284 *tft,
                   M5Canvas *canvas,
                   MCP23008 *mcpExpander,
                   PaletteImageRenderer *renderer,
                   nvs_handle_t nvsHandle)
    : tft_(tft), canvas_(canvas), mcpExpander_(mcpExpander), renderer_(renderer), nvsHandle_(nvsHandle), menu_active_(false), last_lever_state_(0xFF), last_input_time_(0), background_canvas_(nullptr) // 追加
      ,
      background_saved_(false) // 追加
{
    // 初期設定
    config_.sound_enabled = true;
    config_.items.speed_up_count = 3; // 初期アイテム数
    config_.items.speed_down_count = 2;
    config_.items.bonus_count = 1;
    config_.selected_item = 0;
    config_.anim_state = MENU_ANIM_IDLE;
    config_.anim_start_time = 0;
}

esp_err_t GameMenu::begin()
{
    ESP_LOGI(TAG, "Initializing GameMenu system...");
    
    // 背景保存用キャンバスを作成
    background_canvas_ = new M5Canvas(tft_);
    if (!background_canvas_) {
        ESP_LOGE(TAG, "Failed to create background canvas");
        return ESP_ERR_NO_MEM;
    }
    
    // キャンバスを画面サイズで初期化
    if (!background_canvas_->createSprite(tft_->width(), tft_->height())) {
        ESP_LOGE(TAG, "Failed to create background sprite");
        delete background_canvas_;
        background_canvas_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Background canvas created: %ldx%ld", 
             background_canvas_->width(), background_canvas_->height());
    
    // 既存のNVS読み込み処理...
    esp_err_t ret = loadConfigFromNVS();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS, using defaults");
        saveConfigToNVS();
    }

    ESP_LOGI(TAG, "GameMenu initialized: Sound=%s, Items=[%d,%d,%d]",
             config_.sound_enabled ? "ON" : "OFF",
             config_.items.speed_up_count,
             config_.items.speed_down_count,
             config_.items.bonus_count);

    return ESP_OK;
}

// デストラクタに背景キャンバスの解放を追加
GameMenu::~GameMenu()
{
    if (background_canvas_) {
        delete background_canvas_;
        background_canvas_ = nullptr;
    }
}

bool GameMenu::shouldActivateMenu(uint8_t lever_state)
{
    // 左右レバーが倒されている場合にメニュー起動
    // GP0 (bit 0) = 右レバー, GP1 (bit 1) = 左レバー
    // プルアップなので、倒されている場合は0
    bool lever_left = ((lever_state & 0b00011000) == 0);
    bool lever_right = ((lever_state & 0b00000011) == 0);

    // 前回状態と異なり、かつ左右のどちらかが倒された場合
    if (lever_state != last_lever_state_)
    {
        last_lever_state_ = lever_state;

        if (lever_left || lever_right)
        {
            ESP_LOGI(TAG, "Menu activation detected: Left=%s, Right=%s",
                     lever_left ? "ON" : "OFF", lever_right ? "ON" : "OFF");
            return true;
        }
    }

    return false;
}

menu_result_t GameMenu::runMenu()
{
    ESP_LOGI(TAG, "Starting menu execution...");

    menu_active_ = true;
    config_.anim_state = MENU_ANIM_SLIDE_IN;
    config_.anim_start_time = esp_timer_get_time() / 1000; // ms
    last_input_time_ = config_.anim_start_time;

    // メニューループ
    while (menu_active_)
    {
        uint32_t current_time = esp_timer_get_time() / 1000;

        // アニメーション更新
        updateAnimation();

        // レバー入力チェック（デバウンス付き）
        if (current_time - last_input_time_ >= INPUT_DEBOUNCE_MS)
        {
            uint8_t lever_state = 0;
            if (checkLeverInput(&lever_state))
            {
                processLeverInput(lever_state);
                last_input_time_ = current_time;
            }
        }

        if (menu_active_ == false)
        {
            break; // メニュー終了
        }

        // メニュー描画
        // canvas_->fillScreen(TFT_BLACK);
        drawMenuBackground();
        drawMenuIcons();
        canvas_->pushSprite(0, 0);

        // フレームレート制御
        vTaskDelay(pdMS_TO_TICKS(33)); // 30FPS
    }

    ESP_LOGI(TAG, "Menu execution completed");
    return MENU_RESULT_RETURN; // 暫定的な戻り値
}

bool GameMenu::checkLeverInput(uint8_t *new_state)
{
    esp_err_t ret = mcpExpander_->readSwitches(new_state);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read lever state: %s", esp_err_to_name(ret));
        return false;
    }

    // 状態が変化した場合のみtrue
    if (*new_state != last_lever_state_)
    {
        last_lever_state_ = *new_state;
        return true;
    }

    return false;
}

void GameMenu::processLeverInput(uint8_t lever_state)
{
    // アニメーション中は入力を無視
    if (config_.anim_state != MENU_ANIM_IDLE)
    {
        return;
    }

    bool lever_left = ((lever_state & 0b00000011) == 0);
    bool lever_right = ((lever_state & 0b00011000) == 0);
    bool lever_push = ((lever_state & 0b00000100) == 0);

    if (lever_left)
    {
        // 左選択：選択項目を左に移動
        config_.selected_item = (config_.selected_item == 0) ? (MENU_ITEM_COUNT - 1) : (config_.selected_item - 1);
        ESP_LOGI(TAG, "Menu selection moved left to item %d", config_.selected_item);
    }
    else if (lever_right)
    {
        // 右選択：選択項目を右に移動
        config_.selected_item = (config_.selected_item + 1) % MENU_ITEM_COUNT;
        ESP_LOGI(TAG, "Menu selection moved right to item %d", config_.selected_item);
    }
    else if (lever_push)
    {
        // 決定：選択されたアイテムを実行
        ESP_LOGI(TAG, "Menu item selected: %d", config_.selected_item);
        executeSelectedItem();
    }
}

void GameMenu::executeSelectedItem()
{
    menu_item_t selected = (menu_item_t)config_.selected_item;

    switch (selected)
    {
    case MENU_SOUND_TOGGLE:
        config_.sound_enabled = !config_.sound_enabled;
        ESP_LOGI(TAG, "Sound toggled: %s", config_.sound_enabled ? "ON" : "OFF");
        saveConfigToNVS();
        break;

    case MENU_SPEED_UP:
        if (config_.items.speed_up_count > 0)
        {
            config_.items.speed_up_count--;
            ESP_LOGI(TAG, "Speed up item used, remaining: %d", config_.items.speed_up_count);
            saveConfigToNVS();
            // TODO: スピードアップ効果を適用
        }
        else
        {
            ESP_LOGW(TAG, "No speed up items available");
            // TODO: エラー音やメッセージ表示
        }
        break;

    case MENU_SPEED_DOWN:
        if (config_.items.speed_down_count > 0)
        {
            config_.items.speed_down_count--;
            ESP_LOGI(TAG, "Speed down item used, remaining: %d", config_.items.speed_down_count);
            saveConfigToNVS();
            // TODO: スピードダウン効果を適用
        }
        else
        {
            ESP_LOGW(TAG, "No speed down items available");
        }
        break;

    case MENU_BONUS_ITEM:
        if (config_.items.bonus_count > 0)
        {
            config_.items.bonus_count--;
            ESP_LOGI(TAG, "Bonus item used, remaining: %d", config_.items.bonus_count);
            saveConfigToNVS();
            // TODO: ボーナス効果を適用
        }
        else
        {
            ESP_LOGW(TAG, "No bonus items available");
        }
        break;

    case MENU_RETURN:
        ESP_LOGI(TAG, "Returning to game");
        break;

    default:
        ESP_LOGW(TAG, "Unknown menu item: %d", selected);
        break;
    }

    // メニューを閉じる
    startSlideAnimation(MENU_ANIM_SLIDE_OUT);
}

void GameMenu::startSlideAnimation(menu_animation_t direction)
{
    config_.anim_state = direction;
    config_.anim_start_time = esp_timer_get_time() / 1000;
    ESP_LOGD(TAG, "Started slide animation: %d", direction);
}

void GameMenu::updateAnimation()
{
    if (config_.anim_state == MENU_ANIM_IDLE)
    {
        return;
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t elapsed = current_time - config_.anim_start_time;

    if (elapsed >= SLIDE_DURATION_MS)
    {
        // アニメーション完了
        if (config_.anim_state == MENU_ANIM_SLIDE_IN)
        {
            config_.anim_state = MENU_ANIM_IDLE;
            ESP_LOGD(TAG, "Slide in animation completed");
        }
        else if (config_.anim_state == MENU_ANIM_SLIDE_OUT)
        {
            menu_active_ = false; // メニュー終了
            config_.anim_state = MENU_ANIM_IDLE;
            ESP_LOGD(TAG, "Slide out animation completed, menu closed");
        }
    }
}
/*
void GameMenu::drawMenuBackground()
{
    // メニュー背景を半透明黒で描画
    //canvas_->fillRect(0, MENU_CENTER_Y - 60, canvas_->width(), 120,
    //                 canvas_->color565(0, 0, 0)); // 黒背景

    // メニュータイトル表示
    canvas_->setTextColor(TFT_CYAN);
    canvas_->setTextDatum(TC_DATUM);
    canvas_->drawString("MENU", MENU_CENTER_X, 0);
}
*/
void GameMenu::drawMenuIcons()
{
    if (!renderer_)
    {
        return;
    }

    // 選択中とその前後1個ずつを表示（計3個）
    for (int offset = -1; offset <= 1; offset++)
    {
        int item_index = (config_.selected_item + offset + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
        int x_pos = calculateIconX(offset, 0); // offset=0が中央
        int y_pos = MENU_CENTER_Y;

        // アニメーション効果（スライドイン/アウト）
        if (config_.anim_state == MENU_ANIM_SLIDE_IN)
        {
            uint32_t current_time = esp_timer_get_time() / 1000;
            uint32_t elapsed = current_time - config_.anim_start_time;
            float progress = (float)elapsed / SLIDE_DURATION_MS;
            if (progress > 1.0f)
                progress = 1.0f;

            x_pos = (int)(x_pos * progress); // 左からスライドイン
        }
        else if (config_.anim_state == MENU_ANIM_SLIDE_OUT)
        {
            uint32_t current_time = esp_timer_get_time() / 1000;
            uint32_t elapsed = current_time - config_.anim_start_time;
            float progress = (float)elapsed / SLIDE_DURATION_MS;
            if (progress > 1.0f)
                progress = 1.0f;

            x_pos = (int)(x_pos * (1.0f - progress)); // 左にスライドアウト
        }

        // アイコン描画
        drawMenuIcon((menu_item_t)item_index, x_pos, y_pos, (offset == 0));

        // アイテム数表示（該当するアイテムのみ）
        if (offset == 0)
        { // 選択中のアイテムのみ
            uint8_t count = 0;
            switch ((menu_item_t)item_index)
            {
            case MENU_SPEED_UP:
                count = config_.items.speed_up_count;
                break;
            case MENU_SPEED_DOWN:
                count = config_.items.speed_down_count;
                break;
            case MENU_BONUS_ITEM:
                count = config_.items.bonus_count;
                break;
            default:
                count = 0; // 数量表示なし
                break;
            }

            if (count > 0)
            {
                drawItemCount(x_pos, y_pos + ICON_SIZE / 2 + 3, count);
            }
        }
    }

    // 選択インジケーター描画
    if (config_.anim_state == MENU_ANIM_IDLE)
    {
        drawSelectedIndicator(MENU_CENTER_X, MENU_CENTER_Y);
    }
}

void GameMenu::drawMenuIcon(menu_item_t item, int x, int y, bool selected)
{
    if (!renderer_)
    {
        return;
    }

    RetroColorPalette iconPalette;
    iconPalette.initBasicColors();

    // アイコンデータを取得
    const uint8_t *icon_data = nullptr;

    switch (item)
    {
    case MENU_SOUND_TOGGLE:
        if (config_.sound_enabled)
        {
            icon_data = icon_001_data; // サウンドON
        }
        else
        {
            icon_data = icon_000_data; // サウンドOFF
        }
        break;
    case MENU_SPEED_UP:
        icon_data = icon_004_data;
        break;
    case MENU_SPEED_DOWN:
        icon_data = icon_003_data;
        break;
    case MENU_BONUS_ITEM:
        icon_data = icon_002_data;
        break;
    case MENU_RETURN:
        icon_data = icon_007_data;
        break;
    default:
        return; // 不明なアイテム
    }

    if (icon_data)
    {
        PaletteImageData iconImage(icon_data, ICON_SIZE, ICON_SIZE, &iconPalette);
        renderer_->drawToCanvas(iconImage, x - ICON_SIZE / 2, y - ICON_SIZE / 2, true);
    }

    // 選択中の場合は明度を上げる効果（簡易実装）
    if (selected)
    {
        canvas_->drawRect(x - ICON_SIZE / 2 - 2, y - ICON_SIZE / 2 - 2,
                          ICON_SIZE + 4, ICON_SIZE + 4, TFT_YELLOW);
    }
}

int GameMenu::calculateIconX(int offset, int center_offset)
{
    return MENU_CENTER_X + (offset * ICON_SPACING);
}

void GameMenu::drawItemCount(int x, int y, uint8_t count)
{
    canvas_->setTextColor(TFT_WHITE);
    canvas_->setTextDatum(TC_DATUM);
    canvas_->setTextSize(1);

    char count_str[4];
    snprintf(count_str, sizeof(count_str), "%d", count);
    canvas_->drawString(count_str, x, y);
}

void GameMenu::drawSelectedIndicator(int x, int y)
{
    // 選択中アイテムの周りに点滅する枠を描画
    uint32_t time = esp_timer_get_time() / 1000;
    if ((time / 500) % 2 == 0)
    { // 500ms間隔で点滅
        canvas_->drawRect(x - ICON_SIZE / 2 - 3, y - ICON_SIZE / 2 - 3,
                          ICON_SIZE + 6, ICON_SIZE + 6, TFT_CYAN);
    }
}

esp_err_t GameMenu::loadConfigFromNVS()
{
    size_t required_size = sizeof(menu_config_t);
    esp_err_t ret = nvs_get_blob(nvsHandle_, "menu_config", &config_, &required_size);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Config loaded from NVS");
        return ESP_OK;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No config found in NVS, using defaults");
        return ESP_ERR_NOT_FOUND;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading config from NVS: %s", esp_err_to_name(ret));
        return ret;
    }
}

esp_err_t GameMenu::saveConfigToNVS()
{
    esp_err_t ret = nvs_set_blob(nvsHandle_, "menu_config", &config_, sizeof(menu_config_t));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving config to NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(nvsHandle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Config saved to NVS");
    return ESP_OK;
}

// アクセサーメソッド実装
bool GameMenu::isSoundEnabled() const
{
    return config_.sound_enabled;
}

void GameMenu::setSoundEnabled(bool enabled)
{
    config_.sound_enabled = enabled;
    saveConfigToNVS();
}

item_counts_t GameMenu::getItemCounts() const
{
    return config_.items;
}

esp_err_t GameMenu::addItem(menu_item_t item, uint8_t count)
{
    switch (item)
    {
    case MENU_SPEED_UP:
        config_.items.speed_up_count =
            (config_.items.speed_up_count + count > 255) ? 255 : (config_.items.speed_up_count + count);
        break;
    case MENU_SPEED_DOWN:
        config_.items.speed_down_count =
            (config_.items.speed_down_count + count > 255) ? 255 : (config_.items.speed_down_count + count);
        break;
    case MENU_BONUS_ITEM:
        config_.items.bonus_count =
            (config_.items.bonus_count + count > 255) ? 255 : (config_.items.bonus_count + count);
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return saveConfigToNVS();
}

esp_err_t GameMenu::useItem(menu_item_t item)
{
    switch (item)
    {
    case MENU_SPEED_UP:
        if (config_.items.speed_up_count > 0)
        {
            config_.items.speed_up_count--;
        }
        else
        {
            return ESP_ERR_NOT_FOUND;
        }
        break;
    case MENU_SPEED_DOWN:
        if (config_.items.speed_down_count > 0)
        {
            config_.items.speed_down_count--;
        }
        else
        {
            return ESP_ERR_NOT_FOUND;
        }
        break;
    case MENU_BONUS_ITEM:
        if (config_.items.bonus_count > 0)
        {
            config_.items.bonus_count--;
        }
        else
        {
            return ESP_ERR_NOT_FOUND;
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return saveConfigToNVS();
}

void GameMenu::printDebugInfo() const
{
    ESP_LOGI(TAG, "=== GameMenu Debug Info ===");
    ESP_LOGI(TAG, "Sound enabled: %s", config_.sound_enabled ? "YES" : "NO");
    ESP_LOGI(TAG, "Items: SpeedUp=%d, SpeedDown=%d, Bonus=%d",
             config_.items.speed_up_count,
             config_.items.speed_down_count,
             config_.items.bonus_count);
    ESP_LOGI(TAG, "Selected item: %d", config_.selected_item);
    ESP_LOGI(TAG, "Menu active: %s", menu_active_ ? "YES" : "NO");
    ESP_LOGI(TAG, "===========================");
}

// ====================
// グローバル関数実装
// ====================

esp_err_t initGameMenu(LGFX_ST7789P3_76x284 *tft,
                       M5Canvas *canvas,
                       MCP23008 *mcpExpander,
                       PaletteImageRenderer *renderer,
                       nvs_handle_t nvsHandle)
{
    if (g_gameMenu != nullptr)
    {
        delete g_gameMenu;
    }

    g_gameMenu = new GameMenu(tft, canvas, mcpExpander, renderer, nvsHandle);
    if (!g_gameMenu)
    {
        ESP_LOGE(TAG, "Failed to create GameMenu instance");
        return ESP_ERR_NO_MEM;
    }

    return g_gameMenu->begin();
}

bool checkMenuActivation(uint8_t lever_state)
{
    if (g_gameMenu)
    {
        return g_gameMenu->shouldActivateMenu(lever_state);
    }
    return false;
}

menu_result_t executeGameMenu()
{
    if (g_gameMenu)
    {
        return g_gameMenu->runMenu();
    }
    return MENU_RESULT_NONE;
}

bool getGameSoundEnabled()
{
    if (g_gameMenu)
    {
        return g_gameMenu->isSoundEnabled();
    }
    return true; // デフォルト値
}

esp_err_t addGameItem(menu_item_t item, uint8_t count)
{
    if (g_gameMenu)
    {
        return g_gameMenu->addItem(item, count);
    }
    return ESP_ERR_INVALID_STATE;
}

// 現在の画面を背景として保存
esp_err_t GameMenu::saveCurrentScreenAsBackground()
{
    if (!background_canvas_ || !canvas_) {
        ESP_LOGE(TAG, "Background canvas or main canvas not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Saving current screen as menu background...");
    
    // メインキャンバスの内容を背景キャンバスにコピー
    // M5Canvasのpushスプライト機能を使用してコピー
    canvas_->pushSprite(background_canvas_, 0, 0);
    
    background_saved_ = true;
    ESP_LOGI(TAG, "Background saved successfully");
    
    return ESP_OK;
}

// 保存された背景をクリア
void GameMenu::clearSavedBackground()
{
    if (background_canvas_) {
        background_canvas_->fillScreen(TFT_BLACK);
    }
    background_saved_ = false;
    ESP_LOGD(TAG, "Background cleared");
}

// メニュー背景描画を修正
void GameMenu::drawMenuBackground()
{
    // 保存された背景がある場合はそれを使用
    if (background_saved_ && background_canvas_) {
        // 保存された背景をキャンバスに描画
        background_canvas_->pushSprite(canvas_, 0, 0);
        
        // 半透明のオーバーレイを追加してメニュー感を演出
        //canvas_->fillRect(0, 0, canvas_->width(), canvas_->height(), 
        //                 canvas_->color565(0, 0, 0) | 0x4000); // 半透明黒
    } else {
        // 背景がない場合は従来の黒背景
        canvas_->fillScreen(TFT_BLACK);
    }
    
    // メニュータイトル表示
    canvas_->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas_->setTextDatum(TC_DATUM);
    canvas_->drawString("MENU", MENU_CENTER_X, 4);
    
    // 操作説明を追加
    //canvas_->setTextSize(1);
    //canvas_->setTextColor(TFT_WHITE);
    //canvas_->drawString("Left/Right: Select  Push: OK", MENU_CENTER_X, canvas_->height() - 15);
}

// グローバル関数に背景保存機能を追加
esp_err_t saveMenuBackground()
{
    if (g_gameMenu) {
        return g_gameMenu->saveCurrentScreenAsBackground();
    }
    return ESP_ERR_INVALID_STATE;
}

bool hasMenuBackgroundSaved()
{
    if (g_gameMenu) {
        return g_gameMenu->hasBackgroundSaved();
    }
    return false;
}
