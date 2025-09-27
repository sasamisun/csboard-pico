/**
 * @file buzzer.hpp
 * @brief M5StampPico用ブザー制御ライブラリ 🔊
 * @author 猫エンジニア
 * @date 2025年9月21日
 * 
 * M5StampPicoのG25ピンに接続されたブザーを制御するためのライブラリ
 * PWM信号を使って、様々な音色やメロディを再生できる
 */

#pragma once

#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ブザーピン設定
#define BUZZER_PIN_NUM          25      // M5StampPico G25ピン
#define BUZZER_PWM_CHANNEL      LEDC_CHANNEL_0
#define BUZZER_PWM_TIMER        LEDC_TIMER_0
#define BUZZER_PWM_MODE         LEDC_LOW_SPEED_MODE
#define BUZZER_PWM_RESOLUTION   LEDC_TIMER_8_BIT
#define BUZZER_PWM_DUTY         128     // 50% duty cycle (0-255)

// 効果音の種類
typedef enum {
    BUZZER_SOUND_CAT_STRETCH = 0,   // 猫が伸びる音
    BUZZER_SOUND_OMIKUJI,           // おみくじを引く音  
    BUZZER_SOUND_SCORE_GET,         // スコア獲得音
    BUZZER_SOUND_ERROR,             // エラー音
    BUZZER_SOUND_DECISION,          // 決定音
    BUZZER_SOUND_CANCEL,            // キャンセル音
    BUZZER_SOUND_STARTUP,           // 起動音
    BUZZER_SOUND_OMIKUJI_NO,
    BUZZER_SOUND_OMIKUJI_HAZURE,
    BUZZER_SOUND_OMIKUJI_ARATI,
    BUZZER_SOUND_OMIKUJI_SPECIAL,
    BUZZER_SOUND_COUNT // 効果音の総数
} buzzer_sound_t;

// 音階定義（周波数 Hz）
typedef struct {
    uint32_t frequency;     // 周波数
    uint32_t duration_ms;   // 再生時間（ミリ秒）
} buzzer_note_t;

/**
 * ブザー初期化
 * PWMチャンネルを設定してブザーを使用可能にする
 * 
 * @return esp_err_t 初期化結果
 */
esp_err_t buzzer_init(void);

/**
 * 効果音を再生
 * 指定された効果音パターンを再生する（ノンブロッキング）
 * 
 * @param sound_type 再生する効果音の種類
 * @return esp_err_t 再生開始結果
 */
esp_err_t buzzer_play_sound(buzzer_sound_t sound_type);

/**
 * 単一の音を再生
 * 指定された周波数と時間で音を再生する
 * 
 * @param frequency 周波数（Hz）
 * @param duration_ms 再生時間（ミリ秒）
 * @return esp_err_t 再生結果
 */
esp_err_t buzzer_play_tone(uint32_t frequency, uint32_t duration_ms);

/**
 * ブザーをON/OFF制御
 * ブザー機能全体を有効/無効にする
 * 
 * @param enabled true: ブザー有効, false: ブザー無効
 */
void buzzer_set_enabled(bool enabled);

/**
 * ブザーの有効状態を取得
 * 
 * @return bool ブザーが有効かどうか
 */
bool buzzer_is_enabled(void);

/**
 * ブザーを停止
 * 現在再生中の音を即座に停止する
 * 
 * @return esp_err_t 停止結果
 */
esp_err_t buzzer_stop(void);

/**
 * ブザーリソースを解放
 * PWMチャンネルを解放してリソースをクリーンアップする
 * 
 * @return esp_err_t 解放結果
 */
esp_err_t buzzer_deinit(void);

// デバッグ用：手動で音階を1つずつ再生
void debug_manual_startup_sequence();