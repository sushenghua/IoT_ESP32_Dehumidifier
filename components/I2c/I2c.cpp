/*
 * I2c: Wrap the esp i2c c interface
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "I2c.h"
#include "AppLog.h"

static I2c  _i2cInstance[] = { I2c(I2C_NUM_0), I2c(I2C_NUM_1) };

inline int mapI2cPort(i2c_port_t port)
{
  return (int)port;
}

I2c* I2c::instanceForPort(i2c_port_t port)
{
  return &_i2cInstance[mapI2cPort(port)];
}

I2c* I2c::instanceForPort(i2c_port_t port, int pinSck, int pinSda)
{
  I2c* instance = &_i2cInstance[mapI2cPort(port)];
  instance->setPins(pinSck, pinSda);
  return instance;
}

I2c::I2c(i2c_port_t port)
: _port(port)
, _inited(false)
{
  _config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  _config.sda_pullup_en = GPIO_PULLUP_ENABLE;
}

I2c::I2c(i2c_port_t port, int pinSck, int pinSda)
: _port(port)
, _inited(false)
{
  _config.scl_io_num = (gpio_num_t)pinSck;
  _config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  _config.sda_io_num = (gpio_num_t)pinSda;
  _config.sda_pullup_en = GPIO_PULLUP_ENABLE;
}

void I2c::setMode(i2c_mode_t mode)
{
  if (!_inited)
    _config.mode = mode;
}

void I2c::setMasterClkSpeed(uint32_t speed)
{
  if (!_inited && _config.mode == I2C_MODE_MASTER) {
    _config.master.clk_speed = speed;
  }
}

void I2c::setSlaveAddress(uint16_t addr, uint8_t enable10bitAddr)
{
  if (!_inited && _config.mode == I2C_MODE_SLAVE) {
    _config.slave.addr_10bit_en = enable10bitAddr;
    _config.slave.slave_addr = addr;
  }
}

void I2c::setPins(int pinSck, int pinSda)
{
  if (!_inited) {
    _config.scl_io_num = (gpio_num_t)pinSck;
    _config.sda_io_num = (gpio_num_t)pinSda;
  }
}

void I2c::init(size_t rxBufLen, size_t txBufLen)
{
  if (!_inited) {
    APP_LOGI("[I2c]", "init i2c with port %d", _port);
    i2c_param_config(_port, &_config);
    i2c_driver_install(_port, _config.mode,
                       rxBufLen, txBufLen, 0);
    _inited = true;
  }
}

void I2c::deinit()
{
  if (_inited) {
    ESP_LOGI("[I2c]", "deinit i2c with port %d", _port);
    i2c_driver_delete(_port);
    _inited = false;
  }
}

void I2c::reset()
{
  i2c_reset_tx_fifo(_port);
  i2c_reset_rx_fifo(_port);
}

#define WRITE_BIT      I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL        I2C_MASTER_ACK   /*!< I2C ack value 0x0 */
#define NACK_VAL       I2C_MASTER_NACK  /*!< I2C nack value 0x1 */

bool I2c::deviceReady(uint8_t addr, TickType_t waitTicks)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, waitTicks / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret == ESP_OK;
}

bool I2c::masterTx(uint8_t addr, uint8_t *data, size_t size, TickType_t waitTicks)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write(cmd, data, size, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, waitTicks / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret == ESP_OK;
}

bool I2c::masterRx(uint8_t addr, uint8_t *data, size_t size, TickType_t waitTicks)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN);
  if (size > 1) {
    i2c_master_read(cmd, data, size - 1, ACK_VAL);
  }
  i2c_master_read_byte(cmd, data + size - 1, NACK_VAL);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, waitTicks / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret == ESP_OK;
}

bool I2c::masterMemTx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t waitTicks)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, memAddr, ACK_CHECK_EN);
  i2c_master_write(cmd, data, size, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, waitTicks / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret == ESP_OK;
}

bool I2c::masterMemRx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t waitTicks)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, memAddr, ACK_CHECK_EN);
  i2c_master_start(cmd); // repeated start
  i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN);
  if (size > 1) {
    i2c_master_read(cmd, data, size - 1, ACK_VAL);
  }
  i2c_master_read_byte(cmd, data + size - 1, NACK_VAL);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, waitTicks / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret == ESP_OK;
}

int I2c::slaveTx(uint8_t *data, size_t size, TickType_t waitTicks)
{
  return i2c_slave_write_buffer(_port, data, size, waitTicks / portTICK_PERIOD_MS);
}

int I2c::slaveRx(uint8_t *data, size_t size, TickType_t waitTicks)
{
  return i2c_slave_read_buffer(_port, data, size, waitTicks / portTICK_PERIOD_MS);
}
