/*
 * RetroGamePaletteImage.cpp
 * レトロゲーム風16色パレット画像システム実装
 * csboard-picoプロジェクト対応
 */

#include "RetroGamePaletteImage.hpp"
#include "esp_log.h"
#include <cmath>

// ログタグ定義
static const char *TAG = "RetroGamePalette";

// ===== RetroColorPalette 実装 =====

RetroColorPalette::RetroColorPalette() {
    initClassicRetroColors();
}

void RetroColorPalette::initClassicRetroColors() {
    // ファミコン風16色パレット
    colors[0]  = 0x0000;  // 透明色（黒）
    colors[1]  = 0xFFFF;  // 白
    colors[2]  = 0xF800;  // 赤
    colors[3]  = 0x07E0;  // 緑
    colors[4]  = 0x001F;  // 青
    colors[5]  = 0xFFE0;  // 黄
    colors[6]  = 0xF81F;  // マゼンタ
    colors[7]  = 0x07FF;  // シアン
    colors[8]  = 0x8410;  // グレー
    colors[9]  = 0xFC00;  // オレンジ
    colors[10] = 0x8000;  // ダークレッド
    colors[11] = 0x0400;  // ダークグリーン
    colors[12] = 0x0010;  // ダークブルー
    colors[13] = 0x8400;  // ブラウン
    colors[14] = 0x4208;  // ダークグレー
    colors[15] = 0x2104;  // ベリーダーク
    
    ESP_LOGI(TAG, "Classic retro colors initialized");
}

void RetroColorPalette::initGrayscalePalette() {
    colors[0] = 0x0000;  // 透明色（黒）
    
    // グレースケール15段階
    for (int i = 1; i < MAX_COLORS; i++) {
        uint8_t level = (i * 255) / (MAX_COLORS - 1);
        colors[i] = rgb888ToRgb565(level, level, level);
    }
    
    ESP_LOGI(TAG, "Grayscale palette initialized");
}

void RetroColorPalette::initSepiaPalette() {
    colors[0] = 0x0000;  // 透明色（黒）
    
    // セピア調15段階
    for (int i = 1; i < MAX_COLORS; i++) {
        float ratio = (float)i / (MAX_COLORS - 1);
        uint8_t r = (uint8_t)(ratio * 255 * 0.8f);  // 赤みを強く
        uint8_t g = (uint8_t)(ratio * 255 * 0.6f);  // 緑は中程度
        uint8_t b = (uint8_t)(ratio * 255 * 0.4f);  // 青は弱く
        colors[i] = rgb888ToRgb565(r, g, b);
    }
    
    ESP_LOGI(TAG, "Sepia palette initialized");
}

void RetroColorPalette::setColor(uint8_t index, uint16_t color) {
    if (index < MAX_COLORS) {
        colors[index] = color;
    }
}

uint16_t RetroColorPalette::rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t RetroColorPalette::hsvToRgb565(uint16_t h, uint8_t s, uint8_t v) {
    // HSV to RGB conversion
    h %= 360;
    float S = s / 100.0f;
    float V = v / 100.0f;
    
    float C = V * S;
    float X = C * (1 - abs(fmod(h / 60.0f, 2) - 1));
    float m = V - C;
    
    float r, g, b;
    if (h < 60) {
        r = C; g = X; b = 0;
    } else if (h < 120) {
        r = X; g = C; b = 0;
    } else if (h < 180) {
        r = 0; g = C; b = X;
    } else if (h < 240) {
        r = 0; g = X; b = C;
    } else if (h < 300) {
        r = X; g = 0; b = C;
    } else {
        r = C; g = 0; b = X;
    }
    
    uint8_t R = (uint8_t)((r + m) * 255);
    uint8_t G = (uint8_t)((g + m) * 255);
    uint8_t B = (uint8_t)((b + m) * 255);
    
    return rgb888ToRgb565(R, G, B);
}

// ===== PaletteImageData 実装 =====

PaletteImageData::PaletteImageData(const uint8_t* imageData, int w, int h, const RetroColorPalette* customPalette) 
    : data(imageData), width(w), height(h) {
    dataSize = (width * height + 1) / 2;  // 1バイトに2ピクセル
    
    if (customPalette) {
        palette = *customPalette;
    }
    // デフォルトパレットは既にコンストラクタで初期化済み
    
    ESP_LOGI(TAG, "PaletteImageData created: %dx%d, %zu bytes", width, height, dataSize);
}

uint8_t PaletteImageData::getPixelIndex(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return RetroColorPalette::TRANSPARENT_INDEX;  // 範囲外は透明
    }
    
    int pixelIndex = y * width + x;
    int byteIndex = pixelIndex / 2;
    
    if (byteIndex >= dataSize) {
        return RetroColorPalette::TRANSPARENT_INDEX;  // 範囲外は透明
    }
    
    // 偶数ピクセル: 下位4bit、奇数ピクセル: 上位4bit
    return (pixelIndex % 2 == 0) ? 
           (data[byteIndex] & 0x0F) : 
           ((data[byteIndex] & 0xF0) >> 4);
}

uint16_t PaletteImageData::getPixelColor(int x, int y) const {
    uint8_t index = getPixelIndex(x, y);
    return palette.colors[index];
}

bool PaletteImageData::isTransparent(int x, int y) const {
    return getPixelIndex(x, y) == RetroColorPalette::TRANSPARENT_INDEX;
}

size_t PaletteImageData::getMemoryUsage() const {
    return dataSize + sizeof(RetroColorPalette);
}

void PaletteImageData::setPalette(const RetroColorPalette& newPalette) {
    palette = newPalette;
}

// ===== PaletteImageRenderer 実装 =====

PaletteImageRenderer::PaletteImageRenderer(LGFX_ST7789P3_76x284* gfx, M5Canvas* cnv) 
    : display(gfx), canvas(cnv), canvasOwned(false), lineBuffer(nullptr), bufferSize(0) {
    ESP_LOGI(TAG, "PaletteImageRenderer created with external canvas");
}

PaletteImageRenderer::PaletteImageRenderer(LGFX_ST7789P3_76x284* gfx, int canvasWidth, int canvasHeight) 
    : display(gfx), canvasOwned(true), lineBuffer(nullptr), bufferSize(0) {
    canvas = new M5Canvas(gfx);
    canvas->createSprite(canvasWidth, canvasHeight);
    ESP_LOGI(TAG, "PaletteImageRenderer created with %dx%d canvas", canvasWidth, canvasHeight);
}

PaletteImageRenderer::~PaletteImageRenderer() {
    if (lineBuffer) {
        free(lineBuffer);
    }
    if (canvasOwned && canvas) {
        canvas->deleteSprite();
        delete canvas;
    }
    ESP_LOGI(TAG, "PaletteImageRenderer destroyed");
}

void PaletteImageRenderer::initLineBuffer(int maxWidth) {
    if (lineBuffer) {
        free(lineBuffer);
    }
    bufferSize = maxWidth;
    lineBuffer = (uint16_t*)malloc(bufferSize * sizeof(uint16_t));
    ESP_LOGI(TAG, "Line buffer initialized: %zu bytes", bufferSize * sizeof(uint16_t));
}

void PaletteImageRenderer::drawToCanvas(const PaletteImageData& img, int offsetX, int offsetY, bool useTransparency) {
    if (!canvas) return;
    
    // 透明色を使用しない場合は高速描画
    if (!useTransparency) {
        drawToCanvasOpaque(img, offsetX, offsetY);
        return;
    }
    
    // 透明色対応の描画（ピクセル単位）
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            uint8_t index = img.getPixelIndex(x, y);
            
            // 透明色をスキップ
            if (index != RetroColorPalette::TRANSPARENT_INDEX) {
                uint16_t color = img.palette.colors[index];
                canvas->drawPixel(x + offsetX, y + offsetY, color);
            }
        }
    }
}

void PaletteImageRenderer::drawToCanvasOpaque(const PaletteImageData& img, int offsetX, int offsetY) {
    if (!canvas) return;
    
    // ライン描画用バッファが未初期化なら初期化
    if (!lineBuffer || bufferSize < img.width) {
        initLineBuffer(img.width);
    }
    
    // ライン単位で高速描画
    for (int y = 0; y < img.height; y++) {
        // 1ライン分をバッファに変換
        for (int x = 0; x < img.width; x++) {
            uint8_t index = img.getPixelIndex(x, y);
            lineBuffer[x] = img.palette.colors[index];
        }
        
        // M5GFXの高速ライン描画
        canvas->pushImage(offsetX, y + offsetY, img.width, 1, lineBuffer);
    }
}

void PaletteImageRenderer::drawToCanvasScaled(const PaletteImageData& img, int offsetX, int offsetY, 
                                             float scaleX, float scaleY, bool useTransparency) {
    if (!canvas) return;
    
    int scaledWidth = (int)(img.width * scaleX);
    int scaledHeight = (int)(img.height * scaleY);
    
    for (int sy = 0; sy < scaledHeight; sy++) {
        for (int sx = 0; sx < scaledWidth; sx++) {
            // スケールされた座標を元の画像座標にマッピング
            int origX = (int)(sx / scaleX);
            int origY = (int)(sy / scaleY);
            
            uint8_t index = img.getPixelIndex(origX, origY);
            
            if (!useTransparency || index != RetroColorPalette::TRANSPARENT_INDEX) {
                uint16_t color = img.palette.colors[index];
                canvas->drawPixel(sx + offsetX, sy + offsetY, color);
            }
        }
    }
}

void PaletteImageRenderer::pushCanvasToDisplay(int x, int y, uint16_t transparentColor) {
    if (!canvas || !display) return;
    
    canvas->pushSprite(display, x, y, transparentColor);
}

void PaletteImageRenderer::pushCanvasToDisplayOpaque(int x, int y) {
    if (!canvas || !display) return;
    
    canvas->pushSprite(display, x, y);
}

void PaletteImageRenderer::clearCanvas(uint16_t color) {
    if (canvas) {
        canvas->fillSprite(color);
    }
}

M5Canvas* PaletteImageRenderer::getCanvas() {
    return canvas;
}

void PaletteImageRenderer::getDisplaySize(int& width, int& height) {
    if (display) {
        width = display->width();
        height = display->height();
    } else {
        width = height = 0;
    }
}

// ===== RetroAnimation 実装 =====

RetroAnimation::RetroAnimation(AnimationFrame* animFrames, int count, bool loopAnimation) 
    : frames(animFrames), frameCount(count), currentFrame(0), loop(loopAnimation), playing(false) {
    lastFrameTime = esp_timer_get_time() / 1000;  // ミリ秒に変換
    ESP_LOGI(TAG, "RetroAnimation created: %d frames, loop=%s", count, loop ? "true" : "false");
}

bool RetroAnimation::update() {
    if (!playing || frameCount <= 0) return false;
    
    uint32_t currentTime = esp_timer_get_time() / 1000;  // ミリ秒に変換
    
    if (currentTime - lastFrameTime >= frames[currentFrame].duration) {
        currentFrame++;
        
        if (currentFrame >= frameCount) {
            if (loop) {
                currentFrame = 0;
            } else {
                playing = false;
                return false;
            }
        }
        
        lastFrameTime = currentTime;
        return true;  // フレームが変更された
    }
    
    return false;
}

const PaletteImageData* RetroAnimation::getCurrentFrame() {
    if (!playing || currentFrame >= frameCount) return nullptr;
    return frames[currentFrame].image;
}

void RetroAnimation::getCurrentOffset(int& offsetX, int& offsetY) {
    if (playing && currentFrame < frameCount) {
        offsetX = frames[currentFrame].offsetX;
        offsetY = frames[currentFrame].offsetY;
    } else {
        offsetX = offsetY = 0;
    }
}

void RetroAnimation::start() {
    playing = true;
    lastFrameTime = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Animation started");
}

void RetroAnimation::stop() {
    playing = false;
    ESP_LOGI(TAG, "Animation stopped");
}

void RetroAnimation::pause() {
    playing = !playing;
    if (playing) {
        lastFrameTime = esp_timer_get_time() / 1000;  // 再開時に時間をリセット
    }
    ESP_LOGI(TAG, "Animation %s", playing ? "resumed" : "paused");
}

void RetroAnimation::reset() {
    currentFrame = 0;
    lastFrameTime = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Animation reset");
}

bool RetroAnimation::isPlaying() const {
    return playing;
}

// ===== サンプル画像データ =====

// 8x8ハート（32バイト）
const uint8_t SAMPLE_HEART_8x8[] = {
    0x00, 0x00, 0x00, 0x00,  // 行1: 00000000
    0x02, 0x20, 0x02, 0x20,  // 行2: 02200220
    0x22, 0x22, 0x22, 0x22,  // 行3: 22222222
    0x22, 0x22, 0x22, 0x22,  // 行4: 22222222
    0x02, 0x22, 0x22, 0x20,  // 行5: 02222220
    0x00, 0x22, 0x22, 0x00,  // 行6: 00222200
    0x00, 0x02, 0x20, 0x00,  // 行7: 00022000
    0x00, 0x00, 0x00, 0x00   // 行8: 00000000
};

// 16x16顔（128バイト）
const uint8_t SAMPLE_FACE_16x16[] = {
    // 簡易実装：中央に笑顔
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 行1-2
    0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,  // 行3-4: 顔の輪郭開始
    0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,  // 行5-6
    0x00, 0x11, 0x11, 0x22, 0x11, 0x22, 0x11, 0x00,  // 行7-8: 目
    0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,  // 行9-10
    0x00, 0x11, 0x33, 0x11, 0x11, 0x11, 0x33, 0x00,  // 行11-12: 口
    0x00, 0x11, 0x11, 0x33, 0x33, 0x33, 0x11, 0x00,  // 行13-14
    0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,  // 行15-16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 残りをゼロ埋め
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 8x8コイン（32バイト）
const uint8_t SAMPLE_COIN_8x8[] = {
    0x00, 0x05, 0x55, 0x00,  // 行1: 00055550
    0x00, 0x55, 0x55, 0x50,  // 行2: 05555550
    0x05, 0x55, 0x55, 0x55,  // 行3: 55555555
    0x05, 0x55, 0x55, 0x55,  // 行4: 55555555
    0x05, 0x55, 0x55, 0x55,  // 行5: 55555555
    0x05, 0x55, 0x55, 0x55,  // 行6: 55555555
    0x00, 0x55, 0x55, 0x50,  // 行7: 05555550
    0x00, 0x05, 0x55, 0x00   // 行8: 00055550
};

// 12x16キャラクター立ち（96バイト）
const uint8_t SAMPLE_CHAR_STAND_12x16[] = {
    // 簡易実装：基本的な人型
    0x00, 0x00, 0x33, 0x33, 0x00, 0x00,  // 頭部
    0x00, 0x03, 0x33, 0x33, 0x30, 0x00,
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,  // 顔
    0x00, 0x03, 0x42, 0x24, 0x30, 0x00,  // 目
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,
    0x00, 0x03, 0x34, 0x43, 0x30, 0x00,  // 口
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,  // 体
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x00, 0x00,  // 腰
    0x00, 0x00, 0x77, 0x77, 0x00, 0x00,  // 足
    0x00, 0x00, 0x77, 0x77, 0x00, 0x00,
    0x00, 0x00, 0x77, 0x77, 0x00, 0x00,
    0x00, 0x07, 0x77, 0x77, 0x70, 0x00   // 靴
};

// 12x16キャラクター歩き1（96バイト）
const uint8_t SAMPLE_CHAR_WALK1_12x16[] = {
    // 歩行ポーズ1：左足前
    0x00, 0x00, 0x33, 0x33, 0x00, 0x00,
    0x00, 0x03, 0x33, 0x33, 0x30, 0x00,
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,
    0x00, 0x03, 0x42, 0x24, 0x30, 0x00,
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,
    0x00, 0x03, 0x34, 0x43, 0x30, 0x00,
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,
    0x00, 0x07, 0x66, 0x66, 0x00, 0x00,  // 左足前
    0x00, 0x07, 0x77, 0x77, 0x00, 0x00,
    0x00, 0x07, 0x77, 0x00, 0x77, 0x00,  // 右足後
    0x00, 0x07, 0x77, 0x00, 0x77, 0x00,
    0x00, 0x77, 0x77, 0x07, 0x77, 0x70
};

// 12x16キャラクター歩き2（96バイト）
const uint8_t SAMPLE_CHAR_WALK2_12x16[] = {
    // 歩行ポーズ2：右足前
    0x00, 0x00, 0x33, 0x33, 0x00, 0x00,
    0x00, 0x03, 0x33, 0x33, 0x30, 0x00,
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,
    0x00, 0x03, 0x42, 0x24, 0x30, 0x00,
    0x00, 0x03, 0x44, 0x44, 0x30, 0x00,
    0x00, 0x03, 0x34, 0x43, 0x30, 0x00,
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x06, 0x66, 0x66, 0x60, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x70, 0x00,  // 右足前
    0x00, 0x00, 0x77, 0x77, 0x70, 0x00,
    0x00, 0x77, 0x00, 0x77, 0x70, 0x00,  // 左足後
    0x00, 0x77, 0x00, 0x77, 0x70, 0x00,
    0x07, 0x77, 0x70, 0x77, 0x77, 0x00
};

// ===== サンプル実装 =====

void RetroGameExample::basicUsageExample(LGFX_ST7789P3_76x284* display) {
    ESP_LOGI(TAG, "=== Basic Usage Example ===");
    
    // 1. パレット画像データ作成
    PaletteImageData heartImage(SAMPLE_HEART_8x8, 8, 8);
    
    // 2. レンダラー初期化（32x32のキャンバス）
    PaletteImageRenderer renderer(display, 32, 32);
    
    // 3. キャンバスをクリア
    renderer.clearCanvas(0x001F);  // 青背景
    
    // 4. ハート画像を描画（透明色対応）
    renderer.drawToCanvas(heartImage, 12, 12, true);
    
    // 5. ディスプレイに表示（透明色は黒）
    renderer.pushCanvasToDisplay(22, 138, 0x0000);  // 中央付近に表示
    
    ESP_LOGI(TAG, "Heart displayed with transparency");
}

void RetroGameExample::animationExample(LGFX_ST7789P3_76x284* display) {
    ESP_LOGI(TAG, "=== Animation Example ===");
    
    PaletteImageData heartImage(SAMPLE_HEART_8x8, 8, 8);
    PaletteImageData coinImage(SAMPLE_COIN_8x8, 8, 8);
    PaletteImageRenderer renderer(display, 76, 284);  // フルスクリーン
    
    for (int frame = 0; frame < 60; frame++) {
        renderer.clearCanvas(0x0010);  // ダークブルー背景
        
        // フレームごとに点滅するハート
        if ((frame / 10) % 2 == 0) {
            renderer.drawToCanvas(heartImage, 34, 100, true);
        }
        
        // 回転するコイン（スケール変更でアニメーション）
        float scale = 0.5f + 0.5f * sin(frame * 0.2f);
        renderer.drawToCanvasScaled(coinImage, 30, 150, scale, 1.0f, true);
        
        renderer.pushCanvasToDisplayOpaque(0, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "Animation complete");
}

void RetroGameExample::characterWalkExample(LGFX_ST7789P3_76x284* display) {
    ESP_LOGI(TAG, "=== Character Walk Example ===");
    
    // アニメーションフレーム設定
    PaletteImageData standImage(SAMPLE_CHAR_STAND_12x16, 12, 16);
    PaletteImageData walk1Image(SAMPLE_CHAR_WALK1_12x16, 12, 16);
    PaletteImageData walk2Image(SAMPLE_CHAR_WALK2_12x16, 12, 16);
    
    RetroAnimation::AnimationFrame walkFrames[] = {
        {&standImage, 500, 0, 0},  // 立ち：500ms
        {&walk1Image, 300, 0, 0},  // 歩き1：300ms
        {&standImage, 200, 0, 0},  // 立ち：200ms
        {&walk2Image, 300, 0, 0}   // 歩き2：300ms
    };
    
    RetroAnimation walkAnimation(walkFrames, 4, true);
    PaletteImageRenderer renderer(display, 76, 284);
    
    walkAnimation.start();
    
    for (int i = 0; i < 200; i++) {  // 20秒間のアニメーション
        renderer.clearCanvas(0x0400);  // ダークグリーン背景
        
        if (walkAnimation.update()) {
            ESP_LOGI(TAG, "Animation frame changed");
        }
        
        const PaletteImageData* currentFrame = walkAnimation.getCurrentFrame();
        if (currentFrame) {
            int offsetX, offsetY;
            walkAnimation.getCurrentOffset(offsetX, offsetY);
            renderer.drawToCanvas(*currentFrame, 32 + offsetX, 134 + offsetY, true);
        }
        
        renderer.pushCanvasToDisplayOpaque(0, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "Character walk animation complete");
}

void RetroGameExample::paletteEffectExample(LGFX_ST7789P3_76x284* display) {
    ESP_LOGI(TAG, "=== Palette Effect Example ===");
    
    PaletteImageData faceImage(SAMPLE_FACE_16x16, 16, 16);
    PaletteImageRenderer renderer(display, 76, 284);
    
    for (int frame = 0; frame < 120; frame++) {
        renderer.clearCanvas(0x0000);
        
        // パレット色相を動的に変更
        RetroColorPalette dynamicPalette;
        for (int i = 1; i < 16; i++) {
            uint16_t hue = (frame * 3 + i * 24) % 360;
            dynamicPalette.setColor(i, RetroColorPalette::hsvToRgb565(hue, 80, 90));
        }
        
        // パレットを変更
        PaletteImageData coloredFace = faceImage;
        coloredFace.setPalette(dynamicPalette);
        
        renderer.drawToCanvas(coloredFace, 30, 134, true);
        renderer.pushCanvasToDisplayOpaque(0, 0);
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "Palette effect complete");
}