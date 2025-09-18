/**
 * mcp23008_driver.h - MCP23008 IOエキスパンダ制御ライブラリ
 * 
 * 機能：
 * - MCP23008の初期化と制御
 * - GP0-GP4の5つのスイッチ入力をサポート
 * - プルアップ抵抗の自動設定
 * - エラーハンドリング機能付き
 * 
 * 使用例：
 * MCP23008 mcp(I2C_NUM_0, 0x20);
 * mcp.begin();
 * uint8_t switches = mcp.readSwitches();
 */

#ifndef MCP23008_DRIVER_H
#define MCP23008_DRIVER_H

#include "esp_err.h"
#include "driver/i2c.h"
#include "esp_log.h"

// MCP23008 レジスタアドレス定数 にゃ
const uint8_t MCP23008_REG_IODIR   = 0x00;  // 入出力方向設定レジスタ (1=入力, 0=出力)
const uint8_t MCP23008_REG_IPOL    = 0x01;  // 入力極性レジスタ (1=反転, 0=通常)
const uint8_t MCP23008_REG_GPINTEN = 0x02;  // 割り込み有効レジスタ
const uint8_t MCP23008_REG_DEFVAL  = 0x03;  // デフォルト比較値レジスタ
const uint8_t MCP23008_REG_INTCON  = 0x04;  // 割り込み制御レジスタ
const uint8_t MCP23008_REG_IOCON   = 0x05;  // I/O設定レジスタ
const uint8_t MCP23008_REG_GPPU    = 0x06;  // GPIO プルアップレジスタ (1=プルアップ有効)
const uint8_t MCP23008_REG_INTF    = 0x07;  // 割り込みフラグレジスタ (読み取り専用)
const uint8_t MCP23008_REG_INTCAP  = 0x08;  // 割り込み時キャプチャレジスタ (読み取り専用)
const uint8_t MCP23008_REG_GPIO    = 0x09;  // GPIO ポート値レジスタ (メイン制御レジスタ)
const uint8_t MCP23008_REG_OLAT    = 0x0A;  // 出力ラッチレジスタ

/**
 * MCP23008 IOエキスパンダ制御クラス
 * 5つのスイッチ入力 (GP0-GP4) に特化した実装にゃ
 */
class MCP23008 {
private:
    i2c_port_t i2c_port;          // 使用するI2Cポート (通常 I2C_NUM_0)
    uint8_t device_address;       // デバイスのI2Cアドレス (通常 0x20)
    static const char* TAG;       // ログ用タグ
    
    /**
     * 単一レジスタに1バイト書き込み
     * @param reg_addr レジスタアドレス
     * @param data 書き込むデータ
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t writeRegister(uint8_t reg_addr, uint8_t data);
    
    /**
     * 単一レジスタから1バイト読み取り
     * @param reg_addr レジスタアドレス
     * @param data 読み取ったデータの格納先
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t readRegister(uint8_t reg_addr, uint8_t* data);

public:
    /**
     * コンストラクタ
     * @param port I2Cポート番号 (I2C_NUM_0 or I2C_NUM_1)
     * @param addr デバイスアドレス (通常 0x20)
     */
    MCP23008(i2c_port_t port, uint8_t addr);
    
    /**
     * MCP23008を初期化
     * - GP0-GP4を入力に設定
     * - プルアップ抵抗を有効化
     * - その他のピンは未使用として設定
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t begin();
    
    /**
     * スイッチの状態を読み取り (GP0-GP4のみ)
     * @param switch_state 読み取った5ビットのスイッチ状態 (bit4=GP4, bit0=GP0)
     * @return esp_err_t ESP_OK=成功, その他=エラー
     * 
     * 注意：スイッチが押されている場合は0、離されている場合は1が返されます
     *      (プルアップ抵抗により、スイッチが離されていると HIGH レベル)
     */
    esp_err_t readSwitches(uint8_t* switch_state);
    
    /**
     * 指定した単一スイッチの状態を確認
     * @param switch_num スイッチ番号 (0-4: GP0-GP4に対応)
     * @param is_pressed スイッチが押されているかどうか (true=押されている, false=離されている)
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t isSwitchPressed(uint8_t switch_num, bool* is_pressed);
    
    /**
     * 全ての GPIO ピンの状態を読み取り (デバッグ用)
     * @param gpio_state 8ビット全てのGPIO状態
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t readAllGPIO(uint8_t* gpio_state);
    
    /**
     * デバイスが正常に通信できるかテスト
     * IODIRレジスタを読み書きしてテストします
     * @return esp_err_t ESP_OK=成功, その他=エラー
     */
    esp_err_t testConnection();
    
    /**
     * 現在の設定を表示 (デバッグ用)
     * 全てのレジスタの値をログに出力します
     */
    void printConfiguration();
};

#endif // MCP23008_DRIVER_H