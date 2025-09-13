/*
 * RetroGamePaletteImage.hpp
 * レトロゲーム風16色パレット画像システム for M5StampPico + ST7789P3
 * 
 * 特徴:
 * - 16色パレット（1色は透明色）
 * - 1バイトに2ピクセル格納でメモリ効率75%削減
 * - M5Canvas経由での高速描画
 * - 透明色対応スプライト描画
 * - csboard-picoプロジェクト対応
 */

#pragma once

#include <M5Unified.h>
#include "LGFX_ST7789P3_76x284.hpp"

/**
 * 16色レトロパレット定義
 * インデックス0は透明色として予約
 */
struct RetroColorPalette {
    static constexpr uint8_t TRANSPARENT_INDEX = 0;  // 透明色インデックス
    static constexpr uint8_t MAX_COLORS = 16;        // パレット色数
    
    uint16_t colors[MAX_COLORS];  // RGB565形式のカラーパレット
    
    // デフォルトコンストラクタ（レトロゲーム風16色パレット）
    RetroColorPalette();
    
    /**
     * クラシックレトロゲーム風カラーパレット初期化
     * ファミコン風の色合いを再現
     */
    void initClassicRetroColors();
    
    /**
     * グレースケールパレット初期化
     * モノクロゲーム用
     */
    void initGrayscalePalette();
    
    /**
     * セピア調パレット初期化
     * レトロ写真風
     */
    void initSepiaPalette();
    
    /**
     * カスタムパレット設定
     * @param index パレットインデックス（0-15）
     * @param color RGB565色
     */
    void setColor(uint8_t index, uint16_t color);
    
    /**
     * RGB888からRGB565への変換ヘルパー
     * @param r 赤成分（0-255）
     * @param g 緑成分（0-255）
     * @param b 青成分（0-255）
     * @return RGB565色
     */
    static uint16_t rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * HSVからRGB565への変換ヘルパー
     * @param h 色相（0-360）
     * @param s 彩度（0-100）
     * @param v 明度（0-100）
     * @return RGB565色
     */
    static uint16_t hsvToRgb565(uint16_t h, uint8_t s, uint8_t v);
};

/**
 * パレット画像データ構造体
 * 1バイトに2ピクセル格納（4bit/pixel）
 */
struct PaletteImageData {
    const uint8_t* data;           // 画像データ配列（コンスト）
    RetroColorPalette palette;     // カラーパレット
    int width, height;             // 画像サイズ
    size_t dataSize;               // データサイズ（バイト）
    
    /**
     * コンストラクタ
     * @param imageData 画像データ配列のポインタ
     * @param w 画像幅
     * @param h 画像高さ
     * @param customPalette カスタムパレット（nullptr = デフォルト）
     */
    PaletteImageData(const uint8_t* imageData, int w, int h, const RetroColorPalette* customPalette = nullptr);
    
    /**
     * 指定座標のパレットインデックスを取得
     * @param x X座標
     * @param y Y座標
     * @return パレットインデックス（0-15）
     */
    uint8_t getPixelIndex(int x, int y) const;
    
    /**
     * 指定座標のRGB565色を取得
     * @param x X座標
     * @param y Y座標
     * @return RGB565色
     */
    uint16_t getPixelColor(int x, int y) const;
    
    /**
     * 透明ピクセルかどうかチェック
     * @param x X座標
     * @param y Y座標
     * @return true=透明, false=不透明
     */
    bool isTransparent(int x, int y) const;
    
    /**
     * メモリ使用量を計算
     * @return 使用メモリ量（バイト）
     */
    size_t getMemoryUsage() const;
    
    /**
     * パレットを変更
     * @param newPalette 新しいパレット
     */
    void setPalette(const RetroColorPalette& newPalette);
};

/**
 * パレット画像描画クラス
 * M5Canvas経由での高速描画を提供
 */
class PaletteImageRenderer {
private:
    LGFX_ST7789P3_76x284* display;    // ディスプレイインスタンス
    M5Canvas* canvas;                 // 描画用キャンバス
    bool canvasOwned;                 // キャンバス所有フラグ
    
    uint16_t* lineBuffer;             // ライン描画用バッファ
    size_t bufferSize;                // バッファサイズ
    
public:
    /**
     * コンストラクタ（外部キャンバス使用）
     * @param gfx ディスプレイインスタンス
     * @param cnv 既存のキャンバス
     */
    PaletteImageRenderer(LGFX_ST7789P3_76x284* gfx, M5Canvas* cnv);
    
    /**
     * コンストラクタ（自動キャンバス作成）
     * @param gfx ディスプレイインスタンス
     * @param canvasWidth キャンバス幅
     * @param canvasHeight キャンバス高さ
     */
    PaletteImageRenderer(LGFX_ST7789P3_76x284* gfx, int canvasWidth, int canvasHeight);
    
    /**
     * デストラクタ
     */
    ~PaletteImageRenderer();
    
    /**
     * ライン描画用バッファを初期化
     * @param maxWidth 最大描画幅
     */
    void initLineBuffer(int maxWidth);
    
    /**
     * パレット画像をキャンバスに描画（透明色対応）
     * @param img パレット画像データ
     * @param offsetX 描画開始X座標
     * @param offsetY 描画開始Y座標
     * @param useTransparency 透明色を使用するか
     */
    void drawToCanvas(const PaletteImageData& img, int offsetX = 0, int offsetY = 0, bool useTransparency = true);
    
    /**
     * パレット画像をキャンバスに高速描画（不透明）
     * @param img パレット画像データ
     * @param offsetX 描画開始X座標
     * @param offsetY 描画開始Y座標
     */
    void drawToCanvasOpaque(const PaletteImageData& img, int offsetX = 0, int offsetY = 0);
    
    /**
     * パレット画像をキャンバスに描画（スケーリング対応）
     * @param img パレット画像データ
     * @param offsetX 描画開始X座標
     * @param offsetY 描画開始Y座標
     * @param scaleX X方向スケール（1.0=等倍）
     * @param scaleY Y方向スケール（1.0=等倍）
     * @param useTransparency 透明色を使用するか
     */
    void drawToCanvasScaled(const PaletteImageData& img, int offsetX, int offsetY, 
                           float scaleX, float scaleY, bool useTransparency = true);
    
    /**
     * キャンバスをディスプレイにプッシュ（透明色対応）
     * @param x ディスプレイ上のX座標
     * @param y ディスプレイ上のY座標
     * @param transparentColor 透明色（RGB565）
     */
    void pushCanvasToDisplay(int x = 0, int y = 0, uint16_t transparentColor = 0x0000);
    
    /**
     * キャンバスをディスプレイにプッシュ（不透明）
     * @param x ディスプレイ上のX座標
     * @param y ディスプレイ上のY座標
     */
    void pushCanvasToDisplayOpaque(int x = 0, int y = 0);
    
    /**
     * キャンバスをクリア
     * @param color クリア色
     */
    void clearCanvas(uint16_t color = 0x0000);
    
    /**
     * キャンバスの取得
     * @return キャンバスのポインタ
     */
    M5Canvas* getCanvas();
    
    /**
     * ディスプレイサイズの取得
     * @param width 幅の格納先
     * @param height 高さの格納先
     */
    void getDisplaySize(int& width, int& height);
};

/**
 * レトロゲーム用アニメーション管理クラス
 */
class RetroAnimation {
public:
    /**
     * アニメーションフレーム構造体
     */
    struct AnimationFrame {
        const PaletteImageData* image;  // フレーム画像
        uint16_t duration;              // 表示時間（ミリ秒）
        int offsetX, offsetY;           // 表示オフセット
    };
    
private:
    AnimationFrame* frames;           // フレーム配列
    int frameCount;                   // フレーム数
    int currentFrame;                 // 現在のフレーム
    uint32_t lastFrameTime;           // 最後のフレーム更新時間
    bool loop;                        // ループ再生フラグ
    bool playing;                     // 再生中フラグ
    
public:
    /**
     * コンストラクタ
     * @param animFrames フレーム配列
     * @param count フレーム数
     * @param loopAnimation ループ再生するか
     */
    RetroAnimation(AnimationFrame* animFrames, int count, bool loopAnimation = true);
    
    /**
     * アニメーション更新
     * @return フレームが変更された場合true
     */
    bool update();
    
    /**
     * 現在のフレーム画像を取得
     * @return 現在のフレーム画像（nullptr=終了）
     */
    const PaletteImageData* getCurrentFrame();
    
    /**
     * 現在のフレームオフセットを取得
     * @param offsetX X座標の格納先
     * @param offsetY Y座標の格納先
     */
    void getCurrentOffset(int& offsetX, int& offsetY);
    
    /**
     * アニメーション開始
     */
    void start();
    
    /**
     * アニメーション停止
     */
    void stop();
    
    /**
     * アニメーション一時停止/再開
     */
    void pause();
    
    /**
     * アニメーションリセット
     */
    void reset();
    
    /**
     * 再生中かどうか
     * @return true=再生中, false=停止中
     */
    bool isPlaying() const;
};

// ===== サンプル画像データ =====

/**
 * 8x8ピクセルのテストアイコン（ハート型）
 * パレットインデックス0（透明）と2（赤）を使用
 */
extern const uint8_t SAMPLE_HEART_8x8[];

/**
 * 16x16ピクセルのテストキャラクター（顔）
 * 複数色使用サンプル
 */
extern const uint8_t SAMPLE_FACE_16x16[];

/**
 * 8x8ピクセルのコイン
 * アニメーション用
 */
extern const uint8_t SAMPLE_COIN_8x8[];

/**
 * 12x16ピクセルのキャラクター（立ち）
 * RPG風キャラクター
 */
extern const uint8_t SAMPLE_CHAR_STAND_12x16[];

/**
 * 12x16ピクセルのキャラクター（歩き1）
 * RPG風キャラクター
 */
extern const uint8_t SAMPLE_CHAR_WALK1_12x16[];

/**
 * 12x16ピクセルのキャラクター（歩き2）
 * RPG風キャラクター
 */
extern const uint8_t SAMPLE_CHAR_WALK2_12x16[];

/**
 * 基本的な使用方法のサンプルクラス
 */
class RetroGameExample {
public:
    /**
     * 基本描画のサンプル
     * @param display ディスプレイインスタンス
     */
    static void basicUsageExample(LGFX_ST7789P3_76x284* display);
    
    /**
     * アニメーションのサンプル
     * @param display ディスプレイインスタンス
     */
    static void animationExample(LGFX_ST7789P3_76x284* display);
    
    /**
     * キャラクター歩行アニメーションのサンプル
     * @param display ディスプレイインスタンス
     */
    static void characterWalkExample(LGFX_ST7789P3_76x284* display);
    
    /**
     * パレット変更エフェクトのサンプル
     * @param display ディスプレイインスタンス
     */
    static void paletteEffectExample(LGFX_ST7789P3_76x284* display);
};