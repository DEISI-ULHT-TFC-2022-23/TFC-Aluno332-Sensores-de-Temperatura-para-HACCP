// BIBLIOTECAS
// ---------------------------------------------------------------------------
#include <WiFi.h>
#include <PubSubClient.h> //MQTT
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "esp_adc_cal.h"
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp32fota.h>
#include "variaveis.h"

// DEFINIÇÃO DAS VARIAVEIS
// ---------------------------------------------------------------------------
       
RTC_DATA_ATTR int reinicio = 0;
RTC_DATA_ATTR char *Ativo= "S";
int wifi_ligacao = 0;
int mqtt_ligacao = 0;

float TEMP_C = 0;  
String Sensor_ID = "";

WiFiClient espClient;
PubSubClient client(espClient);
#define ONE_WIRE_BUS 15 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);
DeviceAddress addrSensor;

DynamicJsonDocument doc(256);
JsonObject obj = doc.to<JsonObject>();
char buffer[256];

#define OTA_Versao 1
esp32FOTA esp32FOTA("esp32-fota-http", OTA_Versao, false, true);
const char* manifest_url = "https://www.sensores.northspot.pt/Esplanada/Sensor_2/Esplanada_2.json";

//CryptoFileAsset *MyRootCA = new CryptoFileAsset( "/root_ca.pem", &SPIFFS );

// CONETAR MQTT
// ---------------------------------------------------------------------------
void conetar_mqtt() {
  while (!client.connected()){
    SensorROM(addrSensor);
    if (client.connect(Sensor_ID.c_str(),mqttUser,mqttPass)) {
      Temperatura(); 
      obj["Sensor_ID"] = Sensor_ID;
      obj["Temperatura"] = TEMP_C;
      obj["Bateria"] = lerBateria();
      obj["WIFI"] = WiFi.RSSI();
      // Espera de 2 Horas
      if(obj["Bateria"] <= POUCA_Bateria && obj["Bateria"] >= MUITO_POUCA_Bateria){    TIME_TO_SLEEP = 7200;    
      }
      // Espera de 3 Horas
      if(obj["Bateria"] <= MUITO_POUCA_Bateria && obj["Bateria"] >= CRITICO_Bateria){  TIME_TO_SLEEP = 10800;    
      }
      // Espera de 4 Horas 
      if(obj["Bateria"] <= CRITICO_Bateria){    TIME_TO_SLEEP = 14400; 
      }

      obj["Tipo"] = 2;
      obj["Ativo"] = Ativo;
      obj["ESP_IP"] = WiFi.localIP().toString();
      obj["VERSAO"] = OTA_Versao;
      obj["Nome"] = "Esplanada - Arca dos Gelados";
      
      size_t n = serializeJson(doc, buffer);
      Serial.print("PUB: ");  Serial.println(buffer);
      bool publishSuccess = client.publish(mqttTopico, buffer, true);
      if (publishSuccess){  
        Serial.println("Mensagem MQTT Publicada.");
      }
      if(client.subscribe(mqttTopico2)){ Serial.println("Topico Subscrito.");} 
      else {  Serial.println("Falha na subscrição do Topico.");}
    }  
    else {
      ++mqtt_ligacao;
      // SE não conseguir conetar com o MQTT dorme por x minutos
      if(mqtt_ligacao > 10){
        Serial.println("Falha ao Conetar MQTT");
        esp_sleep_enable_timer_wakeup(SLEEP_MQTT * uS_TO_S_FACTOR);
        Serial.flush();
        esp_deep_sleep_start();
      }
      delay(500);
      Serial.print(".");
    }
  }
}


// WIFI
// ---------------------------------------------------------------------------
void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    ++wifi_ligacao;
    // SE não conseguir conetar com o Wifi, desliga
    if(wifi_ligacao > 10){
      Serial.println("Falha ao Conetar Wifi");
      esp_sleep_enable_timer_wakeup(SLEEP_WIFI * uS_TO_S_FACTOR);
      Serial.flush();
      esp_deep_sleep_start();
    }
    delay(500);
    Serial.print(".");
  }
  wifi_ligacao=0;
  randomSeed(micros());
}

// FUNCAO PARA LER TEMPERATURA 
// ---------------------------------------------------------------------------
void Temperatura(void){ 
  delay(750);
  sensor.requestTemperatures(); 
  TEMP_C = sensor.getTempCByIndex(0);
}

// FUNCAO PARA OBTER ROM DO SENSOR 
// ---------------------------------------------------------------------------
void SensorROM(DeviceAddress addr){

  sensor.begin();
  if (!sensor.getAddress(addrSensor, 0)) 
    Serial.println("Sensor Não Encontrado..."); 
  
  for (uint8_t i = 0; i < 8; i++){
    Sensor_ID += String(addr[i], HEX);
  }  
}

// LER BATERIA
// ---------------------------------------------------------------------------
float lerBateria() {
  uint32_t valor = 0;
  int rounds = 11;
  esp_adc_cal_characteristics_t adc_chars;

  //battery voltage divided by 2 can be measured at GPIO34, which equals ADC1_CHANNEL6
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 2000, &adc_chars);
 
  for(int i=1; i<=rounds; i++) {
    valor += adc1_get_raw(ADC1_CHANNEL_6);
  }
  valor /= (uint32_t)rounds;
  return esp_adc_cal_raw_to_voltage(valor, &adc_chars)*2.0/1000.0;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  Serial.print("Message recebida [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == mqttTopico2){
    if(messageTemp == "true"){    Ativo = "S";}
    if(messageTemp == "false"){   Ativo = "N";}
  }
}

void verifica_versao(){
  esp32FOTA.setManifestURL( manifest_url );
  //esp32FOTA.setRootCA( MyRootCA );
  esp32FOTA.printConfig();
  bool updatedNeeded = esp32FOTA.execHTTPcheck();
  if (updatedNeeded){
    Serial.println("Entrei no setup.");
    esp32FOTA.execOTA();
  }
  else{
    Serial.println("Não Entrei no setup.");
  }
}

void setup(){
  Serial.begin(115200);
  ++reinicio;
  Serial.println("Reinicio: " + String(reinicio));

  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback); 

  verifica_versao();
  
  if (!client.connected()) {
    conetar_mqtt();
  }
  //Ativo = 
  for(int i=0;i<10; i++){
   client.loop();
   delay(100);
  }
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Tempo de Execução: " + String(millis()));
  Serial.flush();
  esp_deep_sleep_start();
}

void loop(void) {}
