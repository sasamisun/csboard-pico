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
struct RetroColorPalette
{
    static constexpr uint8_t TRANSPARENT_INDEX = 0; // 透明色インデックス
    static constexpr uint8_t MAX_COLORS = 16;       // パレット色数

    uint16_t colors[MAX_COLORS]; // RGB565形式のカラーパレット

    // デフォルトコンストラクタ（レトロゲーム風16色パレット）
    RetroColorPalette();

    void initClassicRetroColors()
    {
        // ファミコン風16色パレット
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xFFFF;  // 白
        colors[2] = 0xF800;  // 赤
        colors[3] = 0x07E0;  // 緑
        colors[4] = 0x001F;  // 青
        colors[5] = 0xFFE0;  // 黄
        colors[6] = 0xF81F;  // マゼンタ
        colors[7] = 0x07FF;  // シアン
        colors[8] = 0x8410;  // グレー
        colors[9] = 0xFC00;  // オレンジ
        colors[10] = 0x8000; // ダークレッド
        colors[11] = 0x0400; // ダークグリーン
        colors[12] = 0x0010; // ダークブルー
        colors[13] = 0x8400; // ブラウン
        colors[14] = 0x4208; // ダークグレー
        colors[15] = 0x2104; // ベリーダーク

    }

    void initGrayscalePalette()
    {
        colors[0] = 0x0000; // 透明色（黒）

        // グレースケール15段階
        for (int i = 1; i < MAX_COLORS; i++)
        {
            uint8_t level = (i * 255) / (MAX_COLORS - 1);
            colors[i] = rgb888ToRgb565(level, level, level);
        }

    }

    void initSepiaPalette()
    {
        colors[0] = 0x0000; // 透明色（黒）

        // セピア調15段階
        for (int i = 1; i < MAX_COLORS; i++)
        {
            float ratio = (float)i / (MAX_COLORS - 1);
            uint8_t r = (uint8_t)(ratio * 255 * 0.8f); // 赤みを強く
            uint8_t g = (uint8_t)(ratio * 255 * 0.6f); // 緑は中程度
            uint8_t b = (uint8_t)(ratio * 255 * 0.4f); // 青は弱く
            colors[i] = rgb888ToRgb565(r, g, b);
        }

    }

    void initGameBoyColors()
    {
        // ゲームボーイ風グリーン単色グラデーション
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0x9FE6;  // ライトグリーン（最明色）
        colors[2] = 0x8FA5;  //
        colors[3] = 0x7F64;  //
        colors[4] = 0x6F23;  //
        colors[5] = 0x5EE2;  //
        colors[6] = 0x4EA1;  //
        colors[7] = 0x3E60;  //
        colors[8] = 0x2E1F;  //
        colors[9] = 0x1DDE;  //
        colors[10] = 0x0D9D; //
        colors[11] = 0x0D5C; //
        colors[12] = 0x051B; //
        colors[13] = 0x04DA; //
        colors[14] = 0x0499; //
        colors[15] = 0x0258; // ダークグリーン（最暗色）

    }

    void initVividColors()
    {
        // ビビットカラー（鮮やかで目立つ色）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xFFFF;  // 純白
        colors[2] = 0xF800;  // 鮮やか赤
        colors[3] = 0x07E0;  // 鮮やか緑
        colors[4] = 0x001F;  // 鮮やか青
        colors[5] = 0xFFE0;  // 鮮やか黄
        colors[6] = 0xF81F;  // 鮮やかマゼンタ
        colors[7] = 0x07FF;  // 鮮やかシアン
        colors[8] = 0xFC00;  // 鮮やかオレンジ
        colors[9] = 0x8000;  // ディープレッド
        colors[10] = 0x0400; // ディープグリーン
        colors[11] = 0x0010; // ディープブルー
        colors[12] = 0xA800; // ディープオレンジ
        colors[13] = 0x8010; // パープル
        colors[14] = 0x0410; // ティール
        colors[15] = 0x4000; // マルーン

    }

    void initJapaneseColors()
    {
        // 日本色風（和色）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xF7DE;  // 象牙色（ぞうげいろ）
        colors[2] = 0xE8E4;  // 桜色（さくらいろ）
        colors[3] = 0xAD55;  // 若草色（わかくさいろ）
        colors[4] = 0x5C9A;  // 藍色（あいいろ）
        colors[5] = 0xFE60;  // 山吹色（やまぶきいろ）
        colors[6] = 0xA8E3;  // 薄紅色（うすべにいろ）
        colors[7] = 0x6E3C;  // 浅葱色（あさぎいろ）
        colors[8] = 0x8C71;  // 銀鼠色（ぎんねずいろ）
        colors[9] = 0xBC00;  // 朱色（しゅいろ）
        colors[10] = 0x6C64; // 海松色（みるいろ）
        colors[11] = 0x4219; // 紺色（こんいろ）
        colors[12] = 0x7800; // 茶色（ちゃいろ）
        colors[13] = 0x6010; // 菫色（すみれいろ）
        colors[14] = 0x4208; // 鼠色（ねずみいろ）
        colors[15] = 0x2945; // 墨色（すみいろ）

    }

    void initPastelColors()
    {
        // パステルカラー（柔らかい色調）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xF7BE;  // パステル白
        colors[2] = 0xFCE4;  // パステルピンク
        colors[3] = 0xDFE4;  // パステルグリーン
        colors[4] = 0xC6DF;  // パステルブルー
        colors[5] = 0xFF60;  // パステルイエロー
        colors[6] = 0xED1C;  // パステルパープル
        colors[7] = 0xC7FF;  // パステルシアン
        colors[8] = 0xFD20;  // パステルオレンジ
        colors[9] = 0xEB24;  // パステルコーラル
        colors[10] = 0xAFE5; // パステルミント
        colors[11] = 0xB69F; // パステルラベンダー
        colors[12] = 0xFEA0; // パステルピーチ
        colors[13] = 0xD69A; // パステルローズ
        colors[14] = 0x94B2; // パステルグレー
        colors[15] = 0x6B4D; // パステルブラウン

    }

    void initNightColors()
    {
        // 夜景風（ダーク系）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xC618;  // 月白
        colors[2] = 0x8000;  // ダークレッド
        colors[3] = 0x0340;  // ダークグリーン
        colors[4] = 0x0015;  // ダークブルー
        colors[5] = 0x8400;  // ダークゴールド
        colors[6] = 0x6013;  // ダークパープル
        colors[7] = 0x0455;  // ダークティール
        colors[8] = 0x4208;  // チャコール
        colors[9] = 0x4800;  // ダークオレンジ
        colors[10] = 0x0220; // ミッドナイトグリーン
        colors[11] = 0x000A; // ミッドナイトブルー
        colors[12] = 0x2800; // ダークブラウン
        colors[13] = 0x2008; // ダークグレー
        colors[14] = 0x1004; // 濃グレー
        colors[15] = 0x0800; // ディープブラック

    }

    void initSunsetColors()
    {
        // 夕焼け風（オレンジ・レッド系）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xFFDF;  // サンセットホワイト
        colors[2] = 0xFE00;  // ブライトオレンジ
        colors[3] = 0xF800;  // サンセットレッド
        colors[4] = 0xFC00;  // 暖かいオレンジ
        colors[5] = 0xFFE0;  // ゴールデンイエロー
        colors[6] = 0xF400;  // コーラルレッド
        colors[7] = 0xFCC0;  // ピーチオレンジ
        colors[8] = 0xFA00;  // タンジェリン
        colors[9] = 0xD800;  // ダークオレンジ
        colors[10] = 0xB000; // バーントオレンジ
        colors[11] = 0x8800; // ダークゴールド
        colors[12] = 0x6000; // シエナ
        colors[13] = 0x4800; // チョコレート
        colors[14] = 0x3000; // エスプレッソ
        colors[15] = 0x1800; // ダークブラウン

    }

    void initOceanColors()
    {
        // 海風（ブルー・ティール系）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xF7FF;  // 泡白
        colors[2] = 0xC7FF;  // ライトシアン
        colors[3] = 0x87FF;  // アクアブルー
        colors[4] = 0x47FF;  // ディープシアン
        colors[5] = 0x07FF;  // 純シアン
        colors[6] = 0x067F;  // ティール
        colors[7] = 0x063F;  // ディープティール
        colors[8] = 0x041F;  // オーシャンブルー
        colors[9] = 0x021F;  // ディープブルー
        colors[10] = 0x0015; // ネイビー
        colors[11] = 0x0010; // ダークネイビー
        colors[12] = 0x000C; // ミッドナイトブルー
        colors[13] = 0x0008; // アビス
        colors[14] = 0x0004; // ディープアビス
        colors[15] = 0x0001; // オーシャンデプス

    }

    void initForestColors()
    {
        // 森風（グリーン系）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xEFFF;  // 朝露白
        colors[2] = 0xCFE0;  // ライトグリーン
        colors[3] = 0xAFE0;  // フレッシュグリーン
        colors[4] = 0x8FE0;  // スプリンググリーン
        colors[5] = 0x6FE0;  // フォレストグリーン
        colors[6] = 0x4FE0;  // ディープグリーン
        colors[7] = 0x2FE0;  // ジャングルグリーン
        colors[8] = 0x0FC0;  // モスグリーン
        colors[9] = 0x0F80;  // オリーブグリーン
        colors[10] = 0x0F40; // ダークオリーブ
        colors[11] = 0x0700; // ハンターグリーン
        colors[12] = 0x0500; // パイングリーン
        colors[13] = 0x0300; // エバーグリーン
        colors[14] = 0x0200; // ディープフォレスト
        colors[15] = 0x0100; // シャドウグリーン

    }

    void initNeonColors()
    {
        // ネオンカラー（電光掲示板風）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xFFFF;  // ネオン白
        colors[2] = 0xF81F;  // ネオンピンク
        colors[3] = 0x07FF;  // ネオンシアン
        colors[4] = 0xFFE0;  // ネオンイエロー
        colors[5] = 0xFC00;  // ネオンオレンジ
        colors[6] = 0x87F0;  // ネオンライム
        colors[7] = 0x07F0;  // エレクトリックグリーン
        colors[8] = 0x041F;  // エレクトリックブルー
        colors[9] = 0x801F;  // エレクトリックパープル
        colors[10] = 0xF810; // ネオンマゼンタ
        colors[11] = 0x8410; // エレクトリックグレー
        colors[12] = 0x6318; // ネオングレー
        colors[13] = 0x4210; // ダークネオン
        colors[14] = 0x2108; // シャドウネオン
        colors[15] = 0x1084; // ディープネオン

    }

    void initCandyColors()
    {
        // キャンディカラー（お菓子風）
        colors[0] = 0x0000;  // 透明色（黒）
        colors[1] = 0xFFDF;  // バニラホワイト
        colors[2] = 0xFCE4;  // ストロベリーピンク
        colors[3] = 0xF5A0;  // ピーチ
        colors[4] = 0xFE60;  // レモンイエロー
        colors[5] = 0xDFE4;  // ミントグリーン
        colors[6] = 0xC6DF;  // ブルーベリー
        colors[7] = 0xED1C;  // グレープパープル
        colors[8] = 0xFD20;  // オレンジシャーベット
        colors[9] = 0xEA69;  // ラズベリー
        colors[10] = 0xAFE5; // ピスタチオ
        colors[11] = 0x9D1F; // ラベンダー
        colors[12] = 0xFCE0; // キャラメル
        colors[13] = 0xD69A; // ローズゴールド
        colors[14] = 0x94B2; // シルバー
        colors[15] = 0x6B4D; // チョコレート

    }

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
struct PaletteImageData
{
    const uint8_t *data;       // 画像データ配列（コンスト）
    RetroColorPalette palette; // カラーパレット
    int width, height;         // 画像サイズ
    size_t dataSize;           // データサイズ（バイト）

    /**
     * コンストラクタ
     * @param imageData 画像データ配列のポインタ
     * @param w 画像幅
     * @param h 画像高さ
     * @param customPalette カスタムパレット（nullptr = デフォルト）
     */
    PaletteImageData(const uint8_t *imageData, int w, int h, const RetroColorPalette *customPalette = nullptr);

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
    void setPalette(const RetroColorPalette &newPalette);
};

/**
 * パレット画像描画クラス
 * M5Canvas経由での高速描画を提供
 */
class PaletteImageRenderer
{
private:
    LGFX_ST7789P3_76x284 *display; // ディスプレイインスタンス
    M5Canvas *canvas;              // 描画用キャンバス
    bool canvasOwned;              // キャンバス所有フラグ

    uint16_t *lineBuffer; // ライン描画用バッファ
    size_t bufferSize;    // バッファサイズ

public:
    /**
     * コンストラクタ（外部キャンバス使用）
     * @param gfx ディスプレイインスタンス
     * @param cnv 既存のキャンバス
     */
    PaletteImageRenderer(LGFX_ST7789P3_76x284 *gfx, M5Canvas *cnv);

    /**
     * コンストラクタ（自動キャンバス作成）
     * @param gfx ディスプレイインスタンス
     * @param canvasWidth キャンバス幅
     * @param canvasHeight キャンバス高さ
     */
    PaletteImageRenderer(LGFX_ST7789P3_76x284 *gfx, int canvasWidth, int canvasHeight);

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
    void drawToCanvas(const PaletteImageData &img, int offsetX = 0, int offsetY = 0, bool useTransparency = true);

    /**
     * パレット画像をキャンバスに高速描画（不透明）
     * @param img パレット画像データ
     * @param offsetX 描画開始X座標
     * @param offsetY 描画開始Y座標
     */
    void drawToCanvasOpaque(const PaletteImageData &img, int offsetX = 0, int offsetY = 0);

    /**
     * パレット画像をキャンバスに描画（スケーリング対応）
     * @param img パレット画像データ
     * @param offsetX 描画開始X座標
     * @param offsetY 描画開始Y座標
     * @param scaleX X方向スケール（1.0=等倍）
     * @param scaleY Y方向スケール（1.0=等倍）
     * @param useTransparency 透明色を使用するか
     */
    void drawToCanvasScaled(const PaletteImageData &img, int offsetX, int offsetY,
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
    M5Canvas *getCanvas();

    /**
     * ディスプレイサイズの取得
     * @param width 幅の格納先
     * @param height 高さの格納先
     */
    void getDisplaySize(int &width, int &height);
};

/**
 * レトロゲーム用アニメーション管理クラス
 */
class RetroAnimation
{
public:
    /**
     * アニメーションフレーム構造体
     */
    struct AnimationFrame
    {
        const PaletteImageData *image; // フレーム画像
        uint16_t duration;             // 表示時間（ミリ秒）
        int offsetX, offsetY;          // 表示オフセット
    };

private:
    AnimationFrame *frames; // フレーム配列
    int frameCount;         // フレーム数
    int currentFrame;       // 現在のフレーム
    uint32_t lastFrameTime; // 最後のフレーム更新時間
    bool loop;              // ループ再生フラグ
    bool playing;           // 再生中フラグ

public:
    /**
     * コンストラクタ
     * @param animFrames フレーム配列
     * @param count フレーム数
     * @param loopAnimation ループ再生するか
     */
    RetroAnimation(AnimationFrame *animFrames, int count, bool loopAnimation = true);

    /**
     * アニメーション更新
     * @return フレームが変更された場合true
     */
    bool update();

    /**
     * 現在のフレーム画像を取得
     * @return 現在のフレーム画像（nullptr=終了）
     */
    const PaletteImageData *getCurrentFrame();

    /**
     * 現在のフレームオフセットを取得
     * @param offsetX X座標の格納先
     * @param offsetY Y座標の格納先
     */
    void getCurrentOffset(int &offsetX, int &offsetY);

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
class RetroGameExample
{
public:
    /**
     * 基本描画のサンプル
     * @param display ディスプレイインスタンス
     */
    static void basicUsageExample(LGFX_ST7789P3_76x284 *display);

    /**
     * アニメーションのサンプル
     * @param display ディスプレイインスタンス
     */
    static void animationExample(LGFX_ST7789P3_76x284 *display);

    /**
     * キャラクター歩行アニメーションのサンプル
     * @param display ディスプレイインスタンス
     */
    static void characterWalkExample(LGFX_ST7789P3_76x284 *display);

    /**
     * パレット変更エフェクトのサンプル
     * @param display ディスプレイインスタンス
     */
    static void paletteEffectExample(LGFX_ST7789P3_76x284 *display);
};