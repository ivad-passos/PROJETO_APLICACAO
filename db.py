import os
import mysql.connector
from mysql.connector import Error


def _get_connection(database=None):
    return mysql.connector.connect(
        host=os.getenv("MYSQL_HOST", "localhost"),
        port=int(os.getenv("MYSQL_PORT", 3306)),
        user=os.getenv("MYSQL_USER", "root"),
        password=os.getenv("MYSQL_PASSWORD", ""),
        database=database,
    )


def init_db():
    try:
        conn = _get_connection()
        cursor = conn.cursor()
        cursor.execute("CREATE DATABASE IF NOT EXISTS lampada_iot")
        cursor.close()
        conn.close()

        conn = _get_connection(database="lampada_iot")
        cursor = conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS leituras (
                id              INT AUTO_INCREMENT PRIMARY KEY,
                timestamp       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                temperatura     FLOAT,
                luminosidade    INT,
                lampada_ligada  TINYINT(1),
                status_sistema  VARCHAR(100),
                cor_ativa       VARCHAR(20)
            )
        """)
        conn.commit()
        cursor.close()
        conn.close()
        print("[DB] Banco e tabela verificados com sucesso.")
    except Error as e:
        print(f"[DB] Erro ao inicializar banco: {e}")


def inserir_leitura(temperatura, luminosidade, lampada_ligada, status_sistema, cor_ativa):
    try:
        conn = _get_connection(database=os.getenv("MYSQL_DATABASE", "lampada_iot"))
        cursor = conn.cursor()
        cursor.execute(
            """
            INSERT INTO leituras (temperatura, luminosidade, lampada_ligada, status_sistema, cor_ativa)
            VALUES (%s, %s, %s, %s, %s)
            """,
            (temperatura, luminosidade, lampada_ligada, status_sistema, cor_ativa),
        )
        conn.commit()
        cursor.close()
        conn.close()
    except Error as e:
        print(f"[DB] Erro ao inserir leitura: {e}")


def buscar_ultimas(n=20):
    try:
        conn = _get_connection(database=os.getenv("MYSQL_DATABASE", "lampada_iot"))
        cursor = conn.cursor(dictionary=True)
        cursor.execute(
            "SELECT * FROM leituras ORDER BY timestamp DESC LIMIT %s", (n,)
        )
        resultados = cursor.fetchall()
        cursor.close()
        conn.close()
        return resultados
    except Error as e:
        print(f"[DB] Erro ao buscar leituras: {e}")
        return []
