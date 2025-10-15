#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <map>

// Substitua com as suas credenciais da rede
const char* ssid = "<nomeDaRede>";
const char* password = "<precisoFalarOqueÉ?>";

#define LED_PIN 2

// Cria a instancia do servidor web na porta 80
WebServer server(80);

// Estrutura da ABB e da lista encadeda
struct EventoNode {
    int id;
    String lixeiraID;
    int contadorLatinhas;
    EventoNode* left;
    EventoNode* right;

    EventoNode(int id, String lixID, int total) 
        : id(id), lixeiraID(lixID), contadorLatinhas(total), 
          left(nullptr), right(nullptr) {}
};

struct Deposito {
    int idEvento;
    String lixeiraID;
    unsigned long timestamp;
    Deposito* next;

    Deposito(int id, String lixID, unsigned long tempo) 
        : idEvento(id), lixeiraID(lixID), timestamp(tempo), next(nullptr) {}
};

class HistoricoDepositos {
private:
    EventoNode* root;
    Deposito* head;
    int proximoId;
    std::map<String, int> contadoresLixeiras;
    String ultimaLixeiraID; 
    
    EventoNode* inserirRec(EventoNode* node, int id, String lixID, int total) {
        if (node == nullptr) {
            return new EventoNode(id, lixID, total);
        }
        if (id < node->id) {
            node->left = inserirRec(node->left, id, lixID, total);
        } else if (id > node->id) {
            node->right = inserirRec(node->right, id, lixID, total);
        }
        return node;
    }

    String inOrderRec(EventoNode* node, String filterID) {
        String result = "";
        if (node != nullptr) {
            result += inOrderRec(node->left, filterID);
            if (filterID.length() == 0 || node->lixeiraID == filterID) {
                result += "<li>Deposito n. " + String(node->id) + " | Lixeira: " + String(node->lixeiraID) + 
                          "</li>";
            }
            result += inOrderRec(node->right, filterID);
        }
        return result;
    }

public:
    HistoricoDepositos() : root(nullptr), head(nullptr), proximoId(1) {}

    void registrarDeposito(String lixeiraID) {
        if (contadoresLixeiras.find(lixeiraID) == contadoresLixeiras.end()) {
            contadoresLixeiras[lixeiraID] = 0;
        }
        contadoresLixeiras[lixeiraID]++;
        int totalAtual = contadoresLixeiras[lixeiraID];
        
        root = inserirRec(root, proximoId, lixeiraID, totalAtual); 

        Deposito* novo = new Deposito(proximoId, lixeiraID, millis());
        if (head == nullptr) {
            head = novo;
        } else {
            Deposito* temp = head;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = novo;
        }

        ultimaLixeiraID = lixeiraID;

        Serial.println("\nLatinha registrada na Lixeira " + lixeiraID + " com sucesso!");
        Serial.println("Total de latinhas na Lixeira " + lixeiraID + ": " + String(totalAtual));
        proximoId++;
    }
    
    String listarEventosHTML(String filterID) {
        if (root == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        return inOrderRec(root, filterID);
    }
    
    String listarHistoricoTemporalHTML(String filterID) {
        if (head == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        String html = "";
        Deposito* temp = head;
        while (temp != nullptr) {
            if (filterID.length() == 0 || temp->lixeiraID == filterID) {
                html += "<li>Evento ID: " + String(temp->idEvento) + " | Lixeira: " + String(temp->lixeiraID) +
                        " | Timestamp: " + String(temp->timestamp) + " ms</li>";
            }
            temp = temp->next;
        }
        return html;
    }

    String getContadoresHTML() {
        String html = "";
        if (ultimaLixeiraID.length() > 0) {
          html += "<p class='status' style='font-weight:bold; color: #007BFF;'>Ultima Lixeira Usada: " + ultimaLixeiraID + "</p>";
        }
        for (auto const& [lixeiraID, total] : contadoresLixeiras) {
            html += "<p class='status'>Lixeira " + lixeiraID + ": " + String(total) + " latinhas</p>";
        }
        return html;
    }
    
    int getContadorLixeira(String lixeiraID) {
        if (contadoresLixeiras.count(lixeiraID)) {
            return contadoresLixeiras[lixeiraID];
        }
        return 0;
    }
};

HistoricoDepositos historico;

// Funcoes do servidor
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>AluminiTech - Dashboard</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background-color: #f0f0f0; }
  .container { padding: 20px; }
  h1 { color: #333; }
  h2 { color: #555; margin-top: 30px; }
  .info-box { background-color: #fff; border-radius: 8px; padding: 15px; margin-top: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  .status { font-size: 1.2em; color: #555; }
  .btn {
    display: inline-block;
    padding: 15px 30px;
    margin: 10px;
    text-decoration: none;
    color: #fff;
    background-color: #007BFF; /* Azul para navegacao */
    border-radius: 5px;
    font-size: 1.2em;
    cursor: pointer;
  }
</style>
</head>
<body>
  <div class="container">
    <h1>Dashboard de Gerenciamento</h1>
    <div class="info-box">
      <p class="status">Status do Sistema: ONLINE</p>
      <p class="status">Endereco IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
      )rawliteral" + historico.getContadoresHTML() + R"rawliteral(
    </div>
    <h2>Acessar Lixeiras Individuais</h2>
    <a href="/lixeiraA" class="btn">Lixeira A</a>
    <a href="/lixeiraB" class="btn">Lixeira B</a>
    <a href="/lixeiraC" class="btn">Lixeira C</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// Funcao unica para gerar a pagina de controle da lixeira
void handleLixeira(String lixeiraID) {
    int totalLatinhas = historico.getContadorLixeira(lixeiraID);
    
    String statusCheio = (totalLatinhas >= 10) ? "<p style='color:red; font-weight:bold;'>ESTADO: CHEIO - COLETAR!</p>" : "<p style='color:green; font-weight:bold;'>ESTADO: DISPONIVEL</p>";

    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Controle Lixeira )rawliteral" + lixeiraID + R"rawliteral(</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background-color: #f0f0f0; }
  .container { padding: 20px; }
  h1 { color: #333; }
  h3 { color: #555; }
  .info-box { background-color: #fff; border-radius: 8px; padding: 15px; margin-top: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  .status { font-size: 1.2em; color: #555; }
  .btn {
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
  .btn.blue { background-color: #007BFF; }
</style>
</head>
<body>
  <div class="container">
    <h1>Controle - Lixeira )rawliteral" + lixeiraID + R"rawliteral(</h1>
    <div class="info-box">
      )rawliteral" + statusCheio + R"rawliteral(
      <p class="status">Total de Latinhas: )rawliteral" + String(totalLatinhas) + R"rawliteral(</p>
    </div>
    
    <h2>Simulacao e Acoes</h2>
    <a href="/depositar?lixeira=)rawliteral" + lixeiraID + R"rawliteral(" class="btn">SIMULAR DEPOSITO</a>
    <a href="/historico?lixeira=)rawliteral" + lixeiraID + R"rawliteral(" class="btn blue">VER HISTORICO</a>

    <h2>Acionamentos do Atuador</h2>
    <p class="status">Neste espaco, futuramente, teremos os controles do Servo Motor.</p>

    <a href="/" class="btn blue">Voltar ao Dashboard</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleLixeiraA() { handleLixeira("A"); }
void handleLixeiraB() { handleLixeira("B"); }
void handleLixeiraC() { handleLixeira("C"); }


void handleDepositar() {
    String lixeiraID = server.arg("lixeira");
    if (lixeiraID.length() > 0) {
        historico.registrarDeposito(lixeiraID);
        
        digitalWrite(LED_PIN, HIGH);
        delay(200); 
        digitalWrite(LED_PIN, LOW);
    }
    
    server.sendHeader("Location", "/lixeira" + lixeiraID); 
    server.send(302, "text/plain", "");
}

void handleHistorico() {
  String lixeiraID = server.arg("lixeira");
  String historicoABB = historico.listarEventosHTML(lixeiraID);
  String historicoLista = historico.listarHistoricoTemporalHTML(lixeiraID);

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
  h3 { color: #555; }
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
    <h1>Historico da Lixeira )rawliteral" + lixeiraID + R"rawliteral(</h1>
    <h3>Arvore Binaria de Busca (ABB)</h3>
    <ul>)rawliteral" + historicoABB + R"rawliteral(</ul>
    <h3>Lista Encadeada (Historico Temporal)</h3>
    <ul>)rawliteral" + historicoLista + R"rawliteral(</ul>
    <a href="/lixeira)rawliteral" + lixeiraID + R"rawliteral(">Voltar ao Controle da Lixeira</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    pinMode(LED_PIN, OUTPUT);

    Serial.println("Sinalizando inicio...");
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);

    WiFi.begin(ssid, password);
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 15) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {

        Serial.println("\nWiFi Conectado!");
        Serial.println("Sinalizando conexao...");
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(300);
            digitalWrite(LED_PIN, LOW);
            delay(300);
        }

        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFalha no WiFi - Modo Offline");
    }
    
    // Configuracao das rotas
    server.on("/", handleRoot);
    server.on("/lixeiraA", handleLixeiraA);
    server.on("/lixeiraB", handleLixeiraB);
    server.on("/lixeiraC", handleLixeiraC);
    server.on("/depositar", handleDepositar);
    server.on("/historico", handleHistorico);

    server.begin();
    Serial.println("\nServidor web iniciado.");
}

void loop() {
    server.handleClient();
}
