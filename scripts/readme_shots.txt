# Capturas del README (docs/readme/*.jpg). No es una verificacion: es la receta para rehacer
# las cinco imagenes cuando cambia el aspecto del juego (2026-11-10: los munecos adultos).
#
# Uso (sobre un guardado con el idioma ya contestado, ver scripts/CLAUDE.md):
#   MANTLE --script scripts/readme_shots.txt --shots <dir>
# y convertir rd_*.png a JPG (calidad 88) en docs/readme/: title, combat, map, dialogue, boss.

# --- 1. Titulo ---
wait 1.5
shot rd_title
wait 0.1

# --- Arranque: menu -> prologo -> carta cap 1 -> juego ---
press confirm
wait 3.0
press confirm
wait 3.0
press confirm
wait 1.5

# --- 2. Combate en el arroyo de Querit ---
# Pelea larga a proposito: en un guardado nuevo el primer milagro del bot saca el cartel de
# logro (3,5 s) en mitad de la pantalla. La foto va cuando ya se ha ido (y con la racha arriba).
spawn 0 8
spawn 1 2
wait 0.3
bot 4.4
shot rd_combat
wait 0.1
killall
wait 0.6

# --- 3. El mapa del capitulo ---
hold map 0.8
shot rd_map
hold map 0.4
wait 0.4

# --- 4. Elias ante Acab ---
audiencia
wait 1.2
shot rd_dialogue
wait 0.1
press cancel
wait 2.5
press cancel
wait 2.5
press confirm
wait 1.5

# --- 5. El jefe del Carmelo: los 450 profetas de Baal ---
god
chapter 3
wait 2.5
press confirm
wait 1.5
goto boss
wait 0.6
walkarena 50
wait 0.6
bot 4
shot rd_boss
wait 0.1
quit
