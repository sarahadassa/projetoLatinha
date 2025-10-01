#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Substitua com as suas credenciais da rede Wi-Fi
const char* ssid = "iPhonee";
const char* password = "vai12345";

// O pino 2 é agora o pino para todas as funções do LED.
#define LED_PIN 2

// Cria a instância do servidor web na porta 80
WebServer server(80);

// ==============================
// Estrutura da ÁRVORE BINÁRIA (ABB) e LISTA ENCADEADA
// (Mantidas do seu código original)
// ==============================
struct EventoNode {
    int id;
    int contadorLatinhas;
    EventoNode* left;
    EventoNode* right;

    EventoNode(int id, int total) 
        : id(id), contadorLatinhas(total), 
          left(nullptr), right(nullptr) {}
};

struct Deposito {
    int idEvento;
    unsigned long timestamp;
    Deposito* next;

    Deposito(int id, unsigned long tempo) 
        : idEvento(id), timestamp(tempo), next(nullptr) {}
};

class HistoricoDepositos {
private:
    EventoNode* root;
    Deposito* head;
    int proximoId;
    int totalLatinhas;
    
    // A implementação da função agora está aqui!
    EventoNode* inserirRec(EventoNode* node, int id, int total) {
        if (node == nullptr) {
            return new EventoNode(id, total);
        }
        if (id < node->id) {
            node->left = inserirRec(node->left, id, total);
        } else if (id > node->id) {
            node->right = inserirRec(node->right, id, total);
        }
        return node;
    }

    String inOrderRec(EventoNode* node) {
        String result = "";
        if (node != nullptr) {
            result += inOrderRec(node->left);
            result += "<li>Deposito n. " + String(node->id) + 
                       " | Total acumulado: " + String(node->contadorLatinhas) + "</li>";
            result += inOrderRec(node->right);
        }
        return result;
    }

public:
    HistoricoDepositos() : root(nullptr), head(nullptr), proximoId(1), totalLatinhas(0) {}

    void registrarDeposito() {
        totalLatinhas++;
        root = inserirRec(root, proximoId, totalLatinhas); 

        Deposito* novo = new Deposito(proximoId, millis());
        if (head == nullptr) {
            head = novo;
        } else {
            Deposito* temp = head;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = novo;
        }

        Serial.println("\nLatinha registrada com sucesso!");
        Serial.println("Total de latinhas acumuladas: " + String(totalLatinhas));
        proximoId++;
    }
    
    String listarEventosHTML() {
        if (root == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        return inOrderRec(root);
    }
    
    String listarHistoricoTemporalHTML() {
        if (head == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        String html = "";
        Deposito* temp = head;
        while (temp != nullptr) {
            html += "<li>Evento ID: " + String(temp->idEvento) + 
                    " | Timestamp: " + String(temp->timestamp) + " ms</li>";
            temp = temp->next;
        }
        return html;
    }

    int getTotalLatinhas() {
      return totalLatinhas;
    }
};

HistoricoDepositos historico;

// Variável para controlar o estado do LED sem usar delay()
unsigned long ledTime = 0;
bool ledState = false;

// ==============================
// Funções de manipulação do servidor (handlers)
// ==============================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>AluminiTech</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background-color: #f0f0f0; }
  .container { padding: 20px; }
  h1 { color: #333; }
  .info-box { background-color: #fff; border-radius: 8px; padding: 15px; margin-top: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  .status { font-size: 1.2em; color: #555; }
  a {
    display: inline-block;
    padding: 15px 30px;
    margin: 10px;
    text-decoration: none;
    color: #fff;
    background-color: #28a745;
    border-radius: 5px;
    font-size: 1.2em;
    cursor: pointer;
  }
</style>
</head>
<body>
  <div class="container">
    <h1>Controle do Protótipo AluminiTech</h1>
    <div class="info-box">
      <p class="status">Status: Conectado</p>
      <p class="status">Endereço IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
      <p class="status">Total de latinhas: )rawliteral" + String(historico.getTotalLatinhas()) + R"rawliteral(</p>
    </div>
    <a href="/detectar">Simular Deposito de Latinha</a>
    <a href="/historico">Ver Histórico</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleDetect() {
    historico.registrarDeposito();
    ledState = true;
    ledTime = millis();
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

void handleHistorico() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Historico de Depositos</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background-color: #f0f0f0; }
  .container { padding: 20px; }
  h1 { color: #333; }
  ul { list-style-type: none; padding: 0; }
  li { background-color: #fff; border-radius: 5px; padding: 10px; margin: 5px 0; }
  a {
    display: inline-block;
    padding: 10px 20px;
    margin-top: 20px;
    text-decoration: none;
    color: #fff;
    background-color: #007BFF;
    border-radius: 5px;
  }
</style>
</head>
<body>
  <div class="container">
    <h1>Historico de Depositos</h1>
    <h3>Arvore Binaria de Busca (ABB)</h3>
    <ul>)rawliteral" + historico.listarEventosHTML() + R"rawliteral(</ul>
    <h3>Lista Encadeada (Historico Temporal)</h3>
    <ul>)rawliteral" + historico.listarHistoricoTemporalHTML() + R"rawliteral(</ul>
    <a href="/">Voltar</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}


// ==============================
// Setup inicial
// ==============================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    pinMode(LED_PIN, OUTPUT);

    // Sinal de início
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);

    // Conexão WiFi
    WiFi.begin(ssid, password);
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 15) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
        Serial.println("\nWiFi Conectado!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFalha no WiFi - Modo Offline");
    }

    server.on("/", handleRoot);
    server.on("/detectar", handleDetect);
    server.on("/historico", handleHistorico);

    server.begin();
    Serial.println("\nServidor web iniciado.");
}

// ==============================
// Loop principal
// ==============================
void loop() {
    server.handleClient();
    
    // Lógica para piscar o LED de forma não-bloqueante
    if (ledState && (millis() - ledTime > 500)) {
        digitalWrite(LED_PIN, LOW);
        ledState = false;
    }
}
