#!/usr/bin/env python3
"""
画像カットツール - Image Cutter
指定した位置で画像を上から順番にカットして保存するプログラム

使用例:
python image_cutter.py image.png 100 200 150
→ image_cut_0_100.png (0-100ピクセル)
→ image_cut_100_300.png (100-300ピクセル)  
→ image_cut_300_450.png (300-450ピクセル)

python image_cutter.py -p .\nekonoba2025.bmp 560 730 1180
"""

import sys
import argparse
from pathlib import Path
from PIL import Image


def parse_arguments():
    """
    コマンドライン引数を解析する関数
    画像ファイルパスと切り出し位置のリストを返す
    """
    parser = argparse.ArgumentParser(
        description='画像を指定した位置で上から順番にカットして保存します',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用例:
  python %(prog)s image.png 100 200 150
  python %(prog)s photo.jpg 50 100 75 200
  python %(prog)s --position image.png 560 730 1180  (絶対位置モード)
        """
    )
    
    # 画像ファイルの引数
    parser.add_argument('image', 
                       help='処理する画像ファイルのパス')
    
    # カット位置の引数（1つ以上）
    parser.add_argument('cuts', 
                       nargs='+', 
                       type=int,
                       help='カット位置（ピクセル単位、複数指定可能）')
    
    # 絶対位置モードのオプション
    parser.add_argument('--position', '-p',
                       action='store_true',
                       help='数字を高さではなく絶対Y座標として解釈する')
    
    return parser.parse_args()


def validate_image_file(image_path):
    """
    画像ファイルの存在確認と形式チェック
    
    Args:
        image_path (str): 画像ファイルのパス
        
    Returns:
        Path: 検証済みのPathオブジェクト
        
    Raises:
        FileNotFoundError: ファイルが存在しない場合
        ValueError: サポートされていない画像形式の場合
    """
    path = Path(image_path)
    
    # ファイルの存在確認
    if not path.exists():
        raise FileNotFoundError(f"画像ファイルが見つかりません: {image_path}")
    
    # サポートされている画像形式の確認
    supported_formats = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff', '.webp'}
    if path.suffix.lower() not in supported_formats:
        raise ValueError(f"サポートされていない画像形式です: {path.suffix}")
    
    return path


def create_output_filename(original_path, start_y, end_y, index):
    """
    出力ファイル名を生成する関数
    
    Args:
        original_path (Path): 元画像のパス
        start_y (int): 切り出し開始Y座標
        end_y (int): 切り出し終了Y座標
        index (int): 切り出し順序番号
        
    Returns:
        Path: 出力ファイルのパス
    """
    stem = original_path.stem  # 拡張子を除いたファイル名
    suffix = original_path.suffix  # 拡張子
    
    # ファイル名の形式: 元ファイル名_cut_開始位置_終了位置.拡張子
    output_name = f"{stem}_cut_{index}_{start_y}_{end_y}{suffix}"
    return original_path.parent / output_name


def cut_image_sections(image_path, cut_positions, position_mode=False):
    """
    画像を指定した位置で切り出す主処理関数
    
    Args:
        image_path (Path): 画像ファイルのパス
        cut_positions (list): 切り出し位置のリスト
        position_mode (bool): Trueの場合は絶対位置モード、Falseの場合は高さモード
        
    Returns:
        list: 保存されたファイルパスのリスト
    """
    try:
        # 画像を開く
        with Image.open(image_path) as img:
            print(f"画像を読み込みました: {image_path}")
            print(f"画像サイズ: {img.size[0]} x {img.size[1]} ピクセル")
            
            image_width, image_height = img.size
            saved_files = []
            
            # 切り出し位置を昇順でソート（重複も除去）
            unique_cuts = sorted(set(cut_positions))
            
            if position_mode:
                print(f"絶対位置モード - 切り出し位置: {unique_cuts}")
                return cut_by_absolute_positions(img, image_path, unique_cuts, image_width, image_height)
            else:
                print(f"高さモード - 切り出し高さ: {unique_cuts}")
                return cut_by_heights(img, image_path, unique_cuts, image_width, image_height)
            
    except Exception as e:
        raise RuntimeError(f"画像処理中にエラーが発生しました: {e}")


def cut_by_heights(img, image_path, unique_cuts, image_width, image_height):
    """
    高さベースで画像を切り出す（元の動作）
    """
    saved_files = []
    start_y = 0  # 最初は画像の上端から開始
    
    for i, cut_height in enumerate(unique_cuts):
        # 不正な値のチェック
        if cut_height <= 0:
            print(f"警告: 切り出し高さ {cut_height} は無効です（スキップ）")
            continue
        
        # 切り出し終了位置を計算
        end_y = min(start_y + cut_height, image_height)
        
        # 切り出し範囲が有効かチェック
        if start_y >= image_height:
            print(f"警告: 開始位置 {start_y} が画像の高さを超えています（残りはスキップ）")
            break
        
        if start_y >= end_y:
            print(f"警告: 切り出し範囲が無効です start:{start_y}, end:{end_y}（スキップ）")
            continue
        
        # 画像を切り出し (left, top, right, bottom)
        cropped_img = img.crop((0, start_y, image_width, end_y))
        
        # 出力ファイル名を生成
        output_path = create_output_filename(image_path, start_y, end_y, i)
        
        # 切り出した画像を保存
        cropped_img.save(output_path)
        saved_files.append(output_path)
        
        print(f"保存しました: {output_path} (Y: {start_y}-{end_y}, 高さ: {end_y-start_y}px)")
        
        # 次の切り出し開始位置を更新
        start_y = end_y
        
        # 画像の底に達した場合は終了
        if start_y >= image_height:
            break
    
    return saved_files


def cut_by_absolute_positions(img, image_path, unique_cuts, image_width, image_height):
    """
    絶対位置ベースで画像を切り出す（新機能）
    """
    saved_files = []
    positions = [0] + unique_cuts  # 先頭に0を追加
    
    for i in range(len(positions) - 1):
        start_y = positions[i]
        end_y = positions[i + 1]
        
        # 範囲の妥当性チェック
        if start_y >= image_height:
            print(f"警告: 開始位置 {start_y} が画像の高さ {image_height} を超えています（残りはスキップ）")
            break
        
        if end_y <= start_y:
            print(f"警告: 無効な範囲 {start_y}-{end_y}（スキップ）")
            continue
        
        # 画像の範囲内にクリップ
        end_y = min(end_y, image_height)
        
        # 画像を切り出し (left, top, right, bottom)
        cropped_img = img.crop((0, start_y, image_width, end_y))
        
        # 出力ファイル名を生成
        output_path = create_output_filename(image_path, start_y, end_y, i)
        
        # 切り出した画像を保存
        cropped_img.save(output_path)
        saved_files.append(output_path)
        
        print(f"保存しました: {output_path} (Y: {start_y}-{end_y}, 高さ: {end_y-start_y}px)")
    
    return saved_files
            


def main():
    """
    メイン処理関数
    コマンドライン引数の解析から画像処理まで一連の流れを管理
    """
    try:
        # コマンドライン引数を解析
        args = parse_arguments()
        
        # 画像ファイルの検証
        image_path = validate_image_file(args.image)
        
        # 切り出し位置の検証
        if not args.cuts:
            raise ValueError("少なくとも1つの切り出し位置を指定してください")
        
        # 負の値をチェック
        invalid_cuts = [cut for cut in args.cuts if cut <= 0]
        if invalid_cuts:
            print(f"警告: 以下の無効な切り出し値は無視されます: {invalid_cuts}")
        
        print("=" * 50)
        print("🎨 画像カットツール開始！")
        print("=" * 50)
        
        # 画像を切り出し処理
        saved_files = cut_image_sections(image_path, args.cuts, args.position)
        
        # 結果を表示
        print("\n" + "=" * 50)
        print("✨ 処理完了！")
        print(f"📁 {len(saved_files)} 個のファイルを保存しました:")
        for file_path in saved_files:
            print(f"  📄 {file_path}")
        print("=" * 50)
        
    except FileNotFoundError as e:
        print(f"❌ ファイルエラー: {e}", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"❌ 値エラー: {e}", file=sys.stderr)
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