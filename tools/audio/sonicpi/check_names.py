"""Verifica las partituras ANTES de grabar: sintaxis Ruby y nombres.

Dos cosas que no se notan hasta que ya has perdido la captura:

1. Un sample mal escrito NO da error al evaluar en Sonic Pi: la capa entera se
   pierde en silencio y solo se ve al medir la pista, minutos despues.
2. Un error de sintaxis hace que la partitura entera no arranque, y el harness
   graba dos periodos de silencio. Se caza con el ruby que trae Sonic Pi
   (`ruby -c`), que no necesita Sonic Pi corriendo.

No comprueba los OPTS de cada synth: Sonic Pi acepta sin rechistar los que el
synthdef no tiene y los ignora. `:dsaw`, `:saw`, `:tri`, `:pluck` y `:recorder`
tragan `vibrato_rate` y no vibran; el UNICO synth con vibrato es `:blade`.

Uso:  python check_names.py [fichero.rb ...]   (sin argumentos, todos)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SAMPLES = os.path.expandvars(
    r"%LOCALAPPDATA%\Programs\SonicPiPortable\PFiles\Sonic Pi\etc\samples")
# Sintetizadores: los .scsyndef compilados, sin el prefijo sonic-pi-.
SYNTHDEFS = os.path.expandvars(
    r"%LOCALAPPDATA%\Programs\SonicPiPortable\PFiles\Sonic Pi\etc\synthdefs\compiled")
# El ruby que trae Sonic Pi. Sirve para `ruby -c` sin instalar nada. Se saca
# de la ruta de los synthdefs para no repetir la raiz de la instalacion.
_SP_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SYNTHDEFS)))
RUBY = os.path.join(_SP_ROOT, 'app', 'server', 'native', 'ruby', 'bin', 'ruby.exe')


def syntax_error(path):
    """Devuelve el mensaje de `ruby -c`, o None si la sintaxis esta bien."""
    if not os.path.exists(RUBY):
        return None
    import subprocess
    r = subprocess.run([RUBY, "-c", path], capture_output=True, text=True)
    out = (r.stdout + r.stderr).strip()
    if "Syntax OK" in out:
        return None
    return out.splitlines()[0] if out else "error de sintaxis"


def known_samples():
    return {f.rsplit(".", 1)[0] for f in os.listdir(SAMPLES)
            if f.endswith((".flac", ".wav"))}


def known_synths():
    out = set()
    for f in os.listdir(SYNTHDEFS):
        if not f.endswith(".scsyndef"):
            continue
        n = f[:-len(".scsyndef")]
        if n.startswith("sonic-pi-"):
            n = n[len("sonic-pi-"):]
        out.add(n)
    # Alias que Sonic Pi registra sin synthdef propio: :sine es :beep.
    out |= {"sine"}
    return out


def check(path, samples, synths):
    src = open(path, encoding="utf-8").read()
    bad = []
    for m in re.finditer(r"(?:pk|sample)\s*\(?\s*:([a-z0-9_]+)", src):
        if m.group(1) not in samples:
            bad.append(("sample", m.group(1), src[:m.start()].count("\n") + 1))
    for m in re.finditer(r"use_synth\s+:([a-z0-9_]+)", src):
        if m.group(1) not in synths:
            bad.append(("synth", m.group(1), src[:m.start()].count("\n") + 1))
    return bad


def main(argv):
    samples, synths = known_samples(), known_synths()
    files = argv or sorted(f for f in os.listdir(HERE)
                           if f.endswith(".rb") and f not in ("lib.rb", "runner.rb"))
    total, roto = 0, 0
    for f in files:
        p = f if os.path.isabs(f) else os.path.join(HERE, f)
        err = syntax_error(p)
        if err:
            roto += 1
            print(f"{os.path.basename(p)}: SINTAXIS -> {err}")
            continue           # con la sintaxis rota, los nombres dan igual
        bad = check(p, samples, synths)
        total += len(bad)
        for kind, name, line in bad:
            print(f"{os.path.basename(p)}:{line}: {kind} DESCONOCIDO -> :{name}")
    print(f"{len(files)} partituras, {roto} con la sintaxis rota, {total} nombres malos")
    return 1 if (total or roto) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
