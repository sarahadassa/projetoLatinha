#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <map>
#include <time.h>
#include <ESP32Servo.h>
#include <algorithm>
#include <vector> 

// --- CONFIGURACOES GLOBAIS ---

// Credenciais da rede Wi-Fi para o ESP32 se conectar.
const char* ssid = "TesteESP32";
const char* password = "12345678";

// Mapeamento dos pinos de hardware do ESP32.
#define LED_VERDE_STATUS 18    // LED Verde: indica que a lixeira está disponível para uso.
#define LED_AMARELO_ACAO 19    // LED Amarelo: pisca para dar feedback visual de uma ação (depósito).
#define LED_VERMELHO_LOTADO 21 // LED Vermelho: indica que a lixeira atingiu sua capacidade máxima.
#define LED_PIN_SETUP 2        // LED azul interno do ESP32: usado para sinalizar o status da conexão Wi-Fi.
#define SENSOR_PIN 26          // Pino de entrada para o Sensor Indutivo, que detecta a latinha.
#define SERVO_PIN 23           // Pino de saída PWM para controlar o Servo Motor da tampa.

// Parâmetro de negócio: define a capacidade máxima de latinhas.
#define LIMITE_LATINHAS 5

// Constantes para o controle do Servo Motor.
const int POSICAO_FECHADA = 0;   // Ângulo em graus para a tampa fechada.
const int POSICAO_ABERTA = 90;   // Ângulo em graus para a tampa aberta.
const int TEMPO_MOVIMENTO = 500; // Delay para garantir que o servo complete seu movimento.

// --- VARIAVEIS DE ESTADO DO SISTEMA ---

// Variáveis para o tratamento de "debouncing" do sensor, evitando leituras falsas.
int sensorEstadoAtual = LOW;
int sensorEstadoAnterior = LOW;
unsigned long sensorUltimoTempoMuda = 0;
const long debounceDelay = 50;
bool latinhaFoiRegistrada = false; // Flag para garantir que uma única detecção gere apenas um registro.

// Variável de estado que armazena qual lixeira (A, B, ou C) está sendo visualizada no navegador.
// É crucial para direcionar a lógica do sensor e dos LEDs para a lixeira correta.
String lixeiraAtualEmFoco = "";

// Instância do servidor web que atenderá as requisições HTTP na porta 80.
WebServer server(80);

// Instância do objeto para controle do Servo Motor.
Servo servoMotor;

// --- ESTRUTURAS DE DADOS E CLASSE DE GERENCIAMENTO ---

// Nó da Árvore Binária de Busca (ABB). Armazena eventos de depósito.
// A ABB permite uma busca e listagem eficiente dos eventos em ordem de ID.
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

// Nó da Lista Encadeada. Armazena o histórico de depósitos em ordem cronológica.
// A Lista Encadeada é ideal para registrar a sequência temporal dos eventos.
struct Deposito {
    int idEvento;
    String lixeiraID;
    String dataHora;
    Deposito* next; 
    Deposito(int id, String lixID, String dataHoraStr) 
        : idEvento(id), lixeiraID(lixID), dataHora(dataHoraStr), next(nullptr) {}
};

// Estrutura auxiliar usada para ordenar as lixeiras no dashboard.
// Facilita a classificação com base no status (lotada) e na contagem.
struct DadosLixeira {
    String id;
    int contagem;
    bool lotada;
};

bool compararLixeiras(const DadosLixeira& a, const DadosLixeira& b) {
    if (a.lotada != b.lotada) {
        return a.lotada;
    }
    return a.contagem > b.contagem;
}


// Classe principal que encapsula a lógica de negócio e as estruturas de dados.
class HistoricoDepositos {
private:
    EventoNode* root; // Raiz da Árvore Binária de Busca (ABB) de eventos.
    Deposito* head;   // Início (cabeça) da Lista Encadeada de histórico temporal.
    int proximoId;    // Contador para gerar IDs únicos para cada evento de depósito.

    // Um mapa para armazenar a contagem de latinhas por lixeira (ID "A", "B", "C").
    // Oferece acesso rápido (O(log n)) à contagem de qualquer lixeira.
    std::map<String, int> contadoresLixeiras;
    String ultimaLixeiraID; // Armazena o ID da última lixeira que recebeu um depósito.
    
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
    
    // Obtém a data e hora atuais a partir de um servidor NTP (Network Time Protocol).
    // Possui um fallback para um tempo baseado em millis() caso a sincronização NTP falhe.
    String obterDataHora() {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char buffer[25];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
            return String(buffer);
        } else { // Fallback caso a sincronização de hora falhe.
            unsigned long tempoAtual = millis();
            int segundos = (tempoAtual / 1000) % 60;
            int minutos = (tempoAtual / 60000) % 60;
            int horas = (tempoAtual / 3600000) % 24;
            int dias = (tempoAtual / 86400000) % 30 + 1;
            
            return String(dias) + "/12/2024 " + 
                       String(horas < 10 ? "0" : "") + String(horas) + ":" +
                       String(minutos < 10 ? "0" : "") + String(minutos) + ":" +
                       String(segundos < 10 ? "0" : "") + String(segundos);
        }
    }

    // Função central que registra um novo depósito.
    // Atualiza o contador, insere na ABB, adiciona à Lista Encadeada e atualiza o estado.
    void registrarDeposito(String lixeiraID) {
        if (contadoresLixeiras.find(lixeiraID) == contadoresLixeiras.end()) {
            contadoresLixeiras[lixeiraID] = 0;
        }
        contadoresLixeiras[lixeiraID]++;
        int totalAtual = contadoresLixeiras[lixeiraID];
        
        // Insere o novo evento na Árvore Binária de Busca.
        root = inserirRec(root, proximoId, lixeiraID, totalAtual); 

        // Adiciona o mesmo evento ao final da Lista Encadeada para manter a ordem cronológica.
        String dataHoraAtual = obterDataHora();
        Deposito* novo = new Deposito(proximoId, lixeiraID, dataHoraAtual);
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
    
    // Gera uma lista HTML dos eventos percorrendo a ABB em ordem (in-order).
    String listarEventosHTML(String filterID) {
        if (root == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        return inOrderRec(root, filterID);
    }
    
    // Gera uma lista HTML dos eventos percorrendo a Lista Encadeada do início ao fim.
    String listarHistoricoTemporalHTML(String filterID) {
        if (head == nullptr) {
            return "Nenhum deposito registrado ainda.";
        }
        String html = "";
        Deposito* temp = head;
        while (temp != nullptr) {
            if (filterID.length() == 0 || temp->lixeiraID == filterID) {
                html += "<li>Evento ID: " + String(temp->idEvento) + " | Lixeira: " + String(temp->lixeiraID) +
                        " | Data/Hora: " + temp->dataHora + "</li>";
            }
            temp = temp->next;
        }
        return html;
    }

    // Gera o HTML para o painel de status das lixeiras no dashboard principal.
    String getContadoresHTML() {
        String html = "";
        if (ultimaLixeiraID.length() > 0) {
          html += "<p class='status' style='font-weight:bold; color: #007BFF;'>Ultima Lixeira Usada: " + ultimaLixeiraID + "</p>";
        }
        
        // Lógica de Ordenação do Dashboard:
        // 1. Popula um vetor com os dados atuais de cada lixeira.
        // 2. Ordena o vetor usando uma função de comparação customizada (`compararLixeiras`).
        //    A regra é: lixeiras lotadas primeiro, depois por maior número de latinhas.
        // 3. Gera o HTML a partir da lista já ordenada.
        std::vector<DadosLixeira> listaOrdenada;
        String lixeiras[] = {"A", "B", "C"};
        
        for (const String& id : lixeiras) {
            int total = getContadorLixeira(id);
            listaOrdenada.push_back({id, total, isLixeiraLotada(id)});
        }
        std::sort(listaOrdenada.begin(), listaOrdenada.end(), compararLixeiras);

        for (const auto& dados : listaOrdenada) {
            String cor = dados.lotada ? "#dc3545" : (dados.contagem > 0 ? "#ffc107" : "#28a745");
            String status = dados.lotada ? "LOTADA" : (dados.contagem > 0 ? "EM USO" : "VAZIA");

            html += "<div class='lixeira-status' style='background-color: " + cor + "; color: white; padding: 10px; margin: 5px; border-radius: 8px; display: inline-block; min-width: 120px;'>";
            html += "<strong> Lixeira " + dados.id + "</strong><br>";
            html += "Status: " + status + "<br>";
            html += "Latinhas: " + String(dados.contagem) + "/" + String(LIMITE_LATINHAS);
            html += "</div>";
        }

        return html;
    }
    
    // Retorna a contagem de latinhas para uma lixeira específica.
    int getContadorLixeira(String lixeiraID) {
        if (contadoresLixeiras.count(lixeiraID)) {
            return contadoresLixeiras[lixeiraID];
        }
        return 0;
    }
    
    // Verifica se uma lixeira está lotada comparando sua contagem com o limite.
    bool isLixeiraLotada(String lixeiraID) {
        return getContadorLixeira(lixeiraID) >= LIMITE_LATINHAS;
    }
    
    String getStatusLixeirasHTML() {
        return ""; // A informacao ja sera gerada no getContadoresHTML()
    }
};

HistoricoDepositos historico;

// --- FUNCOES DE CONTROLE DE HARDWARE (LED, SERVO, SENSOR) ---

// Controla o Servo Motor para mover a tampa para um ângulo específico.
void controlarTampaServo(int angulo) {
    servoMotor.write(angulo);
    delay(TEMPO_MOVIMENTO); // Pausa para garantir que o servo complete o movimento.
}


// Função de pisca não-bloqueante para o LED vermelho.
// Executada continuamente no loop(), ela só piscará o LED se a lixeira em foco estiver lotada.
void loopPiscaVermelho() {
    // Condição: só pisca se uma lixeira específica estiver em foco e lotada.
    if (lixeiraAtualEmFoco.length() > 0 && historico.isLixeiraLotada(lixeiraAtualEmFoco)) {
        const int intervalo = 500; // Pisca a cada meio segundo
        static unsigned long proximoMuda = 0;
        
        if (millis() >= proximoMuda) {
            // Alterna o estado do LED Vermelho
            digitalWrite(LED_VERMELHO_LOTADO, !digitalRead(LED_VERMELHO_LOTADO));
            proximoMuda = millis() + intervalo;
        }
    } else {
        // Garante que o LED vermelho permaneça desligado se a condição não for atendida.
        digitalWrite(LED_VERMELHO_LOTADO, LOW);
    }
}

// Controla o estado do LED verde (status de disponibilidade).
void setVerdeStatusLED(bool estado) {
    digitalWrite(LED_VERDE_STATUS, estado ? HIGH : LOW);
}

// Pisca o LED amarelo uma vez para indicar que uma ação (depósito) ocorreu.
void piscaAmareloAcao() {
    digitalWrite(LED_AMARELO_ACAO, HIGH);
    delay(100); 
    digitalWrite(LED_AMARELO_ACAO, LOW);
}


// Função principal de leitura do sensor com lógica de debouncing.
// Esta função é chamada continuamente no loop principal.
void loopSensor() {
    // O sensor só deve acionar o sistema se o usuário estiver na página de uma lixeira específica.
    if (lixeiraAtualEmFoco.length() == 0) {
        return;
    }
    
    // Implementação do debouncing para evitar ruídos elétricos no sensor.
    int leitura = digitalRead(SENSOR_PIN);
    if (leitura != sensorEstadoAnterior) {
        sensorUltimoTempoMuda = millis();
    }
    
    if ((millis() - sensorUltimoTempoMuda) > debounceDelay) {
        // Confirma que o estado do sensor mudou e está estável.
        if (leitura != sensorEstadoAtual) {
            sensorEstadoAtual = leitura;

            // A detecção ocorre na borda de subida (quando o sensor vai para HIGH).
            if (sensorEstadoAtual == HIGH) {
                // A flag `latinhaFoiRegistrada` previne registros múltiplos para uma única latinha.
                if (!latinhaFoiRegistrada) {
                    // Verifica se a lixeira em foco não está lotada antes de agir.
                    if (!historico.isLixeiraLotada(lixeiraAtualEmFoco)) {
                        // ACAO CRUCIAL: Registro, Feedback Visual, e ACAO DO ATUADOR (Servo)
                        historico.registrarDeposito(lixeiraAtualEmFoco);
                        piscaAmareloAcao();
                        // 1. ABRE A TAMPA
                        controlarTampaServo(POSICAO_ABERTA); 
                        // 2. FECHA A TAMPA (A lata ja deve ter caido)
                        controlarTampaServo(POSICAO_FECHADA);
                    } else {
                        Serial.println("Lixeira " + lixeiraAtualEmFoco + " Lotada! Deteccao ignorada.");
                    }
                    
                    latinhaFoiRegistrada = true; // Trava para a próxima detecção.
                }
            } else {
                if (latinhaFoiRegistrada) {
                    // Quando o sensor volta ao estado LOW, reseta a flag, preparando para a próxima detecção.
                    Serial.println("Sensor voltou ao repouso (LOW). Pronto para a proxima lata.");
                    latinhaFoiRegistrada = false;
                }
            }
        }
    }
    sensorEstadoAnterior = leitura;
}

// --- HANDLERS DO SERVIDOR WEB ---

// Handler genérico que gera a página HTML para uma lixeira específica (A, B ou C).
void handleLixeira(String lixeiraID) {
    // Define a lixeira em foco, para que os loops de hardware (sensor, LED) saibam em qual lixeira operar.
    lixeiraAtualEmFoco = lixeiraID;

    int totalLatinhas = historico.getContadorLixeira(lixeiraID);
    bool lotada = historico.isLixeiraLotada(lixeiraID);
    
    // Lógica de status visual dos LEDs de hardware.
    if (!lotada) {
        // Se disponível, acende o LED verde.
        setVerdeStatusLED(true);
        digitalWrite(LED_VERMELHO_LOTADO, LOW);
    } else {
        // Se lotada, apaga o LED verde. O LED vermelho será controlado pela função `loopPiscaVermelho`.
        setVerdeStatusLED(false); 
    }

    // Lógica para gerar o status visual na página HTML.
    String statusCheio, corStatus;
    if (lotada) {
        statusCheio = "LOTADA - COLETAR!";
        corStatus = "#dc3545";
    } else if (totalLatinhas > 0) {
        statusCheio = "EM USO";
        corStatus = "#ffc107";
    } else {
        statusCheio = "VAZIA";
        corStatus = "#28a745";
    }
    
    String statusHTML = "<div style='background-color: " + corStatus + "; color: white; padding: 15px; border-radius: 10px; margin: 15px 0;'>" +
                        "<h3 style='margin: 0;'> ESTADO: " + statusCheio + "</h3></div>";

    // O acionamento agora é exclusivamente via sensor, então a página apenas informa isso.
    String botaoSimulacao = "<p class='status' style='color: white; font-weight: bold;'>ACIONAMENTO POR SENSOR (GPIO 26) ATIVO.</p>";
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Controle Lixeira )rawliteral" + lixeiraID + R"rawliteral(</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="3"> 
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
  .container { padding: 20px; }
  h1 { color: white; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
  h2 { color: white; text-shadow: 1px 1px 2px rgba(0,0,0,0.3); }
  h3 { color: #555; }
  .info-box { background-color: rgba(255,255,255,0.95); border-radius: 15px; padding: 20px; margin-top: 20px; box-shadow: 0 8px 32px rgba(0,0,0,0.2); backdrop-filter: blur(10px); }
  .status { font-size: 1.2em; color: #555; margin: 10px 0; }
  .progress-bar { width: 100%; height: 30px; background-color: #e0e0e0; border-radius: 15px; overflow: hidden; margin: 15px 0; }
  .progress-fill { height: 100%; background: linear-gradient(90deg, #28a745, #ffc107, #dc3545); transition: width 0.3s ease; }
  .btn {
    display: inline-block;
    padding: 15px 30px;
    margin: 10px;
    text-decoration: none;
    color: #fff;
    background: linear-gradient(45deg, #28a745, #20c997);
    border-radius: 25px;
    font-size: 1.2em;
    cursor: pointer;
    transition: transform 0.3s ease, box-shadow 0.3s ease;
    box-shadow: 0 4px 15px rgba(40,167,69,0.3);
  }
  .btn:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(40,167,69,0.4); }
  .btn.blue { background: linear-gradient(45deg, #007BFF, #0056b3); box-shadow: 0 4px 15px rgba(0,123,255,0.3); }
  .btn.blue:hover { box-shadow: 0 6px 20px rgba(0,123,255,0.4); }
</style>
</head>
<body>
  <div class="container">
    <h1> Controle - Lixeira )rawliteral" + lixeiraID + R"rawliteral(</h1>
    <div class="info-box">
      )rawliteral" + statusHTML + R"rawliteral(
      <p class="status"> Total de Latinhas: )rawliteral" + String(totalLatinhas) + R"rawliteral(/5</p>
      <div class="progress-bar">
        <div class="progress-fill" style="width: )rawliteral" + String((totalLatinhas * 100) / LIMITE_LATINHAS) + R"rawliteral(%"></div>
      </div>
    </div>
    
    )rawliteral" + botaoSimulacao + R"rawliteral(
    <a href="/historico?lixeira=)rawliteral" + lixeiraID + R"rawliteral(" class="btn blue"> VER HISTORICO</a>

    <a href="/" class="btn blue"> Voltar ao Dashboard</a>
  </div>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

void handleLixeiraA() { handleLixeira("A"); }
void handleLixeiraB() { handleLixeira("B"); }
void handleLixeiraC() { handleLixeira("C"); }

// Handler para depósitos via URL. Esta funcionalidade foi removida em favor do sensor.
// A função é mantida para evitar erros 404, redirecionando o usuário para a página inicial.
void handleDepositar() {
    String lixeiraID = server.arg("lixeira");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

// Handler para a página de histórico.
void handleHistorico() {
    String lixeiraID = server.arg("lixeira");

    // Ao sair da página de controle para ver o histórico, desliga os LEDs de status e remove o foco.
    setVerdeStatusLED(false); 
    digitalWrite(LED_AMARELO_ACAO, LOW);
    digitalWrite(LED_VERMELHO_LOTADO, LOW); 
    lixeiraAtualEmFoco = "";

    // Gera o HTML com dados das duas estruturas: ABB e Lista Encadeada.
    String historicoABB = historico.listarEventosHTML(lixeiraID);
    String historicoLista = historico.listarHistoricoTemporalHTML(lixeiraID);
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Historico de Depositos</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
  .container { padding: 20px; }
  h1 { color: white; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
  h3 { color: white; text-shadow: 1px 1px 2px rgba(0,0,0,0.3); margin-top: 30px; }
  .history-section { background-color: rgba(255,255,255,0.95); border-radius: 15px; padding: 20px; margin: 20px 0; box-shadow: 0 8px 32px rgba(0,0,0,0.2); backdrop-filter: blur(10px); }
  ul { list-style-type: none; padding: 0; }
  li { 
    background: linear-gradient(135deg, #f8f9fa, #e9ecef); 
    border-radius: 10px; 
    padding: 15px; 
    margin: 10px 0; 
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    border-left: 4px solid #007BFF;
    transition: transform 0.2s ease;
  }
  li:hover { transform: translateX(5px); }
  a {
    display: inline-block;
    padding: 15px 30px;
    margin-top: 20px;
    text-decoration: none;
    color: #fff;
    background: linear-gradient(45deg, #007BFF, #0056b3);
    border-radius: 25px;
    transition: transform 0.3s ease, box-shadow 0.3s ease;
    box-shadow: 0 4px 15px rgba(0,123,255,0.3);
  }
  a:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0,123,255,0.4); }
</style>
</head>
<body>
  <div class="container">
    <h1> Historico da Lixeira )rawliteral" + lixeiraID + R"rawliteral(</h1>
    
    <div class="history-section">
      <h3> Arvore Binaria de Busca (ABB)</h3>
      <ul>)rawliteral" + historicoABB + R"rawliteral(</ul>
    </div>
    
    <div class="history-section">
      <h3> Lista Encadeada (Historico Temporal)</h3>
      <ul>)rawliteral" + historicoLista + R"rawliteral(</ul>
    </div>
    
    <a href="/lixeira)rawliteral" + lixeiraID + R"rawliteral("> Voltar ao Controle da Lixeira</a>
  </div>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

// Handler para a página raiz ("/"), o dashboard principal.
void handleRoot() {
    // Ao voltar para o dashboard, reseta o estado dos LEDs e remove o foco de qualquer lixeira.
    setVerdeStatusLED(false); 
    digitalWrite(LED_AMARELO_ACAO, LOW);
    digitalWrite(LED_VERMELHO_LOTADO, LOW); 
    lixeiraAtualEmFoco = "";
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>AluminiTech - Dashboard</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; text-align: center; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
  .container { padding: 20px; }
  h1 { color: white; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); margin-bottom: 30px; }
  h2 { color: white; margin-top: 30px; text-shadow: 1px 1px 2px rgba(0,0,0,0.3); }
  .info-box { background-color: rgba(255,255,255,0.95); border-radius: 15px; padding: 20px; margin-top: 20px; box-shadow: 0 8px 32px rgba(0,0,0,0.2); backdrop-filter: blur(10px); }
  .status { font-size: 1.2em; color: #555; margin: 10px 0; }
  .status-grid { display: flex; justify-content: center; flex-wrap: wrap; gap: 10px; margin: 20px 0; }
  .btn {
    display: inline-block;
    padding: 15px 30px;
    margin: 10px;
    text-decoration: none;
    color: #fff;
    background: linear-gradient(45deg, #007BFF, #0056b3);
    border-radius: 25px;
    font-size: 1.2em;
    cursor: pointer;
    transition: transform 0.3s ease, box-shadow 0.3s ease;
    box-shadow: 0 4px 15px rgba(0,123,255,0.3);
  }
  .btn:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0,123,255,0.4); }
  .alert { background: linear-gradient(45deg, #dc3545, #c82333); }
  .warning { background: linear-gradient(45deg, #ffc107, #e0a800); }
  .success { background: linear-gradient(45deg, #28a745, #1e7e34); }
</style>
</head>
<body>
  <div class="container">
    <h1> AluminiTech - Dashboard</h1>
    <div class="info-box">
      <p class="status"> Status do Sistema: ONLINE</p>
      <p class="status"> Endereco IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
      )rawliteral" + historico.getContadoresHTML() + R"rawliteral(
    </div>  
    
    <div class="status-grid">)rawliteral" + historico.getStatusLixeirasHTML() + R"rawliteral(</div>
    
    <h2> Acessar Lixeiras Individuais</h2>
    <a href="/lixeiraA" class="btn"> Lixeira A</a>
    <a href="/lixeiraB" class="btn"> Lixeira B</a>
    <a href="/lixeiraC" class="btn"> Lixeira C</a>
  </div>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}


void setup() {
    // Inicializa o Servo Motor e o posiciona na posição fechada.
    servoMotor.attach(SERVO_PIN);
    controlarTampaServo(POSICAO_FECHADA);

    Serial.begin(115200);
    
    // Configura os pinos dos LEDs como saída.
    pinMode(LED_VERDE_STATUS, OUTPUT);
    pinMode(LED_AMARELO_ACAO, OUTPUT);
    pinMode(LED_VERMELHO_LOTADO, OUTPUT);
    pinMode(LED_PIN_SETUP, OUTPUT);

    // Garante que todos os LEDs de status comecem desligados.
    setVerdeStatusLED(false); 
    digitalWrite(LED_AMARELO_ACAO, LOW);
    digitalWrite(LED_VERMELHO_LOTADO, LOW);

    Serial.println();
    Serial.print("Conectando-se a rede WiFi: ");
    Serial.println(ssid);

    // Conecta ao Wi-Fi e pisca o LED de setup durante o processo.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(50);
        digitalWrite(LED_PIN_SETUP, !digitalRead(LED_PIN_SETUP));
    }
    digitalWrite(LED_PIN_SETUP, HIGH);
    Serial.println("\nWiFi conectado!");
    Serial.println("Endereco IP: " + WiFi.localIP().toString());

    // Configura o cliente NTP para obter a hora real da internet (Fuso -3h, sem horário de verão).
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
    Serial.println("Sincronizando hora com NTP...");
    delay(2000);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.print("Hora atual sincronizada: ");
        Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
    } else {
        Serial.println("Falha ao obter hora do NTP");
    }

    // Define as rotas do servidor web e as funções (handlers) que as atenderão.
    server.on("/", handleRoot);
    server.on("/lixeiraA", handleLixeiraA);
    server.on("/lixeiraB", handleLixeiraB);
    server.on("/lixeiraC", handleLixeiraC);
    server.on("/depositar", handleDepositar);
    server.on("/historico", handleHistorico);

    server.begin();
    Serial.println("Servidor Web iniciado!");
    
    // Configura o pino do sensor como entrada.
    pinMode(SENSOR_PIN, INPUT);
    Serial.println("GPIO 26 configurado como INPUT. Iniciando monitoramento...");
}

// Loop principal do programa.
void loop() {
    server.handleClient(); // Processa requisições HTTP recebidas.
    loopPiscaVermelho();   // Executa a lógica de piscar o LED vermelho (não-bloqueante).
    loopSensor();          // Executa a leitura do sensor e a lógica de detecção (não-bloqueante).
}
