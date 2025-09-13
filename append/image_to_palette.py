#!/usr/bin/env python3
"""
画像16色パレット変換ツール for M5StampPico レトロゲームシステム（改良版）

新機能:
- max-size未指定時は入力画像の幅をそのまま使用
- 入力ファイル名から自動的に変数名を生成
- 画像の幅と高さ情報をヘッダーファイルに追加
- 自然な色合いを保つ色距離計算
- ディザリング対応
- M5StampPico用データ配列生成
- 複数のカラーパレット対応

使用例:
python improved_palette_tool.py cat.jpg --palette classic --dither --output result
→ 変数名: cat_data, cat_width, cat_height が自動生成される
"""

import numpy as np
from PIL import Image, ImageDraw
import argparse
import os
import json
import re
from typing import List, Tuple, Dict, Optional
import colorsys
import math


def sanitize_variable_name(filename: str) -> str:
    """ファイル名を有効なC言語変数名に変換"""
    # 拡張子を除去
    name = os.path.splitext(os.path.basename(filename))[0]
    
    # 無効な文字を除去・変換
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    
    # 先頭が数字の場合はアンダースコアを追加
    if name and name[0].isdigit():
        name = '_' + name
    
    # 空文字列や無効な場合はデフォルト名
    if not name or name == '_':
        name = 'image'
    
    return name


class ColorPalette:
    """16色カラーパレット管理クラス"""
    
    def __init__(self, name: str = "classic"):
        self.name = name
        self.colors_rgb = []  # RGB888形式 [(r,g,b), ...]
        self.colors_rgb565 = []  # RGB565形式 [0x1234, ...]
        self.transparent_index = 0
        
        self._load_palette(name)
    
    def _load_palette(self, name: str):
        """パレットを読み込み"""
        palettes = {
            "classic": [
                (0, 0, 0),        # 0: 透明色（黒）
                (255, 255, 255),  # 1: 白
                (248, 0, 0),      # 2: 赤
                (0, 248, 0),      # 3: 緑
                (0, 0, 248),      # 4: 青
                (248, 248, 0),    # 5: 黄
                (248, 0, 248),    # 6: マゼンタ
                (0, 248, 248),    # 7: シアン
                (132, 132, 132),  # 8: グレー
                (252, 100, 0),    # 9: オレンジ
                (128, 0, 0),      # 10: ダークレッド
                (0, 100, 0),      # 11: ダークグリーン
                (0, 0, 128),      # 12: ダークブルー
                (132, 100, 0),    # 13: ブラウン
                (66, 66, 66),     # 14: ダークグレー
                (33, 33, 33),     # 15: ベリーダーク
            ],
            "gameboy": [
                (0, 0, 0),        # 0: 透明色
                (155, 188, 15),   # 1: ライトグリーン
                (139, 172, 15),   # 2: 
                (123, 156, 15),   # 3:
                (107, 140, 15),   # 4:
                (91, 124, 15),    # 5:
                (75, 108, 15),    # 6:
                (59, 92, 15),     # 7:
                (43, 76, 15),     # 8:
                (27, 60, 15),     # 9:
                (15, 56, 15),     # 10: ダークグリーン
                (0, 40, 0),       # 11:
                (0, 32, 0),       # 12:
                (0, 24, 0),       # 13:
                (0, 16, 0),       # 14:
                (0, 8, 0),        # 15:
            ],
            "sepia": [
                (0, 0, 0),        # 0: 透明色
                (255, 255, 255),  # 1: セピア白
                (240, 220, 180),  # 2:
                (220, 200, 160),  # 3:
                (200, 180, 140),  # 4:
                (180, 160, 120),  # 5:
                (160, 140, 100),  # 6:
                (140, 120, 80),   # 7:
                (120, 100, 60),   # 8:
                (100, 80, 40),    # 9:
                (80, 60, 20),     # 10:
                (70, 50, 15),     # 11:
                (60, 40, 10),     # 12:
                (50, 30, 5),      # 13:
                (40, 20, 0),      # 14:
                (20, 10, 0),      # 15: セピア黒
            ],
            "neon": [
                (0, 0, 0),        # 0: 透明色
                (255, 255, 255),  # 1: 白
                (255, 0, 255),    # 2: ネオンピンク
                (0, 255, 255),    # 3: ネオンシアン
                (255, 255, 0),    # 4: ネオンイエロー
                (255, 128, 0),    # 5: ネオンオレンジ
                (128, 255, 0),    # 6: ネオングリーン
                (0, 255, 128),    # 7: 
                (0, 128, 255),    # 8: ネオンブルー
                (128, 0, 255),    # 9: ネオンパープル
                (255, 0, 128),    # 10:
                (192, 192, 192),  # 11: シルバー
                (128, 128, 128),  # 12: グレー
                (64, 64, 64),     # 13: ダークグレー
                (32, 32, 32),     # 14:
                (16, 16, 16),     # 15:
            ]
        }
        
        if name not in palettes:
            raise ValueError(f"Unknown palette: {name}. Available: {list(palettes.keys())}")
        
        self.colors_rgb = palettes[name]
        self._convert_to_rgb565()
    
    def _convert_to_rgb565(self):
        """RGB888をRGB565に変換"""
        self.colors_rgb565 = []
        for r, g, b in self.colors_rgb:
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            self.colors_rgb565.append(rgb565)
    
    def add_custom_color(self, index: int, r: int, g: int, b: int):
        """カスタム色を追加"""
        if 0 <= index < 16:
            self.colors_rgb[index] = (r, g, b)
            self._convert_to_rgb565()
    
    def save_palette_image(self, output_path: str, cell_size: int = 32):
        """パレットの見本画像を生成"""
        img = Image.new('RGB', (cell_size * 4, cell_size * 4))
        draw = ImageDraw.Draw(img)
        
        for i, (r, g, b) in enumerate(self.colors_rgb):
            x = (i % 4) * cell_size
            y = (i // 4) * cell_size
            
            # 透明色の場合は市松模様
            if i == self.transparent_index:
                for py in range(cell_size):
                    for px in range(cell_size):
                        if (px // 4 + py // 4) % 2:
                            color = (200, 200, 200)
                        else:
                            color = (100, 100, 100)
                        draw.point((x + px, y + py), color)
            else:
                draw.rectangle([x, y, x + cell_size - 1, y + cell_size - 1], fill=(r, g, b))
            
            # インデックス番号を描画
            draw.text((x + 2, y + 2), str(i), fill=(255, 255, 255) if sum([r, g, b]) < 384 else (0, 0, 0))
        
        img.save(output_path)
        print(f"📄 Palette preview saved: {output_path}")


class ColorQuantizer:
    """色量子化・減色クラス"""
    
    def __init__(self, palette: ColorPalette, color_space: str = "lab"):
        self.palette = palette
        self.color_space = color_space.lower()
        
        # パレット色をLAB色空間に変換（より正確な色距離計算のため）
        if self.color_space == "lab":
            self.palette_lab = [self._rgb_to_lab(r, g, b) for r, g, b in palette.colors_rgb]
    
    def _rgb_to_lab(self, r: int, g: int, b: int) -> Tuple[float, float, float]:
        """RGB to LAB conversion"""
        # RGB to XYZ
        r, g, b = r / 255.0, g / 255.0, b / 255.0
        
        def gamma_correct(c):
            return ((c + 0.055) / 1.055) ** 2.4 if c > 0.04045 else c / 12.92
        
        r, g, b = gamma_correct(r), gamma_correct(g), gamma_correct(b)
        
        # XYZ using sRGB matrix
        x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375
        y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750
        z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041
        
        # XYZ to LAB
        def f(t):
            return t ** (1/3) if t > 0.008856 else (7.787 * t + 16/116)
        
        # D65 白色点で正規化
        x, y, z = x / 0.95047, y / 1.00000, z / 1.08883
        
        fx, fy, fz = f(x), f(y), f(z)
        
        L = 116 * fy - 16
        a = 500 * (fx - fy)
        b = 200 * (fy - fz)
        
        return (L, a, b)
    
    def _color_distance(self, color1: Tuple, color2: Tuple) -> float:
        """色距離を計算"""
        if self.color_space == "lab":
            # ΔE CIE76 距離
            dL, da, db = color1[0] - color2[0], color1[1] - color2[1], color1[2] - color2[2]
            return math.sqrt(dL*dL + da*da + db*db)
        elif self.color_space == "rgb":
            # RGB ユークリッド距離
            dr, dg, db = color1[0] - color2[0], color1[1] - color2[1], color1[2] - color2[2]
            return math.sqrt(dr*dr + dg*dg + db*db)
        else:
            # HSV距離
            h1, s1, v1 = colorsys.rgb_to_hsv(color1[0]/255, color1[1]/255, color1[2]/255)
            h2, s2, v2 = colorsys.rgb_to_hsv(color2[0]/255, color2[1]/255, color2[2]/255)
            
            dh = min(abs(h1 - h2), 1 - abs(h1 - h2))  # 色相は円形
            ds, dv = s1 - s2, v1 - v2
            return math.sqrt(dh*dh + ds*ds + dv*dv)
    
    def find_closest_color(self, r: int, g: int, b: int) -> int:
        """最も近いパレット色のインデックスを取得"""
        if self.color_space == "lab":
            target_lab = self._rgb_to_lab(r, g, b)
            distances = [self._color_distance(target_lab, pal_lab) for pal_lab in self.palette_lab]
        else:
            target = (r, g, b)
            distances = [self._color_distance(target, pal_rgb) for pal_rgb in self.palette.colors_rgb]
        
        return distances.index(min(distances))
    
    def quantize_image(self, image: Image.Image, dither: bool = False) -> Image.Image:
        """画像を16色に減色"""
        # RGBA対応
        if image.mode == 'RGBA':
            # 透明部分を透明色（インデックス0）に変換
            image_array = np.array(image)
            alpha_channel = image_array[:, :, 3]
            
            # RGB部分を変換
            rgb_image = Image.fromarray(image_array[:, :, :3])
            
            if dither:
                # Floyd-Steinbergディザリング（透明度考慮版）
                result = self._floyd_steinberg_dither_with_alpha(rgb_image, alpha_channel)
            else:
                # 単純な最近傍変換（透明度考慮版）
                result = self._quantize_with_alpha(rgb_image, alpha_channel)
        else:
            # RGB画像の処理
            if dither:
                result = self._floyd_steinberg_dither(image)
            else:
                result = self._simple_quantize(image)
        
        return result
    
    def _quantize_with_alpha(self, image: Image.Image, alpha_channel: np.ndarray) -> Image.Image:
        """透明度を考慮したシンプルな量子化"""
        width, height = image.size
        image_array = np.array(image)
        result_array = np.zeros((height, width), dtype=np.uint8)
        
        for y in range(height):
            for x in range(width):
                if alpha_channel[y, x] < 128:  # 透明
                    result_array[y, x] = self.palette.transparent_index
                else:
                    r, g, b = image_array[y, x]
                    closest_idx = self.find_closest_color(r, g, b)
                    result_array[y, x] = closest_idx
        
        # パレット画像として結果を作成
        result = Image.fromarray(result_array, mode='P')
        palette_data = []
        for r, g, b in self.palette.colors_rgb:
            palette_data.extend([r, g, b])
        result.putpalette(palette_data)
        
        return result
    
    def _simple_quantize(self, image: Image.Image) -> Image.Image:
        """シンプルな最近傍量子化"""
        width, height = image.size
        image_array = np.array(image.convert('RGB'))
        result_array = np.zeros((height, width), dtype=np.uint8)
        
        # パレット画像用データを作成
        palette_data = []
        for r, g, b in self.palette.colors_rgb:
            palette_data.extend([r, g, b])
        
        for y in range(height):
            for x in range(width):
                r, g, b = image_array[y, x]
                closest_idx = self.find_closest_color(r, g, b)
                result_array[y, x] = closest_idx
        
        result = Image.fromarray(result_array, mode='P')
        result.putpalette(palette_data)
        
        return result
    
    def _floyd_steinberg_dither(self, image: Image.Image) -> Image.Image:
        """Floyd-Steinbergディザリング"""
        width, height = image.size
        image_array = np.array(image.convert('RGB'), dtype=np.float32)
        result_array = np.zeros((height, width), dtype=np.uint8)
        
        # パレット画像用データを作成
        palette_data = []
        for r, g, b in self.palette.colors_rgb:
            palette_data.extend([r, g, b])
        
        for y in range(height):
            for x in range(width):
                old_pixel = image_array[y, x]
                r, g, b = old_pixel.astype(int)
                
                # クランプ処理
                r = max(0, min(255, r))
                g = max(0, min(255, g))
                b = max(0, min(255, b))
                
                closest_idx = self.find_closest_color(r, g, b)
                result_array[y, x] = closest_idx
                
                new_pixel = np.array(self.palette.colors_rgb[closest_idx], dtype=np.float32)
                quant_error = old_pixel - new_pixel
                
                # エラー拡散
                if x < width - 1:
                    image_array[y, x + 1] += quant_error * 7/16
                if y < height - 1:
                    if x > 0:
                        image_array[y + 1, x - 1] += quant_error * 3/16
                    image_array[y + 1, x] += quant_error * 5/16
                    if x < width - 1:
                        image_array[y + 1, x + 1] += quant_error * 1/16
        
        result = Image.fromarray(result_array, mode='P')
        result.putpalette(palette_data)
        
        return result
    
    def _floyd_steinberg_dither_with_alpha(self, image: Image.Image, alpha_channel: np.ndarray) -> Image.Image:
        """透明度対応Floyd-Steinbergディザリング"""
        width, height = image.size
        image_array = np.array(image, dtype=np.float32)
        result_array = np.zeros((height, width), dtype=np.uint8)
        
        # パレット画像用データを作成
        palette_data = []
        for r, g, b in self.palette.colors_rgb:
            palette_data.extend([r, g, b])
        
        for y in range(height):
            for x in range(width):
                if alpha_channel[y, x] < 128:  # 透明
                    result_array[y, x] = self.palette.transparent_index
                    continue
                
                old_pixel = image_array[y, x]
                r, g, b = old_pixel.astype(int)
                
                # クランプ処理
                r = max(0, min(255, r))
                g = max(0, min(255, g))
                b = max(0, min(255, b))
                
                closest_idx = self.find_closest_color(r, g, b)
                result_array[y, x] = closest_idx
                
                new_pixel = np.array(self.palette.colors_rgb[closest_idx], dtype=np.float32)
                quant_error = old_pixel - new_pixel
                
                # エラー拡散（透明ピクセルには拡散しない）
                if x < width - 1 and alpha_channel[y, x + 1] >= 128:
                    image_array[y, x + 1] += quant_error * 7/16
                if y < height - 1:
                    if x > 0 and alpha_channel[y + 1, x - 1] >= 128:
                        image_array[y + 1, x - 1] += quant_error * 3/16
                    if alpha_channel[y + 1, x] >= 128:
                        image_array[y + 1, x] += quant_error * 5/16
                    if x < width - 1 and alpha_channel[y + 1, x + 1] >= 128:
                        image_array[y + 1, x + 1] += quant_error * 1/16
        
        result = Image.fromarray(result_array, mode='P')
        result.putpalette(palette_data)
        
        return result


class M5DataGenerator:
    """M5StampPico用データ生成クラス（改良版）"""
    
    @staticmethod
    def generate_data_array(quantized_image: Image.Image, base_var_name: str) -> str:
        """1バイト2ピクセル形式のC配列を生成（改良版）"""
        width, height = quantized_image.size
        image_array = np.array(quantized_image)
        
        # 1バイトに2ピクセル格納
        data_bytes = []
        for y in range(height):
            for x in range(0, width, 2):
                pixel1 = image_array[y, x] if x < width else 0
                pixel2 = image_array[y, x + 1] if x + 1 < width else 0
                
                # 下位4bit: 偶数ピクセル, 上位4bit: 奇数ピクセル
                byte_value = (pixel2 << 4) | (pixel1 & 0x0F)
                data_bytes.append(byte_value)
        
        # 変数名を生成
        data_var = f"{base_var_name}_data"
        width_var = f"{base_var_name}_width"
        height_var = f"{base_var_name}_height"
        
        # C言語配列形式で出力
        lines = [f"// Image: {width}x{height} pixels, 16-color palette"]
        lines.append(f"// Generated data size: {len(data_bytes)} bytes")
        lines.append(f"// Memory efficiency: {((width * height * 2 - len(data_bytes)) / (width * height * 2) * 100):.1f}% saving vs 16-bit")
        lines.append("")
        
        # 画像サイズ定数
        lines.append(f"// 画像サイズ情報")
        lines.append(f"const uint16_t {width_var} = {width};")
        lines.append(f"const uint16_t {height_var} = {height};")
        lines.append("")
        
        # 画像データ配列
        lines.append(f"// 画像データ配列（1バイトに2ピクセル格納）")
        lines.append(f"const uint8_t {data_var}[{len(data_bytes)}] = {{")
        
        # 16バイトずつ改行して見やすく
        for i in range(0, len(data_bytes), 16):
            chunk = data_bytes[i:i+16]
            hex_values = [f"0x{b:02X}" for b in chunk]
            line = "    " + ", ".join(hex_values)
            if i + 16 < len(data_bytes):
                line += ","
            lines.append(line)
        
        lines.append("};")
        lines.append("")
        
        # 使用例とマクロ定義
        lines.append(f"// 便利なマクロ定義")
        lines.append(f"#define {base_var_name.upper()}_WIDTH  {width}")
        lines.append(f"#define {base_var_name.upper()}_HEIGHT {height}")
        lines.append(f"#define {base_var_name.upper()}_SIZE   {len(data_bytes)}")
        lines.append("")
        
        lines.append(f"// 使用例:")
        lines.append(f"// PaletteImageData myImage({data_var}, {width_var}, {height_var});")
        lines.append(f"// または")
        lines.append(f"// PaletteImageData myImage({data_var}, {base_var_name.upper()}_WIDTH, {base_var_name.upper()}_HEIGHT);")
        lines.append(f"// renderer.drawToCanvas(myImage, x, y, true);")
        
        return "\n".join(lines)
    
    @staticmethod
    def generate_palette_code(palette: ColorPalette, base_var_name: str) -> str:
        """パレット定義のC++コードを生成（改良版）"""
        palette_var = f"{base_var_name}_palette"
        init_func = f"{base_var_name}_palette_init"
        
        lines = [f"// {palette.name.capitalize()} カラーパレット定義"]
        lines.append(f"// パレット初期化関数")
        lines.append(f"void {init_func}(RetroColorPalette& palette) {{")
        
        for i, (r, g, b) in enumerate(palette.colors_rgb):
            rgb565 = palette.colors_rgb565[i]
            if i == 0:
                comment = "// 透明色"
            else:
                comment = f"// RGB({r},{g},{b})"
            lines.append(f"    palette.setColor({i}, 0x{rgb565:04X}); {comment}")
        
        lines.append("}")
        lines.append("")
        
        # パレット配列も生成
        lines.append(f"// パレット配列定義（RGB565形式）")
        lines.append(f"const uint16_t {palette_var}[16] = {{")
        for i in range(0, 16, 4):
            chunk = palette.colors_rgb565[i:i+4]
            hex_values = [f"0x{c:04X}" for c in chunk]
            line = "    " + ", ".join(hex_values)
            if i + 4 < 16:
                line += ","
            
            # コメント追加
            comments = []
            for j, idx in enumerate(range(i, min(i+4, 16))):
                r, g, b = palette.colors_rgb[idx]
                if idx == 0:
                    comments.append("透明")
                else:
                    comments.append(f"RGB({r},{g},{b})")
            line += "  // " + ", ".join(comments)
            
            lines.append(line)
        
        lines.append("};")
        
        return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Convert images to 16-color palette for M5StampPico (Improved)")
    parser.add_argument("input", help="Input image file")
    parser.add_argument("--palette", choices=["classic", "gameboy", "sepia", "neon"], 
                       default="classic", help="Color palette to use")
    parser.add_argument("--dither", action="store_true", help="Apply Floyd-Steinberg dithering")
    parser.add_argument("--color-space", choices=["rgb", "lab", "hsv"], default="lab",
                       help="Color space for distance calculation")
    parser.add_argument("--output", default="", help="Output filename prefix (default: auto from input)")
    parser.add_argument("--var-name", default="", help="C array variable name (default: auto from filename)")
    parser.add_argument("--max-size", type=int, default=None, 
                       help="Maximum width/height (will resize). If not specified, use original size")
    parser.add_argument("--preview", action="store_true", help="Generate palette preview")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"❌ Error: Input file '{args.input}' not found")
        return 1
    
    # 自動的に出力名と変数名を生成
    base_name = sanitize_variable_name(args.input)
    output_prefix = args.output if args.output else base_name
    var_name = args.var_name if args.var_name else base_name
    
    print(f"🎨 Loading image: {args.input}")
    print(f"🎨 Using palette: {args.palette}")
    print(f"🎨 Color space: {args.color_space}")
    print(f"🎨 Dithering: {'ON' if args.dither else 'OFF'}")
    print(f"🏷️  Variable name: {var_name}")
    
    # 画像読み込み
    try:
        image = Image.open(args.input)
        print(f"📏 Original size: {image.size}")
        
        # サイズ調整（改良版：max-size指定時のみリサイズ）
        if args.max_size is not None:
            if max(image.size) > args.max_size:
                ratio = args.max_size / max(image.size)
                new_size = (int(image.size[0] * ratio), int(image.size[1] * ratio))
                image = image.resize(new_size, Image.Resampling.LANCZOS)
                print(f"📏 Resized to: {image.size}")
            else:
                print(f"📏 Size within limit, no resize needed")
        else:
            print(f"📏 No max-size specified, using original dimensions")
        
    except Exception as e:
        print(f"❌ Error loading image: {e}")
        return 1
    
    # パレット初期化
    try:
        palette = ColorPalette(args.palette)
        quantizer = ColorQuantizer(palette, args.color_space)
    except Exception as e:
        print(f"❌ Error initializing palette: {e}")
        return 1
    
    # パレットプレビュー生成
    if args.preview:
        palette_path = f"{output_prefix}_palette.png"
        palette.save_palette_image(palette_path)
    
    # 画像を減色
    print("🔄 Converting to 16-color palette...")
    try:
        quantized = quantizer.quantize_image(image, args.dither)
    except Exception as e:
        print(f"❌ Error during quantization: {e}")
        return 1
    
    # BMP保存
    bmp_path = f"{output_prefix}.bmp"
    quantized.save(bmp_path)
    print(f"💾 Saved BMP: {bmp_path}")
    
    # RGB表示用画像も保存
    rgb_image = quantized.convert('RGB')
    rgb_path = f"{output_prefix}_preview.png"
    rgb_image.save(rgb_path)
    print(f"💾 Saved preview: {rgb_path}")
    
    # M5StampPico用データ生成（改良版）
    print("🔢 Generating M5StampPico data...")
    try:
        data_code = M5DataGenerator.generate_data_array(quantized, var_name)
        palette_code = M5DataGenerator.generate_palette_code(palette, var_name)
    except Exception as e:
        print(f"❌ Error generating code: {e}")
        return 1
    
    # Cヘッダーファイル生成（改良版）
    header_path = f"{output_prefix}.h"
    try:
        with open(header_path, "w", encoding='utf-8') as f:
            f.write(f"/*\n * Auto-generated from {args.input}\n")
            f.write(f" * Original size: {image.size[0]}x{image.size[1]}\n")
            f.write(f" * Final size: {quantized.size[0]}x{quantized.size[1]}\n")
            f.write(f" * Palette: {args.palette}\n")
            f.write(f" * Dithering: {'ON' if args.dither else 'OFF'}\n")
            f.write(f" * Color space: {args.color_space}\n")
            f.write(f" * Variable name: {var_name}\n")
            f.write(" * \n")
            f.write(" * M5StampPico 16-Color Palette Image Tool (Improved Version)\n")
            f.write(" */\n\n")
            f.write("#pragma once\n")
            f.write('#include "RetroGamePaletteImage.hpp"\n\n')
            f.write(data_code)
            f.write("\n\n")
            f.write(palette_code)
        
        print(f"💾 Saved C header: {header_path}")
    except Exception as e:
        print(f"❌ Error writing header file: {e}")
        return 1
    
    # 統計情報
    final_size = quantized.size
    data_size = (final_size[0] * final_size[1] + 1) // 2
    original_16bit_size = final_size[0] * final_size[1] * 2
    saving = ((original_16bit_size - data_size) / original_16bit_size) * 100
    
    print("\n📊 Conversion Summary:")
    print(f"   Input file: {args.input}")
    print(f"   Original size: {image.size}")
    print(f"   Final size: {final_size}")
    print(f"   Variable names: {var_name}_data, {var_name}_width, {var_name}_height")
    print(f"   Palette: {args.palette} ({len(palette.colors_rgb)} colors)")
    print(f"   Data size: {data_size} bytes")
    print(f"   Memory saving: {saving:.1f}% vs 16-bit RGB565")
    print(f"   Files generated:")
    print(f"     - {bmp_path} (BMP image)")
    print(f"     - {rgb_path} (Preview)")
    print(f"     - {header_path} (C header with size info)")
    if args.preview:
        print(f"     - {output_prefix}_palette.png (Palette preview)")
    
    print("\n🎮 Ready for M5StampPico!")
    print(f"   #include \"{header_path}\"")
    print(f"   PaletteImageData img({var_name}_data, {var_name}_width, {var_name}_height);")
    print(f"   // または")
    print(f"   PaletteImageData img({var_name}_data, {var_name.upper()}_WIDTH, {var_name.upper()}_HEIGHT);")
    
    return 0


if __name__ == "__main__":
    exit(main())