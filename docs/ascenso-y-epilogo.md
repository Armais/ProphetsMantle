# El Ascenso y el epilogo, recorridos

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/core/`** (el contador de paso vive en
el Engine) y a **`src/game/`** (`events.h`). La cinematica del carro de fuego esta en
`docs/cinematicas.md`; la herencia del Manto, en `src/game/CLAUDE.md`.

### El Ascenso y el epilogo, recorridos (GDD2.0 fases 5 y 6, 2026-08-26)

Los dos momentos que cierran cada acto eran **paginas de texto que se pasaban con ENTER**.
Ahora se RECORREN: una cadena de encuentros con decision, uno detras de otro, y la pagina de
cierre al final. Comparten maquinaria con el pozo de encuentros —que es justo lo que el plan
predijo— y por eso costaron poco: son ids de `namespace Ev` fuera de todo capitulo
(`event_chapter` devuelve 0 para ellos) mas un contador de paso en el Engine.

**El Ascenso (2 Reyes 2:1-14): CUATRO paradas.** Gilgal, Betel, el Jordan y «pide lo que
quieras». Tres veces le dice que se quede y tres veces hay que negarse — eso es lo que el
pasaje repite, asi que es lo que el jugador repite.

- **Quedarse no rompe nada: acorta la herencia**, que es exactamente lo que significa no llegar
  hasta el final del camino. Cada parada en la que te quedas atras resta un 10% a la fe
  heredada. **Seguirle las tres veces y pedir la doble porcion deja la herencia EXACTAMENTE
  como estaba** antes de que el Ascenso fuera jugable — esa era la condicion del track, y por
  eso `ascension_check` sigue midiendo lo mismo.
- **El Manto cae igual** (se le cayo a el, no te lo dio: 2 Reyes 2:13). Lo que se pide o no se
  pide es el **espiritu doblado**: sin la doble porcion no hay milagro heredado, «y si no, no»
  (2:10).
- Las decisiones **se apuntan, no se aplican**: la herencia se reparte entera en
  `finish_ascension`. Aplicar ahi la fe perdida seria descontarla dos veces, porque la fe
  heredada se calcula sobre la que tengas al llegar.

**El epilogo "El Legado" (2 Reyes 9; 13:14-21): TRES escenas.** Jehu y la muerte de Jezabel
—que cumple al pie de la letra la palabra de Elias sobre la vina de Nabot y cierra el arco del
Acto I—, el arco de Joas y los huesos de Eliseo, cuyo ultimo milagro ocurre cuando el profeta
lleva un ano muerto.

- Aqui la partida **ya se ha acabado**, asi que estas decisiones no mueven vida ni fe: no queda
  partida que mover, e inventarle un premio mecanico a una escena de cierre seria relleno. Lo
  que cambian es lo que se te cuenta — y **la del arco cambia las ultimas palabras del juego**
  (`story_epilogue(s, joash_full)`), que en ese pasaje es exactamente lo que estaba en juego.
- Sigue siendo **desbloqueble**: fe en Profeta Ungido (>=75) al ganar el capitulo 10. Quien gane
  con la fe por los suelos se lleva su pantalla de victoria con su nota, igual que antes.

**`DialogueState::dark`**: las dos cadenas se juegan **sobre negro** y no sobre el mundo. Pasan
fuera de una sala, y dejar detras la arena del jefe del capitulo 5 —o la del 10— seria mentir
sobre donde estas.

Guiones: `ascension_check.txt` y `epilogo_check.txt`, ampliados para fotografiar cada parada.
Gates del log: `Ascension: le seguiste 3 de 3 veces, doble porcion=si` y `Epilogo: el arco de
Joas hasta el final`. En autoplay el dialogo decide solo y **siempre toma la primera opcion**,
asi que el bot recorre el camino completo — que es justo el que tiene que dar la herencia de
siempre.
