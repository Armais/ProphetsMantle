"""Verifica los nombres de sample de las partituras contra etc/samples/.

Un sample mal escrito NO da error al evaluar en Sonic Pi: la capa entera se
pierde en silencio y solo se nota al medir la pista, 3,5 minutos de captura
despues. Esto lo caza antes de grabar.

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
    total = 0
    for f in files:
        p = f if os.path.isabs(f) else os.path.join(HERE, f)
        bad = check(p, samples, synths)
        total += len(bad)
        for kind, name, line in bad:
            print(f"{os.path.basename(p)}:{line}: {kind} DESCONOCIDO -> :{name}")
    print(f"{len(files)} partituras, {total} nombres malos")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
