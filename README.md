# Lâmpada Inteligente – IoT com ESP32 + Flask + MySQL + Grafana

Sistema de lâmpada inteligente onde o ESP32 envia dados diretamente para um backend Python via Wi-Fi (sem dependência de serviços pagos).

```
ESP32 ──WiFi──► Flask (Python) ──► MySQL ──► Grafana (Dashboard)
                      ▲
                  POST /dados
                  GET  /status
                  POST /comando
```

---

## 1. Componentes e Pinos do ESP32

| Componente       | Pino ESP32 | Observações                        |
|------------------|------------|------------------------------------|
| LED RGB (R)      | GPIO 21    | Saída PWM                          |
| LED RGB (G)      | GPIO 19    | Saída PWM                          |
| LED RGB (B)      | GPIO 18    | Saída PWM                          |
| Botão            | GPIO 23    | INPUT_PULLUP, interrupção FALLING  |
| Potenciômetro    | GPIO 35    | Entrada analógica                  |
| Fotorresistor    | GPIO 34    | Entrada analógica (LDR)            |
| DHT22            | GPIO 15    | Sensor de temperatura              |
| Buzzer           | GPIO 27    | Saída digital                      |

---

## 2. Como Montar o Projeto

### Pré-requisitos

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) instalado
- Python 3.x instalado
- Arduino IDE instalado

### Passo 1 – Subir MySQL e Grafana com Docker

```bash
cd aplicacao_1
docker-compose up -d
```

Aguarde ~20 segundos. Isso sobe:
- **MySQL** na porta `3306` (senha: `senha123`)
- **Grafana** em `http://localhost:3000` (login: `admin` / `admin`)

### Passo 2 – Configurar o backend Python

```bash
# Copiar o arquivo de variáveis de ambiente
cp .env.example .env
```

Edite o `.env` se necessário (a senha padrão já bate com o Docker):

```env
MYSQL_HOST=localhost
MYSQL_PORT=3306
MYSQL_USER=root
MYSQL_PASSWORD=senha123
MYSQL_DATABASE=lampada_iot
FLASK_PORT=5000
```

### Passo 3 – Instalar dependências e rodar o backend

```bash
pip install -r requirements.txt
python app.py
```

O banco `lampada_iot` e a tabela `leituras` são criados automaticamente.
O servidor fica disponível em `http://localhost:5000`.

### Passo 4 – Configurar e gravar o firmware no ESP32

Abra `aplicacao_1.ino` no Arduino IDE e edite as 3 linhas no topo:

```cpp
#define WIFI_SSID     "SEU_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"
#define BACKEND_URL   "http://192.168.1.100:5000"  // IP da sua máquina
```

> Para descobrir o IP da sua máquina: no terminal, rode `ipconfig` (Windows) e copie o **IPv4** da sua rede Wi-Fi.

Instale a biblioteca **DHT sensor library** (by Adafruit) no Library Manager e faça upload para o ESP32.

---

## 3. Como Rodar o Backend

```bash
python app.py
```

Saída esperada:
```
[DB] Banco e tabela verificados com sucesso.
 * Running on http://0.0.0.0:5000
```

---

## 4. Endpoints da API

| Método | Rota      | Descrição                                         |
|--------|-----------|---------------------------------------------------|
| GET    | `/health` | Verifica se o servidor está online                |
| POST   | `/dados`  | Recebe leituras do ESP32 (chamado pelo firmware)  |
| GET    | `/status` | Retorna as últimas 20 leituras do banco           |
| POST   | `/comando`| Enfileira um comando para o ESP32 executar        |

### Enviar um comando ao ESP32

```bash
curl -X POST http://localhost:5000/comando \
  -H "Content-Type: application/json" \
  -d '{"comando": "ligar"}'
```

Comandos válidos:
```
ligar, desligar
vermelho, amarelo, azul
ativar_temp, desativar_temp
ativar_ldr, desativar_ldr
ativar_buzzer, desativar_buzzer
```

O comando é entregue ao ESP32 na próxima vez que ele fizer POST em `/dados` (a cada 5 segundos).

---

## 5. Como Consultar Dados Salvos

### Via Grafana (Dashboard)

Acesse `http://localhost:3000`, login `admin` / `admin`.  
O dashboard **"Lâmpada Inteligente IoT"** já está pré-configurado com:
- Gráfico de temperatura ao longo do tempo
- Gráfico de luminosidade
- Indicador de status (ligada/desligada)
- Cor ativa
- Gauge de temperatura
- Log de status do sistema

### Via API

```bash
curl http://localhost:5000/status
```

### Via MySQL direto

```bash
# Dentro do container:
docker exec -it lampada_mysql mysql -uroot -psenha123 lampada_iot

# Query:
SELECT * FROM leituras ORDER BY timestamp DESC LIMIT 20;
```

---

## 6. Estrutura do Projeto

```
aplicacao_1/
├── aplicacao_1.ino          # Firmware ESP32
├── app.py                   # Backend Flask
├── db.py                    # Acesso ao MySQL
├── requirements.txt
├── .env.example
├── docker-compose.yml       # MySQL + Grafana
└── grafana/
    └── provisioning/
        ├── datasources/
        │   └── mysql.yml    # Conexão automática ao MySQL
        └── dashboards/
            ├── dashboard.yml
            └── lampada_iot.json  # Dashboard pré-configurado
```
