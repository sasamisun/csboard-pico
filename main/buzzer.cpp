/**
 * @file buzzer.cpp
 * @brief M5StampPico用ブザー制御ライブラリ実装
 * @author 猫エンジニア
 * @date 2025年9月21日
 * 
 * ブザー制御の実装ファイル
 * PWMを使用してG25ピンのブザーを制御する
 */

#include "buzzer.hpp"

static const char* TAG = "BUZZER";

// ブザー制御状態
static bool buzzer_enabled = true;
static bool buzzer_initialized = false;
static TaskHandle_t buzzer_task_handle = NULL;

// 音階定義（周波数 Hz）- ピアノの鍵盤に対応
#define NOTE_C4     262     // ド
#define NOTE_D4     294     // レ
#define NOTE_E4     330     // ミ
#define NOTE_F4     349     // ファ
#define NOTE_G4     392     // ソ
#define NOTE_A4     440     // ラ
#define NOTE_B4     494     // シ
#define NOTE_C5     523     // 高いド
#define NOTE_D5     587     // 高いレ
#define NOTE_E5     659     // 高いミ
#define NOTE_F5     698     // 高いファ
#define NOTE_G5     784     // 高いソ
#define NOTE_A5     880     // 高いラ
#define NOTE_B5     988     // 高いシ
#define NOTE_C6     1047    // 超高いド
#define NOTE_D6     1175    // 超高いレ
#define NOTE_E6     1319    // 超高いミ
#define NOTE_F6     1397    // 超高いファ
#define NOTE_G6     1568    // 超高いソ
#define NOTE_A6     1760    // 超高いラ
#define NOTE_B6     1976    // 超高いシ
#define NOTE_C7     2093    // 激高いド
#define NOTE_D7     2349    // 激高いレ
#define NOTE_E7     2637    // 激高いミ
#define NOTE_F7     2794    // 激高いファ
#define NOTE_G7     3136    // 激高いソ
#define NOTE_A7     3520    // 激高いラ
#define NOTE_B7     3951    // 激高いシ
#define NOTE_C8     4186    // 最高音ド（ピアノ最高音）

#define NOTE_SILENT 0       // 無音

// 効果音パターン定義
static const buzzer_note_t sound_patterns[][10] = {
    // BUZZER_SOUND_CAT_STRETCH - 猫が伸びる音（上昇音階）
    {
        {NOTE_C4, 100},     // ド
        {NOTE_E4, 100},     // ミ
        {NOTE_G4, 100},     // ソ
        {NOTE_C5, 200},     // 高いド
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_OMIKUJI - おみくじを引く音（神秘的）
    {
        {NOTE_A4, 150},     // ラ
        {NOTE_F4, 150},     // ファ
        {NOTE_D5, 150},     // 高いレ
        {NOTE_B4, 300},     // シ
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_SCORE_GET - スコア獲得音（明るい音）
    {
        {NOTE_C5, 100},     // 高いド
        {NOTE_E5, 100},     // 高いミ
        {NOTE_G5, 100},     // 高いソ
        {NOTE_C6, 250},     // 超高いド
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_ERROR - エラー音（低い警告音）
    {
        {200, 150},         // 低い音
        {NOTE_SILENT, 50},  // 無音
        {200, 150},         // 低い音
        {NOTE_SILENT, 50},  // 無音
        {200, 200},         // 低い音
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_DECISION - 決定音（クリック音）
    {
        {NOTE_C5, 100},     // 高いド
        {NOTE_G5, 150},     // 高いソ
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_CANCEL - キャンセル音（下降音）
    {
        {NOTE_G4, 100},     // ソ
        {NOTE_E4, 100},     // ミ
        {NOTE_C4, 200},     // ド
        {NOTE_SILENT, 0}    // 終了マーカー
    },
    
    // BUZZER_SOUND_STARTUP - 起動音（楽しい音）
    {
        {NOTE_G5, 100},     // ド
        {NOTE_A5, 100},     // レ
        {NOTE_A6, 100},     // ミ
        {NOTE_SILENT, 0}    // 終了マーカー
    }
};

/**
 * PWMで指定周波数の音を出力
 * @param frequency 周波数（Hz）、0で無音
 */
static esp_err_t buzzer_set_frequency(uint32_t frequency)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (frequency == 0) {
        // 無音：duty=0にしてからPWM停止
        esp_err_t err = ledc_set_duty(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL, 0);
        if (err != ESP_OK) {
            return err;
        }
        err = ledc_update_duty(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL);
        if (err != ESP_OK) {
            return err;
        }
        return ledc_stop(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL, 0);
    } else {
        // 指定周波数でPWM出力
        esp_err_t err = ledc_set_freq(BUZZER_PWM_MODE, BUZZER_PWM_TIMER, frequency);
        if (err != ESP_OK) {
            return err;
        }
        
        // PWM出力開始
        err = ledc_set_duty(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL, BUZZER_PWM_DUTY);
        if (err != ESP_OK) {
            return err;
        }
        
        // ★ 重要：duty設定を実際のハードウェアに反映
        return ledc_update_duty(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL);
    }
}

/**
 * ブザー再生タスク
 * 効果音パターンを順次再生する
 */
static void buzzer_play_task(void* parameter)
{
    buzzer_sound_t sound_type = (buzzer_sound_t)(uintptr_t)parameter;
    
    if (sound_type >= BUZZER_SOUND_COUNT) {
        ESP_LOGE(TAG, "Invalid sound type: %d", sound_type);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGD(TAG, "Playing sound pattern: %d", sound_type);

    // 効果音パターンを順次再生
    const buzzer_note_t* pattern = sound_patterns[sound_type];
    
    for (int i = 0; i < 10; i++) {
        // 終了マーカーチェック
        if (pattern[i].frequency == NOTE_SILENT && pattern[i].duration_ms == 0) {
            break;
        }

        // ブザーが無効になった場合は停止
        if (!buzzer_enabled) {
            break;
        }

        // 音を出力
        if (buzzer_enabled) {
            buzzer_set_frequency(pattern[i].frequency);
        }

        // 指定時間待機
        if (pattern[i].duration_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(pattern[i].duration_ms));
        }
    }

    // 再生終了時は無音にする
    buzzer_set_frequency(0);
    
    ESP_LOGD(TAG, "Sound pattern %d playback completed", sound_type);
    
    // タスクを終了
    buzzer_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t buzzer_init(void)
{
    if (buzzer_initialized) {
        ESP_LOGW(TAG, "Buzzer already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing buzzer on GPIO%d...", BUZZER_PIN_NUM);

    // PWMタイマー設定
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = BUZZER_PWM_MODE;
    timer_config.timer_num = BUZZER_PWM_TIMER;
    timer_config.duty_resolution = BUZZER_PWM_RESOLUTION;
    timer_config.freq_hz = 1000;  // 初期周波数（後で変更される）
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM timer: %s", esp_err_to_name(err));
        return err;
    }

    // PWMチャンネル設定
    ledc_channel_config_t channel_config = {};
    channel_config.speed_mode = BUZZER_PWM_MODE;
    channel_config.channel = BUZZER_PWM_CHANNEL;
    channel_config.timer_sel = BUZZER_PWM_TIMER;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.gpio_num = BUZZER_PIN_NUM;
    channel_config.duty = 0;  // 初期は無音
    channel_config.hpoint = 0;

    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM channel: %s", esp_err_to_name(err));
        return err;
    }

    buzzer_initialized = true;
    buzzer_enabled = true;

    ESP_LOGI(TAG, "Buzzer initialization completed successfully");
    return ESP_OK;
}

esp_err_t buzzer_play_sound(buzzer_sound_t sound_type)
{
    if (!buzzer_initialized) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buzzer_enabled) {
        ESP_LOGD(TAG, "Buzzer disabled, skipping sound playback");
        return ESP_OK;
    }

    if (sound_type >= BUZZER_SOUND_COUNT) {
        ESP_LOGE(TAG, "Invalid sound type: %d", sound_type);
        return ESP_ERR_INVALID_ARG;
    }

    // 既存の再生タスクがある場合は停止
    if (buzzer_task_handle != NULL) {
        vTaskDelete(buzzer_task_handle);
        buzzer_task_handle = NULL;
        buzzer_set_frequency(0);  // 無音にする
    }

    // 新しい再生タスクを作成
    BaseType_t result = xTaskCreate(
        buzzer_play_task,
        "buzzer_play",
        2048,
        (void*)(uintptr_t)sound_type,
        5,
        &buzzer_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create buzzer play task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Started playing sound type: %d", sound_type);
    return ESP_OK;
}

esp_err_t buzzer_play_tone(uint32_t frequency, uint32_t duration_ms)
{
    if (!buzzer_initialized) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buzzer_enabled) {
        ESP_LOGD(TAG, "Buzzer disabled, skipping tone playback");
        return ESP_OK;
    }

    // 既存の再生を停止
    buzzer_stop();

    // 音を出力
    esp_err_t err = buzzer_set_frequency(frequency);
    if (err != ESP_OK) {
        return err;
    }

    // 指定時間待機
    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }

    // 無音にする
    buzzer_set_frequency(0);

    ESP_LOGD(TAG, "Played tone: %ld Hz for %ld ms", frequency, duration_ms);
    return ESP_OK;
}

void buzzer_set_enabled(bool enabled)
{
    buzzer_enabled = enabled;
    
    ESP_LOGI(TAG, "Buzzer %s", enabled ? "enabled" : "disabled");

    // 無効化された場合は再生を停止
    if (!enabled) {
        buzzer_stop();
    }
}

bool buzzer_is_enabled(void)
{
    return buzzer_enabled;
}

esp_err_t buzzer_stop(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 再生タスクを停止
    if (buzzer_task_handle != NULL) {
        vTaskDelete(buzzer_task_handle);
        buzzer_task_handle = NULL;
    }

    // 無音にする
    esp_err_t err = buzzer_set_frequency(0);
    
    ESP_LOGD(TAG, "Buzzer stopped");
    return err;
}

esp_err_t buzzer_deinit(void)
{
    if (!buzzer_initialized) {
        ESP_LOGW(TAG, "Buzzer not initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing buzzer...");

    // 再生を停止
    buzzer_stop();

    // PWMチャンネルを停止
    ledc_stop(BUZZER_PWM_MODE, BUZZER_PWM_CHANNEL, 0);

    buzzer_initialized = false;
    buzzer_enabled = false;

    ESP_LOGI(TAG, "Buzzer deinitialization completed");
    return ESP_OK;
}


// デバッグ用：手動で音階を1つずつ再生
void debug_manual_startup_sequence()
{
    const char* TAG = "BUZZER_DEBUG";
    ESP_LOGI(TAG, "=== 手動STARTUP シーケンス開始 ===");
    
    // 音階データ（期待値）
    struct {
        uint32_t freq;
        uint32_t duration;
        const char* note_name;
    } notes[] = {
        {262, 100, "ド(C4)"},
        {294, 100, "レ(D4)"},
        {330, 100, "ミ(E4)"},
        {349, 100, "ファ(F4)"},
        {392, 100, "ソ(G4)"},
        {440, 100, "ラ(A4)"},
        {494, 100, "シ(B4)"},
        {523, 300, "高いド(C5)"}
    };
    
    int num_notes = sizeof(notes) / sizeof(notes[0]);
    
    for (int i = 0; i < num_notes; i++) {
        ESP_LOGI(TAG, "音階 %d/%d: %s (%ld Hz, %ld ms)", 
                 i+1, num_notes, notes[i].note_name, notes[i].freq, notes[i].duration);
        
        // 直接PWMで音を出力
        esp_err_t err = buzzer_play_tone(notes[i].freq, notes[i].duration);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "音階 %d 再生失敗: %s", i+1, esp_err_to_name(err));
            break;
        }
        
        // 少し間隔を空ける
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "=== 手動STARTUP シーケンス完了 ===");

        ESP_LOGI(TAG, "通常のSTARTUP音再生開始（3秒後）...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "buzzer_play_sound(BUZZER_SOUND_STARTUP) 実行...");
    esp_err_t err = buzzer_play_sound(BUZZER_SOUND_STARTUP);
    ESP_LOGI(TAG, "buzzer_play_sound() 結果: %s", esp_err_to_name(err));
}