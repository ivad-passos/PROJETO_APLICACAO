import os
from datetime import datetime

from dotenv import load_dotenv
from flask import Flask, jsonify, request

load_dotenv()

from db import init_db, inserir_leitura, buscar_ultimas

app = Flask(__name__)

COMANDOS_VALIDOS = [
    "ligar", "desligar",
    "vermelho", "amarelo", "azul",
    "ativar_temp", "desativar_temp",
    "ativar_ldr", "desativar_ldr",
    "ativar_buzzer", "desativar_buzzer",
]

# Armazena o último comando pendente para o ESP32 buscar
_comando_pendente = None


@app.route("/health")
def health():
    return jsonify({"ok": True, "status": "online"})


@app.route("/dados", methods=["POST"])
def receber_dados():
    """Recebe leituras enviadas diretamente pelo ESP32 via HTTP POST."""
    body = request.get_json(silent=True)
    if not body:
        return jsonify({"ok": False, "erro": "JSON inválido ou ausente."}), 400

    inserir_leitura(
        temperatura=body.get("temperatura"),
        luminosidade=body.get("luminosidade"),
        lampada_ligada=body.get("lampada_ligada"),
        status_sistema=body.get("status_sistema"),
        cor_ativa=body.get("cor_ativa"),
    )

    print(f"[{datetime.now()}] Dados recebidos: {body}")

    # Retorna comando pendente (se houver) para o ESP32 executar
    global _comando_pendente
    resposta = {"ok": True}
    if _comando_pendente:
        resposta["comando"] = _comando_pendente
        _comando_pendente = None
    return jsonify(resposta)


@app.route("/status")
def status():
    """Retorna as últimas 20 leituras salvas no banco."""
    leituras = buscar_ultimas(20)
    resultado = []
    for l in leituras:
        resultado.append({
            "id": l["id"],
            "timestamp": l["timestamp"].isoformat() if l["timestamp"] else None,
            "temperatura": l["temperatura"],
            "luminosidade": l["luminosidade"],
            "lampada_ligada": bool(l["lampada_ligada"]) if l["lampada_ligada"] is not None else None,
            "status_sistema": l["status_sistema"],
            "cor_ativa": l["cor_ativa"],
        })
    return jsonify({"ok": True, "leituras": resultado})


@app.route("/comando", methods=["POST"])
def comando():
    """Envia um comando para o ESP32 (será entregue no próximo POST /dados)."""
    body = request.get_json(silent=True)
    if not body or "comando" not in body:
        return jsonify({"ok": False, "erro": "Campo 'comando' é obrigatório."}), 400

    cmd = body["comando"].strip().lower()
    if cmd not in COMANDOS_VALIDOS:
        return jsonify({"ok": False, "erro": "Comando desconhecido."}), 400

    global _comando_pendente
    _comando_pendente = cmd
    print(f"[{datetime.now()}] Comando enfileirado: {cmd}")
    return jsonify({"ok": True, "mensagem": f"Comando '{cmd}' enfileirado."})


if __name__ == "__main__":
    init_db()
    porta = int(os.getenv("FLASK_PORT", 5000))
    app.run(host="0.0.0.0", port=porta, debug=False)
