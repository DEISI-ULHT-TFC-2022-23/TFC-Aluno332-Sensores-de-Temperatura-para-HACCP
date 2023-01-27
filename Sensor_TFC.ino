// BIBLIOTECAS
// ---------------------------------------------------------------------------
#include <WiFi.h>
#include <PubSubClient.h> //MQTT
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "esp_adc_cal.h"


// DEFINIÇÃO DAS VARIAVEIS
// ---------------------------------------------------------------------------
#define POUCA_Bateria 3.50
#define MUITO_POUCA_Bateria 3.30
#define CRITICO_Bateria 3.20

uint64_t uS_TO_S_FACTOR = 1000000;  
uint64_t TIME_TO_SLEEP = 30;      
uint64_t SLEEP_MQTT = 60;  
uint64_t SLEEP_WIFI = 60;     
       

RTC_DATA_ATTR int reinicio = 0;
int wifi_ligacao = 0;
int mqtt_ligacao = 0;

float TEMP_C = 0;  
String Sensor_ID = "";

// WIFI
// ---------------------------------------------------------------------------
const char* ssid = "Cave da Gente";
const char* password = "0419272513";


// MQTT BROKER
// ---------------------------------------------------------------------------
const char *mqttTopico = "TFC/Esplanada/Sensor1";

const char *mqttServer = "192.168.1.97";
const char *mqttUser = "a20065449";
const char *mqttPass = "MQTT2022rrl";
uint16_t mqttPort = 1883;


WiFiClient espClient;
PubSubClient client(espClient);
#define ONE_WIRE_BUS 15 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);
DeviceAddress addrSensor;

StaticJsonBuffer<200> jsonBuffer; 
JsonObject& root = jsonBuffer.createObject(); 
char JSONmessageBuffer[200];

void conetar_mqtt() {
  while (!client.connected()){
    SensorROM(addrSensor);
    if (client.connect(Sensor_ID.c_str(),mqttUser,mqttPass)) {
      Temperatura(); 
      root["Sensor_ID"] = Sensor_ID;
      root["Temperatura"] = TEMP_C;
      root["Bateria"] = lerBateria();
      if(root["Bateria"] <= POUCA_Bateria && root["Bateria"] >= MUITO_POUCA_Bateria){  TIME_TO_SLEEP = 7200;    Serial.print("2 Horas de Espera: ");  }
      if(root["Bateria"] <= MUITO_POUCA_Bateria && root["Bateria"] >= CRITICO_Bateria){  TIME_TO_SLEEP = 14400; Serial.print("4 Horas de Espera: ");  }
      if(root["Bateria"] <= CRITICO_Bateria){  TIME_TO_SLEEP = 28800; Serial.print("6 Horas de Espera: ");  }
      root["Tipo"] = 1;
      root["Ativo"] = "S";
      root["ESP_IP"] = WiFi.localIP().toString();
      root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      Serial.print("PUB: ");  Serial.println(JSONmessageBuffer);
      bool publishSuccess = client.publish(mqttTopico, JSONmessageBuffer,true);
      if (publishSuccess){  
        Serial.println("Mensagem MQTT Publicada.");
      }
      if(client.subscribe(mqttTopico)){
        Serial.println("Topico Subscrito.");  
      } else {
        Serial.println("Falha na subscrição do Topico.");    
      }
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
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup(){
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  ++reinicio;
  Serial.println("Reinicio: " + String(reinicio));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (!client.connected()) {
    conetar_mqtt();
  }
  delay(100);
  client.loop();

  Serial.println("Tempo de Execução: " + String(millis()));
  Serial.flush();
  esp_deep_sleep_start();
}

void loop(void) {}
