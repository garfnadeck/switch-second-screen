import socket
import math
import time

HOST = '127.0.0.1'
PORT = 6000

print(f"🎮 [Fake Nintendo Switch - sys-botbase] Ouvindo em {HOST}:{PORT}...")
print("👉 Na sua tela 320x480, va na aba 'Config', digite o IP 127.0.0.1 e selecione 'Desbloqueado (Sys-Botbase)'!\n")

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind((HOST, PORT))
server.listen(1)

angle = 0.0
garchomp_hp = 280
zelda_hearts = 20
zelda_weapon = 100

while True:
    conn, addr = server.accept()
    print(f"✅ Segunda Tela conectada via TCP! Endereco: {addr}")
    conn.settimeout(0.5)
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            cmd = data.decode('utf-8', errors='ignore').strip()
            print(f"📥 [RX da Segunda Tela]: {cmd}")

            # Se a segunda tela clicou num botão de Macro, aplica o cheat na "RAM" do servidor!
            if cmd.startswith("MACRO:"):
                parts = cmd.split(":")
                profile, slot = int(parts[1]), int(parts[2])
                if profile == 0 and slot == 0:  # Pokemon Heal Party
                    garchomp_hp = 280
                    print("✨ [RAM Cheat] Party Pokemon curada via TCP!")
                elif profile == 1 and slot == 0:  # Zelda Full Hearts
                    zelda_hearts = 20
                    print("❤️ [RAM Cheat] Coracoes do Link restaurados via TCP!")
                elif profile == 1 and slot == 1:  # Zelda Repair Gear
                    zelda_weapon = 100
                    print("⚔️ [RAM Cheat] Master Sword reparada em 100% via TCP!")

            # Calcula nova posição do personagem no Switch falso (movimento em espiral)
            angle += 0.15
            x = 135.0 + math.cos(angle) * 90.0
            y = 120.0 + math.sin(angle) * 80.0
            z = 250.0 + math.sin(angle * 0.5) * 100.0

            # Simula desgaste natural
            garchomp_hp = max(15, garchomp_hp - 8)
            zelda_weapon = max(10, zelda_weapon - 4)
            stamina = int(50 + 50 * math.sin(angle))

            # Envia os dados de volta pelo Socket TCP para a tela LVGL
            if "pointerPeek" in cmd:
                reply = f"TELEM:{x:.1f},{y:.1f},{z:.1f},{garchomp_hp},240,100\n"
            else:
                reply = f"TELEM:{x:.1f},{y:.1f},{z:.1f},{zelda_hearts},{stamina},{zelda_weapon}\n"

            conn.sendall(reply.encode('utf-8'))
    except socket.timeout:
        pass
    except Exception as e:
        print(f"⚠️ Conexao encerrada: {e}")
    finally:
        conn.close()
