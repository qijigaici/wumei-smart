/***********************************************************
 * author: kerwincui
 * create: 2022-02-20
 * email：164770707@qq.com
 * source:https://github.com/kerwincui/wumei-smart
 * board:esp8266 core for arduino v3.0.2
 ***********************************************************/

#include "Helper.h"

WiFiClient wifiClient;
PubSubClient mqttClient;
float rssi = 0;
char wumei_iv[17] = "wumei-smart-open";
int monitorCount = 0;
long monitorInterval = 1000;

//==================================== 这是需要配置的项 ===============================
// 设备信息
String deviceNum = "DRO5938QISX72V6A";
String userId = "1";
String productId = "57";
String firmwareVersion = "1.1";

// Wifi配置
char *wifiSsid = "huawei";
char *wifiPwd = "15208747707";

// Mqtt配置
char *mqttHost = "wumei.live";
int mqttPort = 1883;
char *mqttUserName = "wumei-smart";
char *mqttPwd = "PHA8VG19W5IE7743";
char mqttSecret[17] = "K674HDNN2N6683E3";

// NTP地址（用于获取时间）
String ntpServer = "http://wumei.live:8080/iot/tool/ntp?deviceSendTime=";
//====================================================================================

// 订阅的主题
String prefix = "/" + productId + "/" + deviceNum;
String sOtaTopic = prefix + "/ota/get";
String sNtpTopic = prefix + "/ntp/get";
String sPropertyTopic = prefix + "/property/get/#";
String sFunctionTopic = prefix + "/function/get/#";
String sMonitorTopic = prefix + "/monitor/get";
// 发布的主题
String pInfoTopic = prefix + "/info/post";
String pNtpTopic = prefix + "/ntp/post";
String pPropertyTopic = prefix + "/property/post";
String pFunctionTopic = prefix + "/function/post";
String pMonitorTopic = prefix + "/monitor/post";
String pEventTopic = prefix + "/event/post";

// Mqtt回调
void callback(char *topic, byte *payload, unsigned int length)
{
  blink();
  printMsg("接收数据：");
  String data = "";
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }

  if (strcmp(topic, sOtaTopic.c_str()) == 0)
  {
    printMsg("订阅到设备升级指令...");
  }
  else if (strcmp(topic, sNtpTopic.c_str()) == 0)
  {
    printMsg("订阅到NTP时间...");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    // 计算设备当前时间：(${serverRecvTime} + ${serverSendTime} + ${deviceRecvTime} - ${deviceSendTime}) / 2
    float deviceSendTime = doc["deviceSendTime"];
    float serverSendTime = doc["serverSendTime"];
    float serverRecvTime = doc["serverRecvTime"];
    float deviceRecvTime = millis();
    float now = (serverSendTime + serverRecvTime + deviceRecvTime - deviceSendTime) / 2;
    printMsg("当前时间：" + String(now, 0));
  }
  else if (strcmp(topic, sPropertyTopic.c_str()) == 0 || strcmp(topic, sPropertyTopic.substring(0, sPropertyTopic.lastIndexOf("/")).c_str()) == 0)
  {
    printMsg("订阅到属性指令...");
    processProperty(payload);
  }
  else if (strcmp(topic, sFunctionTopic.c_str()) == 0 || strcmp(topic, sFunctionTopic.substring(0, sFunctionTopic.lastIndexOf("/")).c_str()) == 0)
  {
    printMsg("订阅到功能指令...");
    processFunction(payload);
  }
  else if (strcmp(topic, sMonitorTopic.c_str()) == 0)
  {
    printMsg("订阅到实时监测指令...");
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    monitorCount = doc["count"];
    monitorInterval = doc["interval"];
  }
}

// 属性处理
void processProperty(byte *payload)
{
  StaticJsonDocument<512> doc;
  deserializeJson(doc, payload);
  JsonArray array = doc.as<JsonArray>();
  for (JsonObject object : array)
  {
    // 匹配云端定义的属性（不包含属性中的监测数据）
    String id = object["id"];
    String value = object["value"];
    printMsg(id + "：" + value);
  }
  // 最后发布属性（重要）
  publishProperty((char *)payload);
}

// 功能处理
void processFunction(byte *payload)
{
  StaticJsonDocument<512> doc;
  deserializeJson(doc, payload);
  JsonArray array = doc.as<JsonArray>();
  for (JsonObject object : array)
  {
    // 匹配云端定义的功能
    String id = object["id"];
    String value = object["value"];
    if (value == "switch")
    {
      printMsg("开关 switch：" + value);
    }
    else if (value == "gear")
    {
      printMsg("档位 gear：" + value);
    }
    else if (value == "light_color")
    {
      printMsg("灯光颜色 light_color：" + value);
    }
    else if (value == "message")
    {
      printMsg("屏显消息 message：" + value);
    }
  }
  // 最后发布功能（重要）
  publishFunction((char *)payload);
}

// 1.发布设备信息
void publishInfo()
{
  StaticJsonDocument<256> doc;
  doc["rssi"] = WiFi.RSSI();
  doc["firmwareVersion"] = firmwareVersion;
  doc["status"] = 3; // （1-未激活，2-禁用，3-在线，4-离线）
  doc["userId"] = (String)userId;

  printMsg("发布设备信息:");
  serializeJson(doc, Serial);
  String output;
  serializeJson(doc, output);
  mqttClient.publish(pInfoTopic.c_str(), output.c_str());
}

// 2.发布时钟同步信，用于获取当前时间(可选)
void publishNtp()
{
  StaticJsonDocument<128> doc;
  doc["deviceSendTime"] = millis();

  printMsg("发布NTP信息:");
  serializeJson(doc, Serial);
  String output;
  serializeJson(doc, output);
  mqttClient.publish(pNtpTopic.c_str(), output.c_str());
}

// 3.发布属性
void publishProperty(char *msg)
{
  printMsg("发布属性:" + (String)msg);
  mqttClient.publish(pPropertyTopic.c_str(), msg);
}

// 4.发布功能
void publishFunction(char *msg)
{
  printMsg("发布功能:" + (String)msg);
  mqttClient.publish(pFunctionTopic.c_str(), msg);
}

// 5.发布事件
void publishEvent()
{
  // 匹配云端的事件
  StaticJsonDocument<512> doc;
  JsonObject objTmeperature = doc.createNestedObject();
  objTmeperature["id"] = "height_temperature";
  objTmeperature["value"] = "40";
  objTmeperature["remark"] = "事件备注信息";

  JsonObject objException = doc.createNestedObject();
  objException["id"] = "exception";
  objException["value"] = "设备异常掉线，请检查网络";
  objException["remark"] = "事件备注信息";

  printMsg("发布事件:");
  serializeJson(doc, Serial);
  String output;
  serializeJson(doc, output);
  mqttClient.publish(pEventTopic.c_str(), output.c_str());
}

// 6.发布实时监测数据(1=只是监测数据，2=属性)
void publishMonitor(int type)
{
  // 匹配云端定义的监测数据，随机数代替监测结果
  float randNumber = 0;
  StaticJsonDocument<1024> doc;
  JsonObject objTmeperature = doc.createNestedObject();
  objTmeperature["id"] = "temperature";
  randNumber = random(1000, 3000) ;
  objTmeperature["value"] = (String)(randNumber/100);
  objTmeperature["remark"] = "监测数据备注";

  JsonObject objHumidity   = doc.createNestedObject();
  objHumidity["id"] = "humidity";
  randNumber = random(3000, 6000);
  objHumidity["value"] = (String)(randNumber/100);
  objHumidity["remark"] = "监测数据备注";

  JsonObject objCo2 = doc.createNestedObject();
  objCo2["id"] = "co2";
  randNumber = random(400, 1000);
  objCo2["value"] = (String)(randNumber);
  objCo2["remark"] = "监测数据备注";

  JsonObject objBrightness = doc.createNestedObject();
  objBrightness["id"] = "brightness";
  randNumber = random(1000, 10000);
  objBrightness["value"] = (String)(randNumber);
  objBrightness["remark"] = "监测数据备注";

  printMsg("发布监测数据:");
  serializeJson(doc, Serial);
  String output;
  serializeJson(doc, output);

  if (type == 1)
  {
    mqttClient.publish(pMonitorTopic.c_str(), output.c_str());
  }
  else if (type == 2)
  {
    mqttClient.publish(pPropertyTopic.c_str(), output.c_str());
  }
}

// 连接wifi
void connectWifi()
{
  printMsg("连接 ");
  Serial.print(wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPwd);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  printMsg("WiFi连接成功");
  printMsg("IP地址: ");
  Serial.print(WiFi.localIP());
}

// 连接mqtt
void connectMqtt()
{
  printMsg("连接Mqtt服务器...");
  // 生成mqtt认证密码（密码 = mqtt密码 & 用户ID & 过期时间）
  String password = generationPwd();
  String encryptPassword = encrypt(password, mqttSecret, wumei_iv);
  printMsg("密码(已加密)：" + encryptPassword);
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(15);
  //连接（客户端ID = 设备编号 & 产品ID）
  String clientId = deviceNum + "&" + productId;
  bool connectResult = mqttClient.connect(clientId.c_str(), mqttUserName, encryptPassword.c_str());
  if (connectResult)
  {
    printMsg("连接成功");
    // 订阅(OTA、NTP、属性、功能、实时监测)
    mqttClient.subscribe(sOtaTopic.c_str(), 1);
    mqttClient.subscribe(sNtpTopic.c_str(), 1);
    mqttClient.subscribe(sPropertyTopic.c_str(), 1);
    mqttClient.subscribe(sFunctionTopic.c_str(), 1);
    mqttClient.subscribe(sMonitorTopic.c_str(), 1);
    printMsg("订阅主题：" + sOtaTopic);
    printMsg("订阅主题：" + sNtpTopic);
    printMsg("订阅主题：" + sPropertyTopic);
    printMsg("订阅主题：" + sFunctionTopic);
    printMsg("订阅主题：" + sMonitorTopic);
    // 发布设备信息
    publishInfo();
  }
  else
  {
    printMsg("连接失败, rc=");
    Serial.print(mqttClient.state());
  }
}

// 生成密码
String generationPwd()
{
  String jsonTime = getTime();
  // 128字节内存池容量
  StaticJsonDocument<128> doc;
  // 解析JSON
  DeserializationError error = deserializeJson(doc, jsonTime);
  if (error)
  {
    printMsg("Json解析失败：");
    Serial.print(error.f_str());
    return "";
  }
  // 获取当前时间
  float deviceSendTime = doc["deviceSendTime"];
  float serverSendTime = doc["serverSendTime"];
  float serverRecvTime = doc["serverRecvTime"];
  float deviceRecvTime = millis();
  float now = (serverSendTime + serverRecvTime + deviceRecvTime - deviceSendTime) / 2;
  // 过期时间 = 当前时间 + 1小时
  float expireTime = now + 1 * 60 * 60 * 1000;
  String password = (String)mqttPwd + "&" + userId + "&" + String(expireTime, 0);
  printMsg("密码(未加密):" + password);
  return password;
}

//  HTTP获取时间
String getTime()
{
  while (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    printMsg("获取时间...");
    if (http.begin(wifiClient, (ntpServer + (String)millis()).c_str()))
    {
      // 发送请求
      int httpCode = http.GET();
      if (httpCode > 0)
      {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
          printMsg("获取时间成功，data:");
          Serial.print(http.getString());
          return http.getString();
        }
      }
      else
      {
        printMsg("获取时间失败，error:");
        Serial.printf(http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    else
    {
      printMsg("连接Http失败");
    }
    delay(500);
  }
}

//打印提示信息
void printMsg(String msg)
{
  Serial.print("\r\n[");
  Serial.print(millis());
  Serial.print("ms]");
  Serial.print(msg);
}

// 控制指示灯闪烁
void blink()
{
  pinMode(15, OUTPUT);
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(15, HIGH);
    delay(1000);
    digitalWrite(15, LOW);
    delay(1000);
  }
}

// 加密 (AES-CBC-128-pkcs5padding)
String encrypt(String plain_data, char *wumei_key, char *wumei_iv)
{
  int i;
  // pkcs7padding填充 Block Size : 16
  int len = plain_data.length();
  int n_blocks = len / 16 + 1;
  uint8_t n_padding = n_blocks * 16 - len;
  uint8_t data[n_blocks * 16];
  memcpy(data, plain_data.c_str(), len);
  for (i = len; i < n_blocks * 16; i++)
  {
    data[i] = n_padding;
  }
  uint8_t key[16], iv[16];
  memcpy(key, wumei_key, 16);
  memcpy(iv, wumei_iv, 16);
  // 加密
  br_aes_big_cbcenc_keys encCtx;
  br_aes_big_cbcenc_init(&encCtx, key, 16);
  br_aes_big_cbcenc_run(&encCtx, iv, data, n_blocks * 16);
  // Base64编码
  len = n_blocks * 16;
  char encoded_data[base64_enc_len(len)];
  base64_encode(encoded_data, (char *)data, len);
  return String(encoded_data);
}

// 解密 (AES-CBC-128-pkcs5padding)
String decrypt(String encoded_data_str, char *wumei_key, char *wumei_iv)
{
  int input_len = encoded_data_str.length();
  char *encoded_data = const_cast<char *>(encoded_data_str.c_str());
  int len = base64_dec_len(encoded_data, input_len);
  uint8_t data[len];
  base64_decode((char *)data, encoded_data, input_len);
  uint8_t key[16], iv[16];
  memcpy(key, wumei_key, 16);
  memcpy(iv, wumei_iv, 16);
  int n_blocks = len / 16;
  br_aes_big_cbcdec_keys decCtx;
  br_aes_big_cbcdec_init(&decCtx, key, 16);
  br_aes_big_cbcdec_run(&decCtx, iv, data, n_blocks * 16);
  // PKCS#7 Padding 填充
  uint8_t n_padding = data[n_blocks * 16 - 1];
  len = n_blocks * 16 - n_padding;
  char plain_data[len + 1];
  memcpy(plain_data, data, len);
  plain_data[len] = '\0';
  return String(plain_data);
}
