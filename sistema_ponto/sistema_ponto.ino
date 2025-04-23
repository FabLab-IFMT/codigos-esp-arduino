#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Definições para o RDM6300
#define RDM6300_RX_PIN 17 // Pino RX do ESP32 conectado ao TX do RDM6300
#define RDM6300_TX_PIN 16 // Pino TX do ESP32 conectado ao RX do RDM6300
// Use HardwareSerial em vez de SoftwareSerial
HardwareSerial rdm6300Serial(2); // Use UART2 do ESP32
const int MAX_TAG_LEN = 14; // Tamanho máximo da tag
char tagBuffer[MAX_TAG_LEN]; // Buffer para armazenar a tag
int tagIndex = 0;
unsigned long lastTagTime = 0;
const unsigned long TAG_TIMEOUT = 3000; // 3 segundos entre leituras

// Configuração do WiFi
const char* ssid = "Fabnet";
const char* password = "71037523";

// Configuração da API
const char* serverUrl = "http://192.168.1.103:8000";
String endpoint = "/acesso_e_ponto/verificar_cartao/";

// LED para indicação (opcional)
#define LED_PIN 2

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema de ponto...");
  
  // Inicializa LED para indicação
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Inicializa o leitor RFID RDM6300 com a UART2
  rdm6300Serial.begin(9600, SERIAL_8N1, RDM6300_RX_PIN, RDM6300_TX_PIN);
  Serial.println("Leitor RFID RDM6300 iniciado");
  
  // Conexão WiFi
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado");
  Serial.println("Aproxime o cartão");
}

void loop() {
  // Verificar se um cartão foi lido
  if (rdm6300Serial.available() > 0) {
    char c = rdm6300Serial.read();
    
    // Se encontramos o início da tag
    if (c == 2) { 
      tagIndex = 0;
    } 
    // Se encontramos o fim da tag
    else if (c == 3) {
      // Termina a string e remove os 2 últimos caracteres (checksum)
      if (tagIndex > 2) { // Certifique-se de que há pelo menos 3 caracteres
        tagIndex -= 2; // Elimina os 2 últimos caracteres (checksum)
      }
      tagBuffer[tagIndex] = '\0'; // Termina a string
      
      // Verifica se passou tempo suficiente desde a última leitura
      if (millis() - lastTagTime > TAG_TIMEOUT) {
        String cardNumber = String(tagBuffer);
        Serial.print("Cartão detectado: ");
        Serial.println(cardNumber);
        
        // Pisca o LED
        digitalWrite(LED_PIN, HIGH);
        
        // Registra o ponto
        registrarPonto(cardNumber);
        
        lastTagTime = millis();
        digitalWrite(LED_PIN, LOW);
      }
    } 
    // Acumula os caracteres da tag (apenas dígitos hexadecimais)
    else if (tagIndex < MAX_TAG_LEN - 1 && ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
      tagBuffer[tagIndex++] = c;
    }
  }
  
  delay(10);
}

void registrarPonto(String cardNumber) {
  // Verificar conexão WiFi
  if (WiFi.status() == WL_CONNECTED) {
    // Construir a URL completa
    String fullUrl = String(serverUrl) + endpoint;
    
    // Preparar JSON
    String jsonPayload = "{\"card_number\":\"" + cardNumber + "\"}";
    
    // Mostrar a URL completa e o payload que será enviado
    Serial.println("\n------ Detalhes da requisição ------");
    Serial.println("URL da API: " + fullUrl);
    Serial.println("JSON enviado: " + jsonPayload);
    Serial.println("Equivalente a: curl -X POST " + fullUrl + " -H \"Content-Type: application/json\" -d '" + jsonPayload + "'");
    Serial.println("------------------------------------\n");
    
    HTTPClient http;
    http.begin(fullUrl);
    http.addHeader("Content-Type", "application/json");
    
    Serial.println("Enviando dados para API...");
    
    // Enviar requisição
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Resposta do servidor:");
      Serial.println(response);
      
      // Parsear resposta JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      
      bool authorized = doc["authorized"];
      String displayMessage = doc["display_message"];
      
      if (authorized) {
        Serial.println("Acesso autorizado: " + displayMessage);
        // Pisca o LED 3 vezes para indicar sucesso
        for(int i=0; i<3; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);
          delay(200);
        }
      } else {
        Serial.println("Acesso negado: Cartão inválido");
        // Pisca o LED rapidamente para indicar erro
        for(int i=0; i<5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
        }
      }
    }
    else {
      Serial.print("Erro na conexão. Código HTTP: ");
      Serial.println(httpResponseCode);
      // Pisca o LED para indicar erro de conexão
      for(int i=0; i<2; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
      }
    }
    
    http.end();
  } else {
    Serial.println("WiFi Desconectado. Reconectando...");
    WiFi.begin(ssid, password);
  }
}
