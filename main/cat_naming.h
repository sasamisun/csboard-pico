/**
 * cat_naming.h
 *
 * 猫の名前付けインターフェース
 * カタカナ文字を使って猫の名前を入力するシステム
 *
 * 機能：
 * - レバー左右でカタカナ文字選択（強弱2段階）
 * - 決定ボタンで文字確定・名前決定
 * - 削除機能とバックスペース
 * - 長押しでいつでも名前確定
 *
 * 作成者: 猫エンジニア
 * 日付: 2025年9月21日
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "M5Unified.h"
#include "LGFX_ST7789P3_76x284.hpp"
#include "mcp23008_driver.h"
#include "RetroGamePaletteImage.hpp"

// 名前付けシステムの設定
#define CAT_NAME_MAX_LENGTH 8        // 最大名前長（終端文字除く）
#define LONG_PRESS_TIME_MS 1000      // 長押し判定時間（ミリ秒）
#define INPUT_DEBOUNCE_TIME_MS 150   // 入力デバウンス時間
#define CURSOR_BLINK_INTERVAL_MS 500 // カーソル点滅間隔

// レバー入力定義（01反転後の値）
#define LEVER_LEFT_LIGHT 0x10   // 左ちょっと（1文字戻る）
#define LEVER_LEFT_STRONG 0x18  // 左強く（5文字戻る）
#define LEVER_RIGHT_LIGHT 0x01  // 右ちょっと（1文字進む）
#define LEVER_RIGHT_STRONG 0x03 // 右強く（5文字進む）
#define LEVER_DECIDE 0x04       // 決定ボタン

// 名前付けシステムの状態
typedef enum
{
    CAT_NAMING_ACTIVE,    // 名前入力中
    CAT_NAMING_COMPLETED, // 名前確定完了
    CAT_NAMING_CANCELLED  // キャンセル（未使用）
} cat_naming_state_t;

// 特殊文字のインデックス
typedef enum
{
    SPECIAL_CHAR_DELETE = 84, // "(削除)" の位置
    SPECIAL_CHAR_FINISH = 85  // "(完了)" の位置
} special_char_index_t;

/**
 * 猫名前付けクラス
 * 名前入力UIの管理と操作を行う
 */
class CatNaming
{
private:
    // 表示関連
    LGFX_ST7789P3_76x284 *tft;      // TFTディスプレイ
    M5Canvas *canvas;               // 描画キャンバス
    MCP23008 *mcpExpander;          // IOエキスパンダ
    nvs_handle_t nvsHandle;         // NVS保存ハンドル
    PaletteImageRenderer *renderer; // パレット画像レンダラー

    // 入力状態管理
    uint8_t last_lever_state;         // 前回のレバー状態
    unsigned long last_input_time;    // 最後の入力時間
    unsigned long decide_press_start; // 決定ボタン押下開始時間
    bool decide_button_pressed;       // 決定ボタン押下中フラグ
    bool cursor_visible;              // カーソル表示状態
    unsigned long last_cursor_blink;  // 最後のカーソル点滅時間

    // 名前入力状態
    uint8_t cat_name[CAT_NAME_MAX_LENGTH + 1]; // 猫の名前（終端含む）
    int current_name_length;                   // 現在の名前の長さ
    int current_char_index;                    // 現在選択中の文字インデックス
    cat_naming_state_t naming_state;           // 名前付けの状態

    // 文字セット定義
    static const char *CHARACTER_SET;  // カタカナ文字セット
    static const int TOTAL_CHARACTERS; // 文字総数

    // レバー状態保存用
    //  レバー長押し用変数を追加
    unsigned long lever_press_start_time;                         // レバー押下開始時間
    bool lever_currently_pressed;                                 // レバー押下中フラグ
    uint8_t current_lever_direction;                              // 現在のレバー方向（左右）
    unsigned long last_continuous_move_time;                      // 最後の連続移動時間
    static const unsigned long LONG_HOLD_TIME_MS = 500;           // 長押し判定時間
    static const unsigned long CONTINUOUS_MOVE_INTERVAL_MS = 150; // 連続移動間隔

    // 内部メソッド
    void initializeNaming();                                // 名前付け初期化
    void handleLeverInput();                                // レバー入力処理
    void handleDecideButton();                              // 決定ボタン処理
    void updateCharacterSelection(int direction, int step); // 文字選択更新
    void addCharacterToName();                              // 名前に文字追加
    void deleteLastCharacter();                             // 最後の文字削除
    void finishNaming();                                    // 名前付け完了
    void drawNamingInterface();                             // 名前付けUI描画
    void drawCurrentCharacter();                            // 現在の文字表示
    void drawCurrentName();                                 // 現在の名前表示
    void drawInstructions();                                // 操作説明表示
    void drawCursor();                                      // カーソル描画
    bool isSpecialCharacter(int index);                     // 特殊文字判定
    const char *getCharacterAt(int index);                  // 指定位置の文字取得
    uint8_t convertCharToNumber(const char *katakana);      // カタカナを数字に変換

public:
    /**
     * コンストラクタ
     * @param display TFTディスプレイのポインタ
     * @param draw_canvas 描画用キャンバスのポインタ
     * @param mcp IOエキスパンダのポインタ
     * @param nvs_handle NVSハンドル
     */
    CatNaming(LGFX_ST7789P3_76x284 *display,
              M5Canvas *draw_canvas,
              MCP23008 *mcp,
              nvs_handle_t nvs_handle,
              PaletteImageRenderer *pirenderer);

    /**
     * 名前付けシステム開始
     * ブロッキング関数：名前が決定されるまで戻らない
     * @return 決定された猫の名前（uint8_t配列）
     */
    esp_err_t startNaming(uint8_t *result_name);

    /**
     * 名前付けループ更新
     * メインループから呼び出される非ブロッキング更新
     * @return 名前付けの現在状態
     */
    cat_naming_state_t updateNaming();

    /**
     * 名前をNVSに保存
     * @param name 保存する名前
     * @return 保存結果
     */
    esp_err_t saveNameToNVS(const uint8_t *name);

    /**
     * NVSから名前を読み込み
     * @param name 読み込み先バッファ
     * @return 読み込み結果
     */
    esp_err_t loadNameFromNVS(uint8_t *name);

    /**
     * カタカナ文字を画面に表示用文字列に変換
     * @param name_array 数字配列の名前
     * @param output_str 出力文字列バッファ
     * @param buffer_size バッファサイズ
     */
    void convertNameToDisplayString(const uint8_t *name_array, char *output_str, size_t buffer_size);

    /**
     * デバッグ用：現在の状態をログ出力
     */
    void printDebugInfo();
};

// グローバル関数（app_mainから呼び出し用）

/**
 * 猫名前付けシステムの初期化と実行
 * 初回起動時やリセット後に呼び出される
 * @param display TFTディスプレイ
 * @param canvas 描画キャンバス
 * @param mcp IOエキスパンダ
 * @param nvs_handle NVSハンドル
 * @param result_name 結果の名前を格納するバッファ
 * @return 実行結果
 */
esp_err_t runCatNamingSystem(LGFX_ST7789P3_76x284 *display,
                             M5Canvas *canvas,
                             MCP23008 *mcp,
                             nvs_handle_t nvs_handle,
                             uint8_t *result_name);