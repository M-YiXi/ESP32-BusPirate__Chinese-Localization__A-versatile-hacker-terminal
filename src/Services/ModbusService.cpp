#include "ModbusService.h"

ModbusService* ModbusService::s_self = nullptr;
ModbusService::ModbusService()  { s_self = this; } // 单例初始化
ModbusService::~ModbusService() { if (s_self == this) s_self = nullptr; } // 单例销毁

bool ModbusService::setTarget(const std::string& hostOrIp, uint16_t port) {
  IPAddress ip;
  // 解析IPv4地址（支持域名/IP字符串）
  if (!resolveIPv4(hostOrIp, ip)) return false;
  _host = ip;
  _port = port ? port : 502; // Modbus TCP默认端口为502
  _mb.reset(new ModbusClientTCPasync(_host, _port));

  // 初始化Modbus客户端回调和参数
  if (_mb) {
    _mb->onDataHandler(&ModbusService::s_onData);    // 注册数据回调函数
    _mb->onErrorHandler(&ModbusService::s_onError);  // 注册错误回调函数
    _mb->setTimeout(_timeoutMs);                     // 设置请求超时时间
    _mb->setIdleTimeout(_idleCloseMs);               // 设置空闲连接关闭时间
    _mb->setMaxInflightRequests(_maxInflight);       // 设置最大并发请求数
  }
  return true;
}

void ModbusService::begin(uint32_t reqTimeoutMs, uint32_t idleCloseMs, uint32_t maxInflight) {
  // 初始化Modbus客户端参数
  _timeoutMs   = reqTimeoutMs;   // 请求超时时间（毫秒）
  _idleCloseMs = idleCloseMs;    // 空闲连接关闭时间（毫秒）
  _maxInflight = maxInflight;    // 最大并发请求数

  // 已创建客户端则更新参数和回调
  if (_mb) {
    _mb->onDataHandler(&ModbusService::s_onData);
    _mb->onErrorHandler(&ModbusService::s_onError);
    _mb->setTimeout(_timeoutMs);
    _mb->setIdleTimeout(_idleCloseMs);
    _mb->setMaxInflightRequests(_maxInflight);
  }
}

// FC01 - 读取线圈状态
Error ModbusService::readCoils(uint8_t unit, uint16_t addr0, uint16_t qty) {
  if (!_mb) return INVALID_SERVER; // 客户端未初始化返回无效服务器错误
  return _mb->addRequest(millis(), unit, READ_COIL, addr0, qty);
}

// FC02 - 读取离散输入状态
Error ModbusService::readDiscreteInputs(uint8_t unit, uint16_t addr0, uint16_t qty) {
  if (!_mb) return INVALID_SERVER;
  return _mb->addRequest(millis(), unit, READ_DISCR_INPUT, addr0, qty);
}

// FC03 - 读取保持寄存器
Error ModbusService::readHolding(uint8_t unit, uint16_t addr0, uint16_t qty) {
  if (!_mb) return INVALID_SERVER;
  return _mb->addRequest(millis(), unit, READ_HOLD_REGISTER, addr0, qty);
}

// FC04 - 读取输入寄存器
Error ModbusService::readInputRegisters(uint8_t unit, uint16_t addr0, uint16_t qty) {
  if (!_mb) return INVALID_SERVER;
  return _mb->addRequest(millis(), unit, READ_INPUT_REGISTER, addr0, qty);
}

// FC05 - 写入单个线圈
Error ModbusService::writeSingleCoil(uint8_t unit, uint16_t addr0, bool on) {
  if (!_mb) return INVALID_SERVER;
  // Modbus协议规定：0xFF00表示置1，0x0000表示置0
  const uint16_t val = on ? 0xFF00 : 0x0000;
  return _mb->addRequest(millis(), unit, WRITE_COIL, addr0, val);
}

// FC06 - 写入单个保持寄存器
Error ModbusService::writeHoldingSingle(uint8_t unit, uint16_t addr0, uint16_t value) {
  if (!_mb) return INVALID_SERVER;
  return _mb->addRequest(millis(), unit, WRITE_HOLD_REGISTER, addr0, value);
}

// FC10/0x16 - 写入多个保持寄存器
Error ModbusService::writeHoldingMultiple(uint8_t unit, uint16_t addr0, const std::vector<uint16_t>& values) {
  if (!_mb) return INVALID_SERVER;
  std::vector<uint16_t> tmp(values.begin(), values.end());
  const uint16_t qty     = static_cast<uint16_t>(tmp.size());    // 寄存器数量
  const uint8_t  byteCnt = static_cast<uint8_t>(qty * 2);        // 字节总数（每个寄存器2字节）
  return _mb->addRequest(millis(), unit, WRITE_MULT_REGISTERS, addr0, qty, byteCnt, tmp.data());
}

// FC0F - 写入多个线圈
Error ModbusService::writeMultipleCoils(uint8_t unit, uint16_t addr0,
                                              const std::vector<uint8_t>& packedBytes,
                                              uint16_t coilQty) {
  if (!_mb) return INVALID_SERVER;
  std::vector<uint8_t> tmp(packedBytes.begin(), packedBytes.end());
  const uint8_t byteCnt = static_cast<uint8_t>(tmp.size()); // 打包后的字节数
  return _mb->addRequest(millis(), unit, WRITE_MULT_COILS, addr0, coilQty, byteCnt, tmp.data());
}

// 静态数据回调转发到实例方法
void ModbusService::s_onData(ModbusMessage resp, uint32_t token) {
  if (s_self) s_self->onData(resp, token);
}

// 静态错误回调转发到实例方法
void ModbusService::s_onError(Error error, uint32_t token) {
  if (s_self) s_self->onError(error, token);
}

void ModbusService::onData(ModbusMessage& resp, uint32_t token) {
  Reply r;
  r.fc = resp.getFunctionCode();                  // 获取功能码
  r.raw.assign(resp.begin(), resp.end());         // 调试用：保存原始响应数据

  // 功能码最高位为1表示异常响应
  if (r.fc & 0x80) {
    r.ok = false;
    // 异常响应第二个字节为异常码
    if (resp.size() >= 2) r.exception = resp[1];
  } else {
    r.ok = true;

    // 解析保持寄存器/输入寄存器响应（FC03/FC04）
    if (r.fc == READ_HOLD_REGISTER || r.fc == READ_INPUT_REGISTER) {
      if (!parseFC03or04(resp, r.regs)) {
        r.ok = false; r.error = "解析FC03/04响应失败";
      }

    // 解析线圈/离散输入响应（FC01/FC02）
    } else if (r.fc == READ_COIL || r.fc == READ_DISCR_INPUT) {
      if (!parseFC01or02(resp, r.coilBytes, r.byteCount)) {
        r.ok = false; r.error = "解析FC01/02响应失败";
      }
    }
    // 其他功能码可在此扩展解析逻辑
  }

  // 触发外部注册的响应回调
  if (_onReply) _onReply(r, token);
}

void ModbusService::onError(Error error, uint32_t token) {
  // 触发外部注册的错误回调
  if (_onError) _onError(error, token);

  // 构造错误响应对象并触发通用回调
  Reply r;
  r.ok = false;
  ModbusError me(error);
  r.error = (const char*)me; // 转换错误码为可读字符串

  if (_onReply) _onReply(r, token);
}

bool ModbusService::resolveIPv4(const std::string& host, IPAddress& outIp) {
  // 解析域名/IP字符串为IPv4地址
  addrinfo hints{}; 
  hints.ai_family = AF_INET;        // 仅解析IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP流套接字
  addrinfo* res = nullptr;
  
  // 调用系统接口解析地址
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
  auto* sin = reinterpret_cast<sockaddr_in*>(res->ai_addr);
  outIp = IPAddress(sin->sin_addr.s_addr);
  freeaddrinfo(res); // 释放地址解析资源
  return true;
}

bool ModbusService::parseFC03or04(ModbusMessage& msg, std::vector<uint16_t>& outRegs) {
  // 解析FC03(读取保持寄存器)/FC04(读取输入寄存器)响应
  if (msg.size() < 3) return false; // 响应长度不足（至少3字节：功能码+字节数）
  uint8_t fc = msg.getFunctionCode();
  if (fc != 0x03 && fc != 0x04) return false; // 功能码不匹配
  uint8_t bc = msg[2]; // 数据字节数
  if (msg.size() < static_cast<size_t>(3 + bc)) return false; // 数据长度不匹配
  if (bc % 2) return false; // 寄存器数据必须为偶数字节（每个寄存器2字节）

  // 解析16位寄存器数据（高位在前）
  outRegs.resize(bc / 2);
  for (size_t i = 0; i < outRegs.size(); ++i) {
    outRegs[i] = static_cast<uint16_t>((msg[3 + 2*i] << 8) | msg[3 + 2*i + 1]);
  }
  return true;
}

bool ModbusService::parseFC01or02(ModbusMessage& msg,
                                  std::vector<uint8_t>& outBytes,
                                  uint8_t& outByteCount) {
  // 解析FC01(读取线圈)/FC02(读取离散输入)响应
  if (msg.size() < 3) return false; // 响应长度不足
  uint8_t fc = msg.getFunctionCode();
  if (fc != READ_COIL && fc != READ_DISCR_INPUT) return false; // 功能码不匹配

  uint8_t bc = msg[2]; // 数据字节数
  if (bc == 0) return false; // 无数据
  if (msg.size() < static_cast<size_t>(3 + bc)) return false; // 数据长度不匹配

  // 提取线圈状态字节（低位优先）
  outByteCount = bc;
  outBytes.assign(msg.begin() + 3, msg.begin() + 3 + bc);
  return true;
}