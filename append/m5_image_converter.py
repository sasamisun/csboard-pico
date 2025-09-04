#!/usr/bin/env python3
"""
M5Unified画像→Cヘッダー変換ツール
作者: Claude (にゃんこエンジニア)
目的: 画像ファイルをM5UnifiedのdrawRGBBitmap関数で使えるRGB565形式のCヘッダーファイルに変換

対応形式: PNG, JPEG, BMP, GIF, TIFF, WEBP など
出力形式: RGB565 (16bit) C配列 (.hファイル)
"""

import os
import sys
from PIL import Image, ImageDraw
import numpy as np
from pathlib import Path
import argparse
from typing import Tuple, Optional
import datetime

class M5ImageConverter:
    """
    M5Unified専用の画像変換クラス
    RGB565形式でのC配列生成に特化しているにゃ！
    """
    
    def __init__(self):
        self.supported_formats = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff', '.webp'}
    
    def rgb888_to_rgb565(self, r: int, g: int, b: int) -> int:
        """
        RGB888(24bit)からRGB565(16bit)への変換
        M5Unifiedで使用される標準的な色形式にゃ
        
        Args:
            r: Red値 (0-255)
            g: Green値 (0-255) 
            b: Blue値 (0-255)
            
        Returns:
            RGB565形式の16bit値
        """
        # RGB888 → RGB565変換（ビットシフトによる精度調整）
        r5 = (r >> 3) & 0x1F  # 8bit → 5bit (上位5bit使用)
        g6 = (g >> 2) & 0x3F  # 8bit → 6bit (上位6bit使用)
        b5 = (b >> 3) & 0x1F  # 8bit → 5bit (上位5bit使用)
        
        # RGB565フォーマット: RRRRRGGGGGGBBBBB
        return (r5 << 11) | (g6 << 5) | b5
    
    def process_image(self, input_path: str, max_width: Optional[int] = None, 
                     max_height: Optional[int] = None, 
                     dithering: bool = False) -> Tuple[np.ndarray, int, int]:
        """
        画像ファイルを読み込み、M5Unified用に前処理
        
        Args:
            input_path: 入力画像のパス
            max_width: 最大幅（Noneなら制限なし）
            max_height: 最大高さ（Noneなら制限なし）
            dithering: ディザリング処理の有効/無効
            
        Returns:
            (RGB565配列, 幅, 高さ)のタプル
        """
        try:
            # 画像を読み込み（透明度も考慮）
            with Image.open(input_path) as img:
                print(f"📷 元画像情報: {img.size[0]}x{img.size[1]} {img.mode}にゃ")
                
                # 透明度がある場合は白背景で合成
                if img.mode in ('RGBA', 'LA'):
                    background = Image.new('RGB', img.size, (255, 255, 255))
                    if img.mode == 'RGBA':
                        background.paste(img, mask=img.split()[3])  # アルファチャンネルをマスクに
                    else:  # LA mode
                        background.paste(img, mask=img.split()[1])
                    img = background
                
                # RGBモードに変換
                if img.mode != 'RGB':
                    img = img.convert('RGB')
                
                # サイズ制限がある場合はリサイズ（アスペクト比保持）
                if max_width or max_height:
                    img.thumbnail((max_width or img.width, max_height or img.height), 
                                Image.Resampling.LANCZOS)
                    print(f"🔄 リサイズ後: {img.size[0]}x{img.size[1]}にゃ")
                
                # ディザリング処理（より滑らかな減色）
                if dithering:
                    # Floyd-Steinbergディザリングで RGB565 相当の色数に減色
                    img = img.quantize(colors=65536, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.FLOYDSTEINBERG)
                    img = img.convert('RGB')
                    print("✨ ディザリング処理完了にゃ")
                
                # NumPy配列に変換
                img_array = np.array(img)
                height, width = img_array.shape[:2]
                
                # RGB565形式に変換
                rgb565_data = np.zeros((height, width), dtype=np.uint16)
                
                print("🔄 RGB565変換中...")
                for y in range(height):
                    for x in range(width):
                        r, g, b = img_array[y, x]
                        rgb565_data[y, x] = self.rgb888_to_rgb565(r, g, b)
                
                return rgb565_data.flatten(), width, height
                
        except Exception as e:
            raise Exception(f"画像処理エラー: {str(e)}")
    
    def generate_header_content(self, rgb565_data: np.ndarray, width: int, height: int,
                              array_name: str, input_filename: str, 
                              use_progmem: bool = True, 
                              bytes_per_line: int = 12) -> str:
        """
        Cヘッダーファイルのコンテンツを生成
        
        Args:
            rgb565_data: RGB565形式の画像データ
            width: 画像の幅
            height: 画像の高さ  
            array_name: C配列の名前
            input_filename: 元ファイル名
            use_progmem: PROGMEM使用フラグ（Arduinoのフラッシュメモリ格納用）
            bytes_per_line: 1行あたりの要素数
            
        Returns:
            Cヘッダーファイルの内容文字列
        """
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        total_pixels = len(rgb565_data)
        memory_size = total_pixels * 2  # 16bit = 2 bytes per pixel
        
        # ヘッダー部分
        header_content = f"""// M5Unified用 RGB565画像データ
// 自動生成日時: {timestamp}
// 元ファイル: {input_filename}
// 画像サイズ: {width}x{height} pixels
// データサイズ: {total_pixels} pixels ({memory_size:,} bytes)
// 形式: RGB565 (16-bit color)

#ifndef {array_name.upper()}_H
#define {array_name.upper()}_H

#include <Arduino.h>

// 画像の基本情報
#define {array_name.upper()}_WIDTH  {width}
#define {array_name.upper()}_HEIGHT {height}
#define {array_name.upper()}_SIZE   {total_pixels}

// M5Unified使用例:
// M5.Display.drawRGBBitmap(x, y, {array_name}, {array_name.upper()}_WIDTH, {array_name.upper()}_HEIGHT);

"""
        
        # PROGMEM指定（Arduinoでフラッシュメモリに格納）
        progmem_attr = "PROGMEM " if use_progmem else ""
        
        # 配列宣言
        header_content += f"const uint16_t {progmem_attr}{array_name}[{total_pixels}] = {{\n"
        
        # データ部分（見やすい形式で配置）
        print("📝 Cコード生成中...")
        for i in range(0, total_pixels, bytes_per_line):
            line_data = rgb565_data[i:i + bytes_per_line]
            hex_values = [f"0x{value:04X}" for value in line_data]
            
            # 最後の行でない場合はカンマを追加
            line_end = "," if i + bytes_per_line < total_pixels else ""
            header_content += f"  {', '.join(hex_values)}{line_end}\n"
        
        header_content += "};\n\n#endif // " + array_name.upper() + "_H\n"
        
        return header_content
    
    def convert_image(self, input_path: str, output_path: Optional[str] = None,
                     array_name: Optional[str] = None, max_width: Optional[int] = None,
                     max_height: Optional[int] = None, dithering: bool = False,
                     use_progmem: bool = True, bytes_per_line: int = 12) -> str:
        """
        画像ファイルをM5Unified用Cヘッダーファイルに変換
        
        Args:
            input_path: 入力画像ファイルのパス
            output_path: 出力.hファイルのパス（Noneなら自動生成）
            array_name: C配列名（Noneなら自動生成）
            max_width: 最大幅制限
            max_height: 最大高さ制限
            dithering: ディザリング処理
            use_progmem: PROGMEM使用
            bytes_per_line: 1行あたりの要素数
            
        Returns:
            出力ファイルのパス
        """
        # ファイル存在確認
        if not os.path.exists(input_path):
            raise FileNotFoundError(f"入力ファイルが見つかりません: {input_path}")
        
        input_path = Path(input_path)
        
        # 対応形式チェック
        if input_path.suffix.lower() not in self.supported_formats:
            raise ValueError(f"未対応の画像形式です: {input_path.suffix}")
        
        # 出力パス自動生成
        if output_path is None:
            output_path = input_path.with_suffix('.h')
        
        # 配列名自動生成（C言語の命名規則に準拠）
        if array_name is None:
            array_name = input_path.stem.lower().replace('-', '_').replace(' ', '_')
            # 数字で始まる場合は接頭辞を追加
            if array_name[0].isdigit():
                array_name = f"img_{array_name}"
        
        print(f"🚀 変換開始: {input_path.name} → {output_path}")
        
        # 画像処理
        rgb565_data, width, height = self.process_image(
            str(input_path), max_width, max_height, dithering
        )
        
        # ヘッダーコンテンツ生成
        header_content = self.generate_header_content(
            rgb565_data, width, height, array_name, input_path.name,
            use_progmem, bytes_per_line
        )
        
        # ファイル書き出し
        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(header_content)
            print(f"✅ 変換完了! → {output_path}")
            print(f"📊 配列名: {array_name}")
            print(f"📏 サイズ: {width}x{height}")
            print(f"💾 メモリ使用量: {len(rgb565_data) * 2:,} bytes")
            
            return str(output_path)
            
        except Exception as e:
            raise Exception(f"ファイル書き出しエラー: {str(e)}")

def main():
    """
    コマンドライン実行用のメイン関数
    """
    parser = argparse.ArgumentParser(
        description='M5Unified用画像→Cヘッダー変換ツール',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用例:
  python m5_image_converter.py image.png
  python m5_image_converter.py image.jpg -o custom.h -n my_image
  python m5_image_converter.py large.png --max-width 320 --max-height 240 --dithering
  python m5_image_converter.py icon.png --no-progmem --bytes-per-line 16
        """
    )
    
    parser.add_argument('input', help='入力画像ファイル')
    parser.add_argument('-o', '--output', help='出力.hファイル（未指定なら自動生成）')
    parser.add_argument('-n', '--name', help='C配列名（未指定なら自動生成）')
    parser.add_argument('--max-width', type=int, help='最大幅（ピクセル）')
    parser.add_argument('--max-height', type=int, help='最大高さ（ピクセル）')
    parser.add_argument('--dithering', action='store_true', 
                       help='ディザリング処理を有効化（画質向上）')
    parser.add_argument('--no-progmem', action='store_true',
                       help='PROGMEMを使用しない（RAM配置）')
    parser.add_argument('--bytes-per-line', type=int, default=12,
                       help='1行あたりのデータ数 (デフォルト: 12)')
    
    args = parser.parse_args()
    
    try:
        converter = M5ImageConverter()
        output_file = converter.convert_image(
            input_path=args.input,
            output_path=args.output,
            array_name=args.name,
            max_width=args.max_width,
            max_height=args.max_height,
            dithering=args.dithering,
            use_progmem=not args.no_progmem,
            bytes_per_line=args.bytes_per_line
        )
        
        print(f"\n🎉 変換成功！ {output_file} が生成されました")
        print("\n📖 使用方法:")
        array_name = args.name or Path(args.input).stem.lower().replace('-', '_').replace(' ', '_')
        if array_name[0].isdigit():
            array_name = f"img_{array_name}"
        
        print(f"""
#include "{Path(output_file).name}"

void setup() {{
  M5.begin();
  M5.Display.drawRGBBitmap(0, 0, {array_name}, {array_name.upper()}_WIDTH, {array_name.upper()}_HEIGHT);
}}
        """)
        
    except Exception as e:
        print(f"❌ エラー: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()