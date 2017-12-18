#define PIN1 A0     //温度センサー
#define PIN2 5      //モーター用その①
#define PIN3 6      //モーター用その②
#define PWM1 7      //モーター用その③
#define PIN4 2      //LED用
#define MEAN 10     //移動平均を出すまでのサンプリング数を指定します。

#include <ArduinoJson.h>                                 
#include <Ethernet.h>
#include <PubSubClient.h>                                  // MQTTクライアントライブラリ

//移動平均用
int cnt = 0;
float temp[MEAN+1] = {};
boolean calibration_done = false;

//MACアドレスとMQTTブローカー設定
byte mac[]    = {  0xDE, 0xED, 0xBA, 0x01, 0x05, 0x61 };    // 自身のEtherernet Sheild の MACアドレス に変更
byte server[]   = { 123, 123, 123, 123 };                   // Mosquittoが実行されているPCのIPアドレス に変更

//送信用JSONの初期設定
DynamicJsonBuffer jsonBuffer;
char json[] =
  "{\"protocol\":\"1.0\",\"loginId\":arduino,\"name\":\"userXX\",\"template\":\"handson\",\"status\":[{\"enabled\":\"true\",\"values\":[{\"name\":\"温度\",\"type\":\"2\",\"value\":\"\"}]}]}";
JsonObject& root = jsonBuffer.parseObject(json);

void callback(char* topic, byte* payload, unsigned int length) {
  //メッセージ受信時にシリアルに表示する。
  Serial.println("message arrived!");

  //LEDをオンにする
  digitalWrite(PIN4, HIGH);

  //モーターをオンにする
  digitalWrite(PIN2, HIGH);
  digitalWrite(PIN3, LOW);
  analogWrite(PWM1, 200);

  //2秒待機してオフにする。
  delay(2000);
  digitalWrite(PIN4, LOW);
  analogWrite(PWM1, 0);
}

EthernetClient ethClient;           //イーサネットクライアントの初期化
PubSubClient client(ethClient);     //MQTTクライアントの初期化

//温度センサーの値を摂氏で取得
float get_temp() {
  int analogValue = analogRead(PIN1);
  float temp = ((analogValue * 5) / 1024.0) * 100;
  return temp;
}

void setup() {
  Serial.begin(9600);
  Serial.println("hello, arduino!");

  //MQTT送信用ユーザー名(JSON)
  root["name"] = "user20";

  pinMode(PIN2, OUTPUT);
  pinMode(PIN3, OUTPUT);
  pinMode(PWM1, OUTPUT);

  client.setServer(server, 1883);
  client.setCallback(callback);

  Ethernet.begin(mac);                            //イーサネットの開始
  if (client.connect("arduinoClient")) {           //クライアント名"arduinoClient"で接続。
    client.publish("outTopic", "hello, mqtt!");    //outTopicに対し、"hello, mqtt!"を送信
    client.subscribe("Fab/#");                     //"Fab/#"でサブスクライブを開始
  }
}

void loop() {
  
  //temp[MEAN-1]が0.00でない場合、キャリブレーションは完了していると判定しフラグを立てる
  if (temp[MEAN-1] != 0.00) {
    calibration_done = true;
  }

  //結果格納用変数
  float sum = 0.0;

  //cnt=MEAN、つまり配列の最後までいったらカウンタをゼロリセットする。
  if (cnt == MEAN) {
    cnt = 0;
  }
  temp[cnt] = get_temp();   //temp[cnt]に最新の温度を格納する
  cnt++;

  //配列の平均をとる。
  for (int i = 0; i < MEAN;i++) {
    sum += temp[i];
    //Serial.print(i);
    //Serial.print(":");
    //Serial.print(temp[i]);
    //Serial.print(" ");
  }
  float celsius = sum / MEAN;

  //キャリブレーションが未完了の場合、以降の処理をスキップする
  if (calibration_done == true) {
  //  if (cnt % 10 == 0) {        //送信頻度を10回に1回に制限する（調整可）
  //    Serial.print("Degree(C):   ");
  //    Serial.println(celsius);
      //Serial.print("temp: ");
      //Serial.println(get_temp());
      root["status"][0]["values"][0]["value"] = String(get_temp());   //JSONに温度をセット

      String payload = "";                                            //payloadを初期化
      root.printTo(payload);                                          //JSONの内容をpayloadに吐き出し
      int payload_length = payload.length() + 1;                      //payloadの長さ＋１
      //Serial.print("payload_length: ");
      //Serial.println(payload_length);
      Serial.println(payload);
      char payload_data[payload_length];                              //char型の配列を初期化
      payload.toCharArray(payload_data, payload_length);              //payloadをcharArrayに変換
      client.publish("Notify", payload_data);                         //MQTTブローカーへメッセージ送信  
  } else {
    Serial.println("calibrating... Please Wait");
  }
  
  delay(1000);                                                    //1秒待機
  client.loop();                                                  
}

