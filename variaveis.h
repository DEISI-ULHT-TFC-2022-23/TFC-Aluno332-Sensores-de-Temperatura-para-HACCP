// DEFINIÇÃO DAS VARIAVEIS
// ---------------------------------------------------------------------------
#define POUCA_Bateria 3.55
#define MUITO_POUCA_Bateria 3.45
#define CRITICO_Bateria 3.35

uint64_t uS_TO_S_FACTOR = 1000000;  
uint64_t TIME_TO_SLEEP = 60;      
uint64_t SLEEP_MQTT = 60;  
uint64_t SLEEP_WIFI = 60;     
// WIFI
// ---------------------------------------------------------------------------
const char* ssid = "Cave da Gente";
const char* password = "0419272513";

// MQTT BROKER
// ---------------------------------------------------------------------------
const char *mqttTopico = "TFC/Esplanada/Sensor2";
const char *mqttTopico2 = "TFC/Esplanada/Sensor2/S2"; 
const char *mqttServer = "192.168.1.97";
const char *mqttUser = "a20065449";
const char *mqttPass = "MQTT2022rrl";
uint16_t mqttPort = 1883;
