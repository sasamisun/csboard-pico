#!/usr/bin/env python3
"""
画像をCヘッダーファイル変換ツール - Image to C Header Converter
画像ファイルを1ビット1ピクセルのC言語配列に変換するプログラム

使用例:
python image_to_c_header.py image1.png image2.bmp output.h
python image_to_c_header.py *.png -o display_data.h

# 必要なライブラリをインストール
pip install Pillow

# カットした画像をまとめてCヘッダーに変換
python image_to_c_header.py nekonoba2025_cut_*.bmp -o display_images.h

# 閾値を調整して変換（デフォルトは128）
python image_to_c_header.py image1.png image2.png -o screen_data.h --threshold 100

# 白黒を反転（白=1、黒=0にしたい場合）
python image_to_c_header.py *.bmp -o inverted_data.h --invert

"""

import sys
import argparse
import re
import glob
from pathlib import Path
from PIL import Image
import math


def parse_arguments():
    """
    コマンドライン引数を解析する関数
    画像ファイルパスと出力ファイルパスを返す
    """
    parser = argparse.ArgumentParser(
        description='画像ファイルを1ビット1ピクセルのC言語ヘッダーファイルに変換します',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用例:
  python %(prog)s image1.png image2.bmp -o output.h
  python %(prog)s "*.png" --output display_data.h
  python %(prog)s "cut_*.bmp" -o screen_data.h --threshold 128
  python %(prog)s nekonoba2025_cut_*.bmp -o display_images.h
        """
    )
    
    # 画像ファイルの引数（複数可能）
    parser.add_argument('images', 
                       nargs='+',
                       help='変換する画像ファイルのパス（複数指定可能）')
    
    # 出力ファイルの指定
    parser.add_argument('-o', '--output',
                       required=True,
                       help='出力するCヘッダーファイルのパス')
    
    # 2値化の閾値
    parser.add_argument('--threshold', '-t',
                       type=int,
                       default=128,
                       help='2値化の閾値（0-255、デフォルト: 128）')
    
    # 白黒の反転
    parser.add_argument('--invert',
                       action='store_true',
                       help='白と黒を反転する（白=1、黒=0）')
    
    return parser.parse_args()


def expand_wildcards(image_patterns):
    """
    ワイルドカードを含むパターンを実際のファイルパスに展開する関数
    
    Args:
        image_patterns (list): ワイルドカードを含む可能性があるパターンのリスト
        
    Returns:
        list: 展開されたファイルパスのリスト
    """
    expanded_files = []
    
    for pattern in image_patterns:
        # ワイルドカードが含まれているかチェック
        if '*' in pattern or '?' in pattern or '[' in pattern:
            print(f"🔍 ワイルドカード展開: {pattern}")
            
            # globでファイルを検索
            matched_files = glob.glob(pattern)
            
            if matched_files:
                print(f"  📁 見つかったファイル: {len(matched_files)}個")
                for file in sorted(matched_files):
                    print(f"    📄 {file}")
                expanded_files.extend(sorted(matched_files))
            else:
                print(f"  ⚠️  該当するファイルが見つかりません: {pattern}")
        else:
            # 普通のファイルパス
            expanded_files.append(pattern)
    
    return expanded_files


def validate_threshold(threshold):
    """
    2値化閾値の妥当性をチェックする関数
    
    Args:
        threshold (int): 閾値
        
    Raises:
        ValueError: 閾値が無効な場合
    """
    if not 0 <= threshold <= 255:
        raise ValueError(f"閾値は0-255の範囲で指定してください: {threshold}")


def sanitize_variable_name(filename):
    """
    ファイル名をC言語の変数名として有効な形式に変換する関数
    
    Args:
        filename (str): 元のファイル名
        
    Returns:
        str: C言語で有効な変数名
    """
    # 拡張子を除去
    name = Path(filename).stem
    
    # 英数字とアンダースコア以外を削除
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    
    # 先頭が数字の場合は'img_'を追加
    if name and name[0].isdigit():
        name = 'img_' + name
    
    # 空文字列やアンダースコアのみの場合はデフォルト名を使用
    if not name or name.replace('_', '') == '':
        name = 'image_data'
    
    return name.lower()


def convert_image_to_1bit_array(image_path, threshold=128, invert=False):
    """
    画像を1ビット1ピクセルのバイト配列に変換する関数
    
    Args:
        image_path (Path): 画像ファイルのパス
        threshold (int): 2値化の閾値
        invert (bool): 白黒を反転するかどうか
        
    Returns:
        tuple: (width, height, byte_array)
    """
    try:
        # 画像を開く
        with Image.open(image_path) as img:
            # グレースケールに変換
            gray_img = img.convert('L')
            width, height = gray_img.size
            
            print(f"  📏 サイズ: {width}x{height} ピクセル")
            
            # 2値化処理（閾値より大きい場合は白=255、小さい場合は黒=0）
            binary_img = gray_img.point(lambda x: 255 if x > threshold else 0, mode='1')
            
            # ピクセルデータを取得
            pixels = list(binary_img.getdata())
            
            # 1ビット1ピクセル形式でパッキング
            # 8ピクセルを1バイトにまとめる
            byte_array = []
            
            for y in range(height):
                for x in range(0, width, 8):  # 8ピクセルずつ処理
                    byte_value = 0
                    
                    # 8ビット分のピクセルを処理
                    for bit_pos in range(8):
                        pixel_x = x + bit_pos
                        
                        if pixel_x < width:  # 画像の幅を超えない場合のみ
                            pixel_index = y * width + pixel_x
                            pixel_value = pixels[pixel_index]
                            
                            # 白=True, 黒=False（PILの'1'モードでは255=白, 0=黒）
                            is_white = (pixel_value == 255)
                            
                            # 反転オプションが有効な場合
                            if invert:
                                bit_value = 1 if is_white else 0
                            else:
                                bit_value = 0 if is_white else 1  # 通常は黒=1
                            
                            # MSB側から設定（左のピクセルが上位ビット）
                            if bit_value:
                                byte_value |= (1 << (7 - bit_pos))
                    
                    byte_array.append(byte_value)
            
            print(f"  📦 変換結果: {len(byte_array)} バイト ({len(byte_array)*8} ビット)")
            return width, height, byte_array
            
    except Exception as e:
        raise RuntimeError(f"画像変換中にエラーが発生しました: {e}")


def generate_c_header_content(image_data_list, invert=False):
    """
    C言語ヘッダーファイルの内容を生成する関数
    
    Args:
        image_data_list (list): 画像データのリスト
        invert (bool): 反転モードかどうか
        
    Returns:
        str: ヘッダーファイルの内容
    """
    header_content = []
    
    # ヘッダーガード開始
    header_content.append("#ifndef IMAGE_DATA_H")
    header_content.append("#define IMAGE_DATA_H")
    header_content.append("")
    header_content.append("#include <stdint.h>")
    header_content.append("")
    
    # 色についての説明コメント
    bit_meaning = "1=白, 0=黒" if invert else "1=黒, 0=白"
    header_content.append(f"// 1ビット1ピクセル形式のモノクロ画像データ")
    header_content.append(f"// ビット値: {bit_meaning}")
    header_content.append(f"// バイト内のビット順序: MSB（左のピクセル）→ LSB（右のピクセル）")
    header_content.append("")
    
    # 各画像のデータを生成
    for image_info in image_data_list:
        var_name = image_info['var_name']
        width = image_info['width']
        height = image_info['height']
        byte_array = image_info['byte_array']
        original_file = image_info['original_file']
        
        # 画像情報のコメント
        header_content.append(f"// 画像: {original_file}")
        header_content.append(f"// サイズ: {width}x{height} ピクセル")
        header_content.append(f"// データサイズ: {len(byte_array)} バイト")
        header_content.append("")
        
        # 幅と高さの定数定義
        width_const = f"{var_name.upper()}_WIDTH"
        height_const = f"{var_name.upper()}_HEIGHT"
        
        header_content.append(f"#define {width_const}  {width}")
        header_content.append(f"#define {height_const} {height}")
        header_content.append("")
        
        # 画像データ配列の定義
        header_content.append(f"static const uint8_t {var_name}[] = {{")
        
        # バイトデータを16進数で整形出力（16バイトずつ改行）
        for i in range(0, len(byte_array), 16):
            line_bytes = byte_array[i:i+16]
            hex_values = [f"0x{byte:02X}" for byte in line_bytes]
            line = "    " + ", ".join(hex_values)
            
            # 最後の行でない場合はカンマを追加
            if i + 16 < len(byte_array):
                line += ","
            
            # 行コメントを追加（何行目のデータか）
            start_row = i // math.ceil(width / 8)
            end_row = min(start_row + (16 // math.ceil(width / 8)), height - 1)
            if start_row == end_row:
                line += f"  // Row {start_row}"
            else:
                line += f"  // Rows {start_row}-{end_row}"
            
            header_content.append(line)
        
        header_content.append("};")
        header_content.append("")
    
    # 画像数の定数
    if len(image_data_list) > 1:
        header_content.append(f"#define IMAGE_COUNT {len(image_data_list)}")
        header_content.append("")
    
    # ヘッダーガード終了
    header_content.append("#endif // IMAGE_DATA_H")
    header_content.append("")
    
    return "\n".join(header_content)


def process_images(image_paths, threshold=128, invert=False):
    """
    複数の画像ファイルを処理する関数
    
    Args:
        image_paths (list): 画像ファイルパスのリスト
        threshold (int): 2値化の閾値
        invert (bool): 白黒反転フラグ
        
    Returns:
        list: 処理された画像データのリスト
    """
    image_data_list = []
    
    for image_path in image_paths:
        path = Path(image_path)
        
        # ファイルの存在確認
        if not path.exists():
            print(f"⚠️  警告: ファイルが見つかりません: {image_path}")
            continue
        
        print(f"🖼️  処理中: {image_path}")
        
        try:
            # 画像を変換
            width, height, byte_array = convert_image_to_1bit_array(
                path, threshold, invert
            )
            
            # 変数名を生成
            var_name = sanitize_variable_name(path.name)
            
            # 重複チェック（同じ変数名がある場合は番号を追加）
            original_var_name = var_name
            counter = 1
            existing_names = [item['var_name'] for item in image_data_list]
            while var_name in existing_names:
                var_name = f"{original_var_name}_{counter}"
                counter += 1
            
            image_data_list.append({
                'var_name': var_name,
                'width': width,
                'height': height,
                'byte_array': byte_array,
                'original_file': path.name
            })
            
            print(f"  ✅ 完了: 変数名 '{var_name}'")
            
        except Exception as e:
            print(f"  ❌ エラー: {e}")
            continue
    
    return image_data_list


def main():
    """
    メイン処理関数
    コマンドライン引数の解析から処理まで一連の流れを管理
    """
    try:
        # コマンドライン引数を解析
        args = parse_arguments()
        
        # 閾値の検証
        validate_threshold(args.threshold)
        
        # 出力ファイルパスの設定
        output_path = Path(args.output)
        
        print("=" * 60)
        print("🎨 画像→Cヘッダーファイル変換ツール開始！")
        print("=" * 60)
        print(f"⚙️  設定:")
        print(f"  📊 2値化閾値: {args.threshold}")
        print(f"  🔄 白黒反転: {'有効' if args.invert else '無効'}")
        print(f"  💾 出力ファイル: {output_path}")
        print("")
        
        # ワイルドカードを展開してファイルリストを取得
        expanded_images = expand_wildcards(args.images)
        
        if not expanded_images:
            print("❌ 処理対象の画像ファイルが見つかりませんでした。")
            sys.exit(1)
        
        print("")
        
        # 画像ファイルを処理
        image_data_list = process_images(expanded_images, args.threshold, args.invert)
        
        if not image_data_list:
            print("❌ 処理できる画像がありませんでした。")
            sys.exit(1)
        
        print("")
        print(f"📝 Cヘッダーファイルを生成中...")
        
        # ヘッダーファイルの内容を生成
        header_content = generate_c_header_content(image_data_list, args.invert)
        
        # ファイルに書き込み
        output_path.write_text(header_content, encoding='utf-8')
        
        # 結果を表示
        print("")
        print("=" * 60)
        print("✨ 変換完了！")
        print(f"📁 出力ファイル: {output_path}")
        print(f"🖼️  処理した画像数: {len(image_data_list)}")
        
        total_bytes = sum(len(item['byte_array']) for item in image_data_list)
        print(f"📦 総データサイズ: {total_bytes} バイト")
        
        print("\n📋 生成された変数:")
        for item in image_data_list:
            width_const = f"{item['var_name'].upper()}_WIDTH"
            height_const = f"{item['var_name'].upper()}_HEIGHT"
            print(f"  🔤 {item['var_name']}[] ({item['width']}x{item['height']}, {len(item['byte_array'])}バイト)")
            print(f"     定数: {width_const}, {height_const}")
        print("=" * 60)
        
    except ValueError as e:
        print(f"❌ 入力エラー: {e}", file=sys.stderr)
        sys.exit(1)
    except RuntimeError as e:
        print(f"❌ 実行エラー: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n⚠️ 処理が中断されました")
        sys.exit(1)
    except Exception as e:
        print(f"❌ 予期しないエラー: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()