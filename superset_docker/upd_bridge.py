#!/usr/bin/env python3
"""
PROJECT NERO - Puente UDP → Base de datos
Recibe datos UDP del ESP32 y los guarda en base de datos para Superset
"""

import socket
import json
import sqlite3
import threading
import time
from datetime import datetime
import os

# Configuración
UDP_HOST = "172.20.10.6"  # IP de tu PC en la red del iPhone
UDP_PORT = 8088           # Puerto que usa tu ESP32
DB_PATH = r'C:\PROJECT-NERO-SUPERSET\neural_data.db'

class ProjectNeroUDPBridge:
    def __init__(self):
        self.running = True
        self.total_received = 0
        self.init_database()
        
    def init_database(self):
        """Crear base de datos compatible con Superset"""
        print("🧠 PROJECT NERO - Inicializando base de datos...")
        
        # Crear directorio si no existe
        os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
        
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS neural_readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                unix_timestamp INTEGER NOT NULL,
                device_id TEXT DEFAULT 'ESP32_PROJECT_NERO',
                theta INTEGER,
                alpha INTEGER,
                delta INTEGER,
                beta INTEGER,
                attention INTEGER,
                meditation INTEGER,
                focus_ratio INTEGER,
                signal_quality INTEGER,
                heart_rate INTEGER,
                stress_level INTEGER,
                session_id TEXT
            )
        ''')
        
        conn.commit()
        conn.close()
        print(f"✅ Base de datos creada: {DB_PATH}")
    
    def save_neural_data(self, data):
        """Guardar datos neurales en base de datos"""
        try:
            current_time = datetime.now()
            timestamp_str = current_time.strftime('%Y-%m-%d %H:%M:%S')
            unix_ts = int(time.time())
            
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()
            
            # Calcular métricas derivadas
            attention = data.get('attention', 50)
            meditation = data.get('meditation', 50)
            
            # Calcular frecuencia cardíaca estimada basada en estrés
            beta = data.get('beta', 50)
            stress_level = min(10, max(1, int(beta / 10)))  # Beta alto = más estrés
            heart_rate = 65 + stress_level * 3  # 65-95 BPM basado en estrés
            
            cursor.execute('''
                INSERT INTO neural_readings 
                (timestamp, unix_timestamp, theta, alpha, delta, beta, 
                 attention, meditation, focus_ratio, signal_quality,
                 heart_rate, stress_level, session_id)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                timestamp_str,
                unix_ts,
                data.get('theta', 0),
                data.get('alpha', 0),
                data.get('delta', 0),
                data.get('beta', 0),
                attention,
                meditation,
                data.get('focusRatio', 0),
                data.get('signalQuality', 100),
                heart_rate,
                stress_level,
                f'session_{unix_ts // 300}'  # Nueva sesión cada 5 minutos
            ))
            
            conn.commit()
            conn.close()
            
            self.total_received += 1
            
            print(f"💾 Datos guardados #{self.total_received}")
            print(f"   Theta: {data.get('theta')}, Alpha: {data.get('alpha')}")
            print(f"   Beta: {data.get('beta')}, Delta: {data.get('delta')}")
            print(f"   Attention: {attention}, Meditation: {meditation}")
            print(f"   Heart Rate: {heart_rate}, Stress: {stress_level}")
            
            return True
            
        except Exception as e:
            print(f"❌ Error guardando datos: {e}")
            return False
    
    def udp_listener(self):
        """Escuchar datos UDP del ESP32"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(1.0)  # Timeout de 1 segundo
        
        try:
            sock.bind((UDP_HOST, UDP_PORT))
            print(f"🎧 Escuchando UDP en {UDP_HOST}:{UDP_PORT}")
            print("📡 Esperando datos del ESP32...")
            
            while self.running:
                try:
                    data, addr = sock.recvfrom(1024)
                    message = data.decode('utf-8')
                    
                    print(f"\n📥 UDP recibido de {addr[0]}:")
                    print(f"   Raw: {message}")
                    
                    # Parsear JSON
                    try:
                        json_data = json.loads(message)
                        self.save_neural_data(json_data)
                    except json.JSONDecodeError as e:
                        print(f"❌ Error JSON: {e}")
                        print(f"   Mensaje: {message}")
                        
                except socket.timeout:
                    continue  # Continuar escuchando
                except Exception as e:
                    print(f"❌ Error UDP: {e}")
                    
        except Exception as e:
            print(f"❌ Error iniciando servidor UDP: {e}")
        finally:
            sock.close()
    
    def status_monitor(self):
        """Monitor de estado cada 30 segundos"""
        while self.running:
            try:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                
                cursor.execute('SELECT COUNT(*) FROM neural_readings')
                total = cursor.fetchone()[0]
                
                cursor.execute('''
                    SELECT theta, alpha, beta, delta, timestamp 
                    FROM neural_readings 
                    ORDER BY unix_timestamp DESC 
                    LIMIT 1
                ''')
                latest = cursor.fetchone()
                
                conn.close()
                
                print(f"\n📊 PROJECT NERO - Estado:")
                print(f"   Total registros: {total}")
                print(f"   Recibidos esta sesión: {self.total_received}")
                if latest:
                    print(f"   Último dato: {latest[4]}")
                    print(f"   Valores: T:{latest[0]} A:{latest[1]} B:{latest[2]} D:{latest[3]}")
                
            except Exception as e:
                print(f"❌ Error monitor: {e}")
            
            time.sleep(30)
    
    def start(self):
        """Iniciar el servidor puente"""
        print("🧠 PROJECT NERO - Servidor Puente UDP iniciando...")
        print(f"📡 ESP32 debe enviar a: {UDP_HOST}:{UDP_PORT}")
        print(f"💾 Base de datos: {DB_PATH}")
        print("🚀 Presiona Ctrl+C para detener\n")
        
        # Iniciar hilo de monitoreo
        monitor_thread = threading.Thread(target=self.status_monitor, daemon=True)
        monitor_thread.start()
        
        # Iniciar servidor UDP
        try:
            self.udp_listener()
        except KeyboardInterrupt:
            print("\n🛑 Deteniendo PROJECT NERO...")
            self.running = False

if __name__ == "__main__":
    bridge = ProjectNeroUDPBridge()
    bridge.start()