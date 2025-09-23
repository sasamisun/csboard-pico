/**
 * @file game_menu.h
 * @brief ゲーム内メニューシステム
 * @author 猫エンジニア
 * @date 2025年9月23日
 *
 * レバー操作でアクセスできるゲーム内メニューシステム
 * アイテム使用、設定変更などの機能を提供
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "LGFX_ST7789P3_76x284.hpp"
#include "M5Unified.h"
#include "mcp23008_driver.h"
#include "RetroGamePaletteImage.hpp"
#include "nvs_flash.h"

// メニュー項目定義
typedef enum
{
    MENU_SOUND_TOGGLE = 0, // サウンドON/OFF切り替え
    MENU_SPEED_UP,         // スピードアップアイテム使用
    MENU_SPEED_DOWN,       // スピードダウンアイテム使用
    MENU_BONUS_ITEM,       // ボーナスアイテム使用
    MENU_RETURN,           // 何もせず戻る
    MENU_ITEM_COUNT        // メニュー項目数（5個）
} menu_item_t;

// メニュー選択結果
typedef enum
{
    MENU_RESULT_NONE = 0,       // 何もしない（キャンセル）
    MENU_RESULT_SOUND_ON,       // サウンドON設定
    MENU_RESULT_SOUND_OFF,      // サウンドOFF設定
    MENU_RESULT_USE_SPEED_UP,   // スピードアップアイテム使用
    MENU_RESULT_USE_SPEED_DOWN, // スピードダウンアイテム使用
    MENU_RESULT_USE_BONUS,      // ボーナスアイテム使用
    MENU_RESULT_RETURN          // メニューから戻る
} menu_result_t;

// メニューアニメーション状態
typedef enum
{
    MENU_ANIM_SLIDE_IN = 0, // スライドイン中
    MENU_ANIM_IDLE,         // 待機中（選択可能）
    MENU_ANIM_SLIDE_OUT     // スライドアウト中
} menu_animation_t;

// アイテム所持数構造体
typedef struct
{
    uint8_t speed_up_count;   // スピードアップアイテム所持数
    uint8_t speed_down_count; // スピードダウンアイテム所持数
    uint8_t bonus_count;      // ボーナスアイテム所持数
} item_counts_t;

// メニューシステム設定
typedef struct
{
    bool sound_enabled;          // サウンド有効フラグ
    item_counts_t items;         // アイテム所持数
    uint8_t selected_item;       // 現在選択中のアイテム (0-4)
    menu_animation_t anim_state; // アニメーション状態
    uint32_t anim_start_time;    // アニメーション開始時刻
} menu_config_t;

/**
 * @brief GameMenuクラス - ゲーム内メニューシステム
 *
 * レバー操作によるメニュー表示・操作を管理する
 * アイテム管理と設定変更機能を提供
 */
class GameMenu
{
private:
    // ハードウェア参照
    LGFX_ST7789P3_76x284 *tft_;
    M5Canvas *canvas_;
    MCP23008 *mcpExpander_;
    PaletteImageRenderer *renderer_;
    nvs_handle_t nvsHandle_;

    // メニュー状態
    menu_config_t config_;
    bool menu_active_;
    uint8_t last_lever_state_;
    uint32_t last_input_time_;

    // アニメーション設定
    static const uint32_t SLIDE_DURATION_MS = 300; // スライドアニメーション時間
    static const uint32_t INPUT_DEBOUNCE_MS = 200; // 入力デバウンス時間

    // 表示設定
    static const int MENU_CENTER_X = 213; // メニュー中央X座標
    static const int MENU_CENTER_Y = 38;  // メニュー中央Y座標
    static const int ICON_SIZE = 32;      // アイコンサイズ
    static const int ICON_SPACING = 40;   // アイコン間隔

    // 背景保存機能
    M5Canvas *background_canvas_;
    bool background_saved_;

    // プライベートメソッド
    esp_err_t loadConfigFromNVS();
    esp_err_t saveConfigToNVS();
    void drawMenuBackground();
    void drawMenuIcons();
    void drawItemCount(int x, int y, uint8_t count);
    void drawSelectedIndicator(int x, int y);
    bool checkLeverInput(uint8_t *new_state);
    void processLeverInput(uint8_t lever_state);
    void executeSelectedItem();
    void startSlideAnimation(menu_animation_t direction);
    void updateAnimation();
    void drawMenuIcon(menu_item_t item, int x, int y, bool selected);
    int calculateIconX(int item_index, int selected_index);

public:
    /**
     * @brief コンストラクタ
     * @param tft LCDディスプレイ参照
     * @param canvas 描画キャンバス参照
     * @param mcpExpander IOエキスパンダー参照
     * @param renderer パレット画像レンダラー参照
     * @param nvsHandle NVSハンドル
     */
    GameMenu(LGFX_ST7789P3_76x284 *tft,
             M5Canvas *canvas,
             MCP23008 *mcpExpander,
             PaletteImageRenderer *renderer,
             nvs_handle_t nvsHandle);

    ~GameMenu();

    /**
     * @brief メニューシステム初期化
     * @return ESP_OK: 成功, その他: エラー
     */
    esp_err_t begin();

    /**
     * @brief レバー状態からメニュー起動判定
     * @param lever_state 現在のレバー状態（8bit）
     * @return true: メニューを起動すべき, false: 起動しない
     */
    bool shouldActivateMenu(uint8_t lever_state);

    /**
     * @brief メニュー表示・操作実行
     * @return menu_result_t メニュー選択結果
     */
    menu_result_t runMenu();

    /**
     * @brief サウンド設定を取得
     * @return true: サウンド有効, false: サウンド無効
     */
    bool isSoundEnabled() const;

    /**
     * @brief サウンド設定を変更
     * @param enabled true: サウンド有効, false: サウンド無効
     */
    void setSoundEnabled(bool enabled);

    /**
     * @brief アイテム所持数を取得
     * @return item_counts_t アイテム所持数構造体
     */
    item_counts_t getItemCounts() const;

    /**
     * @brief アイテム追加
     * @param item 追加するアイテム種類
     * @param count 追加数量（デフォルト: 1）
     * @return ESP_OK: 成功, その他: エラー
     */
    esp_err_t addItem(menu_item_t item, uint8_t count = 1);

    /**
     * @brief アイテム使用（所持数減少）
     * @param item 使用するアイテム種類
     * @return ESP_OK: 成功, ESP_ERR_NOT_FOUND: アイテム不足
     */
    esp_err_t useItem(menu_item_t item);

    /**
     * @brief デバッグ情報表示
     */
    void printDebugInfo() const;

    // 背景保存・復元
    esp_err_t saveCurrentScreenAsBackground();
    void clearSavedBackground();
    bool hasBackgroundSaved() const { return background_saved_; }
};

// グローバル関数（C形式インターフェース）

/**
 * @brief メニューシステム初期化（グローバル関数）
 * @param tft LCDディスプレイ参照
 * @param canvas 描画キャンバス参照
 * @param mcpExpander IOエキスパンダー参照
 * @param renderer パレット画像レンダラー参照
 * @param nvsHandle NVSハンドル
 * @return ESP_OK: 成功, その他: エラー
 */
esp_err_t initGameMenu(LGFX_ST7789P3_76x284 *tft,
                       M5Canvas *canvas,
                       MCP23008 *mcpExpander,
                       PaletteImageRenderer *renderer,
                       nvs_handle_t nvsHandle);

/**
 * @brief メニューアクティベート判定（グローバル関数）
 * @param lever_state レバー状態
 * @return true: メニューを起動, false: 起動しない
 */
bool checkMenuActivation(uint8_t lever_state);

/**
 * @brief メニュー実行（グローバル関数）
 * @return menu_result_t 選択結果
 */
menu_result_t executeGameMenu();

/**
 * @brief サウンド設定取得（グローバル関数）
 * @return true: サウンド有効, false: 無効
 */
bool getGameSoundEnabled();

/**
 * @brief アイテム追加（グローバル関数）
 * @param item アイテム種類
 * @param count 追加数量
 * @return ESP_OK: 成功, その他: エラー
 */
esp_err_t addGameItem(menu_item_t item, uint8_t count);

// 背景保存・復元関数を追加
esp_err_t saveCurrentScreenAsBackground();
void clearSavedBackground();