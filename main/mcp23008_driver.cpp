/**
 * mcp23008_driver.cpp - MCP23008 IOエキスパンダ制御ライブラリ実装
 * 
 * このファイルはMCP23008の制御機能を実装していますにゃ
 * 5つのスイッチ入力 (GP0-GP4) を効率的に読み取りできます
 * 
 * 特徴：
 * - 信頼性の高いI2C通信
 * - 詳細なエラーハンドリング
 * - デバッグ機能付き
 * - プルアップ抵抗自動設定
 */

#include "mcp23008_driver.h"

// ログ用のタグを定義 にゃ
const char* MCP23008::TAG = "MCP23008";

// I2C通信のタイムアウト値 (ミリ秒) にゃ
static const int I2C_TIMEOUT_MS = 1000;

// スイッチ用のGPIOマスク (GP0-GP4 = bit 0-4) にゃ
static const uint8_t SWITCH_MASK = 0x1F;  // 0001 1111

/**
 * コンストラクタ
 * I2Cポートとアドレスを設定しますにゃ
 */
MCP23008::MCP23008(i2c_port_t port, uint8_t addr) {
    i2c_port = port;
    device_address = addr;
    ESP_LOGI(TAG, "MCP23008 initialized with I2C port %d, address 0x%02X", port, addr);
}

/**
 * MCP23008の初期設定を行います
 * この関数を最初に呼び出してくださいにゃ
 */
esp_err_t MCP23008::begin() {
    esp_err_t err;
    
    ESP_LOGI(TAG, "Starting MCP23008 initialization...");
    
    // まず接続テストを実行 にゃ
    err = testConnection();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Connection test failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 1: GP0-GP4を入力に設定 (bit0-4 = 1, その他は0で出力)
    uint8_t iodir_value = SWITCH_MASK;  // 0001 1111 = GP0-GP4を入力に設定
    err = writeRegister(MCP23008_REG_IODIR, iodir_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IODIR register: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "IODIR register set to 0x%02X (GP0-GP4: INPUT, GP5-GP7: OUTPUT)", iodir_value);
    
    // Step 2: GP0-GP4のプルアップ抵抗を有効にする
    uint8_t gppu_value = SWITCH_MASK;   // 0001 1111 = GP0-GP4のプルアップ有効
    err = writeRegister(MCP23008_REG_GPPU, gppu_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPPU register: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "GPPU register set to 0x%02X (GP0-GP4: PULLUP ENABLED)", gppu_value);
    
    // Step 3: 入力極性を通常に設定 (0 = 通常, 1 = 反転)
    err = writeRegister(MCP23008_REG_IPOL, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IPOL register: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "IPOL register set to 0x00 (normal polarity)");
    
    // Step 4: I/O設定レジスタ（デフォルト値で良い）
    err = writeRegister(MCP23008_REG_IOCON, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IOCON register: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "IOCON register set to 0x00 (default configuration)");
    
    // 初期化完了をログ出力
    ESP_LOGI(TAG, "MCP23008 initialization completed successfully!");
    
    // 初期設定を表示 (デバッグ用)
    printConfiguration();
    
    return ESP_OK;
}

/**
 * スイッチの状態を読み取り (GP0-GP4のみ)
 * 戻り値: bit4=GP4, bit3=GP3, bit2=GP2, bit1=GP1, bit0=GP0
 */
esp_err_t MCP23008::readSwitches(uint8_t* switch_state) {
    if (switch_state == nullptr) {
        ESP_LOGE(TAG, "switch_state pointer is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t gpio_data;
    esp_err_t err = readRegister(MCP23008_REG_GPIO, &gpio_data);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read GPIO register: %s", esp_err_to_name(err));
        return err;
    }
    
    // GP0-GP4のみを取得 (下位5ビット)
    *switch_state = gpio_data & SWITCH_MASK;
    
    ESP_LOGD(TAG, "Switch state read: 0x%02X (raw GPIO: 0x%02X)", *switch_state, gpio_data);
    
    return ESP_OK;
}

/**
 * 指定したスイッチが押されているかチェック
 * スイッチが押されている場合は true を返しますにゃ
 */
esp_err_t MCP23008::isSwitchPressed(uint8_t switch_num, bool* is_pressed) {
    if (is_pressed == nullptr) {
        ESP_LOGE(TAG, "is_pressed pointer is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (switch_num > 4) {
        ESP_LOGE(TAG, "Invalid switch number: %d (must be 0-4)", switch_num);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t switch_state;
    esp_err_t err = readSwitches(&switch_state);
    
    if (err != ESP_OK) {
        return err;
    }
    
    // スイッチが押されている場合は0、離されている場合は1
    // (プルアップ抵抗により)
    *is_pressed = ((switch_state >> switch_num) & 0x01) == 0;
    
    ESP_LOGD(TAG, "Switch %d is %s", switch_num, *is_pressed ? "PRESSED" : "RELEASED");
    
    return ESP_OK;
}

/**
 * 全GPIOピンの状態を読み取り (デバッグ用)
 */
esp_err_t MCP23008::readAllGPIO(uint8_t* gpio_state) {
    if (gpio_state == nullptr) {
        ESP_LOGE(TAG, "gpio_state pointer is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err = readRegister(MCP23008_REG_GPIO, gpio_state);
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "All GPIO state: 0x%02X", *gpio_state);
    }
    
    return err;
}

/**
 * 接続テストを実行
 * IODIRレジスタの読み書きでテストしますにゃ
 */
esp_err_t MCP23008::testConnection() {
    ESP_LOGI(TAG, "Testing MCP23008 connection...");
    
    // 現在のIODIR値を読み取り
    uint8_t original_value;
    esp_err_t err = readRegister(MCP23008_REG_IODIR, &original_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read IODIR register for connection test: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Connection test passed! Device is responding. Current IODIR: 0x%02X", original_value);
    return ESP_OK;
}

/**
 * 現在の設定を表示 (デバッグ用)
 * 全レジスタの値をログに出力しますにゃ
 */
void MCP23008::printConfiguration() {
    ESP_LOGI(TAG, "=== MCP23008 Current Configuration ===");
    
    uint8_t reg_value;
    const char* reg_names[] = {
        "IODIR  ", "IPOL   ", "GPINTEN", "DEFVAL ", "INTCON ",
        "IOCON  ", "GPPU   ", "INTF   ", "INTCAP ", "GPIO   ", "OLAT   "
    };
    
    for (int i = 0; i <= 0x0A; i++) {
        if (readRegister(i, &reg_value) == ESP_OK) {
            ESP_LOGI(TAG, "  %s (0x%02X): 0x%02X (%s)", 
                     reg_names[i], i, reg_value,
                     (i <= 0x0A) ? "OK" : "Unknown");
        } else {
            ESP_LOGW(TAG, "  %s (0x%02X): READ FAILED", reg_names[i], i);
        }
    }
    
    ESP_LOGI(TAG, "=== Configuration Display Complete ===");
}

/**
 * 指定レジスタに1バイト書き込み
 * プライベート関数ですにゃ
 */
esp_err_t MCP23008::writeRegister(uint8_t reg_addr, uint8_t data) {
    // I2Cコマンドハンドルを作成
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == nullptr) {
        ESP_LOGE(TAG, "Failed to create I2C command handle");
        return ESP_ERR_NO_MEM;
    }
    
    // I2C通信シーケンス：
    // START -> DEVICE_ADDR(Write) -> REG_ADDR -> DATA -> STOP
    i2c_master_start(cmd);                                           // START条件
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);  // デバイスアドレス + Write bit
    i2c_master_write_byte(cmd, reg_addr, true);                      // レジスタアドレス
    i2c_master_write_byte(cmd, data, true);                          // データ
    i2c_master_stop(cmd);                                            // STOP条件
    
    // I2C通信を実行
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Write register 0x%02X failed: %s", reg_addr, esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "Write register 0x%02X = 0x%02X success", reg_addr, data);
    }
    
    return err;
}

/**
 * 指定レジスタから1バイト読み取り
 * プライベート関数ですにゃ
 */
esp_err_t MCP23008::readRegister(uint8_t reg_addr, uint8_t* data) {
    if (data == nullptr) {
        ESP_LOGE(TAG, "Data pointer is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    // I2Cコマンドハンドルを作成
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == nullptr) {
        ESP_LOGE(TAG, "Failed to create I2C command handle");
        return ESP_ERR_NO_MEM;
    }
    
    // I2C通信シーケンス：
    // START -> DEVICE_ADDR(Write) -> REG_ADDR -> RESTART -> DEVICE_ADDR(Read) -> DATA -> NACK -> STOP
    i2c_master_start(cmd);                                           // START条件
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);  // デバイスアドレス + Write bit
    i2c_master_write_byte(cmd, reg_addr, true);                      // レジスタアドレス
    
    i2c_master_start(cmd);                                           // RESTART条件 (重要!)
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_READ, true);   // デバイスアドレス + Read bit
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);                // データ読み取り + NACK
    i2c_master_stop(cmd);                                            // STOP条件
    
    // I2C通信を実行
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read register 0x%02X failed: %s", reg_addr, esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "Read register 0x%02X = 0x%02X success", reg_addr, *data);
    }
    
    return err;
}