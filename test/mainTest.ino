#include <Arduino.h>
#include <WiFi.h>

/*
Protótipo acadêmico: implementamos uma única lixeira física como prova de conceito na faculdade. 
O sistema simula todas as funcionalidades: detecção de latinha pelo sensor, feedback via LED e buzzer, 
registro e armazenamento em árvore binária, e envio de dados via Wi-Fi. 
Embora apenas uma lixeira tenha sido montada fisicamente, o software é escalável: cada depósito gera um nó na árvore, 
e no futuro é possível expandir para múltiplas lixeiras em diferentes locais. 
Assim, o protótipo é simples e de baixo custo, mas demonstra como seria aplicado em larga escala em uma cidade inteligente.
*/

// ==============================
// Configuração da rede WiFi
// ==============================
const char* ssid = "Sara Hadassa";     // Nome da rede WiFi
const char* password = "lenovo2025";   // Senha da rede WiFi

// ==============================
// Estrutura da ÁRVORE BINÁRIA (ABB)
// Cada nó representa um "estado" do contador de latinhas
// ==============================
struct EventoNode {
    int id;                   // ID do depósito (sequencial)
    int contadorLatinhas;     // Total acumulado de latinhas até esse depósito
    EventoNode* left;         // Filho esquerdo
    EventoNode* right;        // Filho direito

    // Construtor para inicializar os valores
    EventoNode(int id, int total) 
        : id(id), contadorLatinhas(total), 
          left(nullptr), right(nullptr) {}
};

// ==============================
// Estrutura da LISTA ENCADEADA
// Cada depósito também é salvo em ordem temporal (com timestamp)
// ==============================
struct Deposito {
    int idEvento;             // ID do depósito
    unsigned long timestamp;  // Momento em que ocorreu (em ms desde que o ESP foi ligado)
    Deposito* next;           // Ponteiro para o próximo depósito

    // Construtor
    Deposito(int id, unsigned long tempo) 
        : idEvento(id), timestamp(tempo), next(nullptr) {}
};

// ==============================
// Classe gerenciadora do histórico
// Ela mantém tanto a Árvore Binária quanto a Lista Encadeada
// ==============================
class HistoricoDepositos {
private:
    EventoNode* root;     // Raiz da Árvore Binária
    Deposito* head;       // Cabeça da Lista Encadeada
    int proximoId;        // Próximo ID de depósito
    int totalLatinhas;    // Contador total de latinhas

    // Inserção recursiva na Árvore Binária
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

    // Impressão em ordem da Árvore Binária (menor ID → maior ID)
    void inOrderRec(EventoNode* node) {
        if (node != nullptr) {
            inOrderRec(node->left);
            Serial.println("Depósito nº " + String(node->id) + 
                           " | Total acumulado: " + String(node->contadorLatinhas));
            inOrderRec(node->right);
        }
    }

public:
    // Construtor inicializa variáveis
    HistoricoDepositos() : root(nullptr), head(nullptr), proximoId(1), totalLatinhas(0) {}

    // Registrar um novo depósito de latinha
    void registrarDeposito() {
        totalLatinhas++;

        // Salvar na Árvore Binária
        root = inserirRec(root, proximoId, totalLatinhas);

        // Salvar na Lista Encadeada (com timestamp)
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

        // Mensagem de confirmação
        Serial.println("\nLatinha registrada com sucesso!");
        Serial.println("Total de latinhas acumuladas: " + String(totalLatinhas));
        proximoId++;
    }

    // Listar eventos pela Árvore Binária (em ordem crescente de ID)
    void listarEventos() {
        if (root == nullptr) {
            Serial.println("\nNenhum depósito registrado ainda.");
            return;
        }
        Serial.println("\n=== HISTÓRICO DE DEPÓSITOS (ÁRVORE BINÁRIA) ===");
        inOrderRec(root);
        Serial.println("===============================================");
    }

    // Listar histórico em ordem temporal (pela Lista Encadeada)
    void listarHistoricoTemporal() {
        if (head == nullptr) {
            Serial.println("\nNenhum depósito registrado ainda.");
            return;
        }
        Serial.println("\n=== HISTÓRICO TEMPORAL (LISTA ENCADEADA) ===");
        Deposito* temp = head;
        while (temp != nullptr) {
            Serial.println("Evento ID: " + String(temp->idEvento) + 
                           " | Timestamp: " + String(temp->timestamp) + " ms");
            temp = temp->next;
        }
        Serial.println("=============================================");
    }
};

// Criamos uma instância global para gerenciar o histórico
HistoricoDepositos historico;

// ==============================
// Funções Placeholders (futuro hardware)
// ==============================

// Feedback ao usuário (FUTURO: LED + buzzer)
// Por enquanto, só mostra no Serial
void feedback() {
    Serial.println("Feedback: LED aceso + beep!");
}

// Detecção de latinha (FUTURO: sensor ultrassônico ou indutivo)
// Agora: simulação via comandos no Serial
void detectarLatinha() {
    if (Serial.available() > 0) {
        char comando = Serial.read();
        comando = tolower(comando);  // Aceita maiúsculas/minúsculas

        if (comando == 'd') {        // Depositar latinha
            historico.registrarDeposito();
            feedback();
        }
        else if (comando == 'l') {   // Listar histórico (ABB)
            historico.listarEventos();
        }
        else if (comando == 't') {   // Listar histórico temporal (Lista Encadeada)
            historico.listarHistoricoTemporal();
        }
        else if (comando == 'h') {   // Mostrar ajuda
            Serial.println("\nCOMANDOS DISPONIVEIS:");
            Serial.println("d   - Depositar latinha");
            Serial.println("l   - Listar histórico (ABB)");
            Serial.println("t   - Listar histórico temporal (Lista)");
            Serial.println("h   - Ajuda");
        }
    }
}

// ==============================
// Setup inicial
// ==============================
void setup() {
    Serial.begin(115200);   // Inicia comunicação serial
    delay(2000);

    Serial.println("\n==========================================");
    Serial.println("    SISTEMA DE LIXEIRA INTELIGENTE");
    Serial.println("==========================================");

    // Conexão WiFi
    Serial.println("Conectando ao WiFi: " + String(ssid));
    WiFi.begin(ssid, password);

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 15) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Conectado!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFalha no WiFi - Modo Offline");
    }

    // Mostra comandos disponíveis
    Serial.println("\nSistema pronto! Monitorando a Lixeira Central.");
    Serial.println("\nCOMANDOS DISPONIVEIS:");
    Serial.println("d   - Depositar latinha");
    Serial.println("l   - Listar histórico (ABB)");
    Serial.println("t   - Listar histórico temporal (Lista)");
    Serial.println("h   - Ajuda");
    Serial.println("==========================================");
}

// ==============================
// Loop principal
// ==============================
void loop() {
    detectarLatinha();  // Aqui simulamos a entrada da latinha via Serial
    delay(100);         // Pequeno atraso para evitar leitura repetida
}
