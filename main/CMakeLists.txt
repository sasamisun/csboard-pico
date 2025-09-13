# M5Unifiedとその依存関係を設定するにゃ
set(COMPONENT_REQUIRES 
    driver          # GPIO, SPI, I2C等のドライバーにゃ
    esp_system      # システム関数にゃ
    freertos        # FreeRTOSタスクにゃ
    log             # ログ出力にゃ
    spi_flash       # フラッシュアクセスにゃ
    nvs_flash       # 不揮発性ストレージにゃ
    M5Unified
    M5GFX
)

# プライベートな要求関係にゃ
set(COMPONENT_PRIV_REQUIRES )

# C++ソースファイルを指定するにゃ
set(COMPONENT_SRCS 
    "LGFX_ST7789P3_76x284.cpp"      # ST7789P3 (76×284) 専用LGFXクラス
    "RetroGamePaletteImage.cpp"     # レトロゲーム16色パレットシステム
    "app_main.cpp"                  # メインアプリケーション
    )

# インクルードディレクトリの設定にゃ
set(COMPONENT_ADD_INCLUDEDIRS "")

# コンポーネントを登録するにゃ
register_component()