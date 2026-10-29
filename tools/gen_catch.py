#!/usr/bin/env python3
"""Genera catchrate.h: el ratio de captura real de cada especie (0-255).

Sale de PokeAPI (pokemon-species/N -> capture_rate), el mismo dato que usan los
juegos: Rattata 255, los iniciales 45, los legendarios 3. wildCatchValue() lo
usa en la formula de captura de los encuentros salvajes.

  python3 tools/gen_catch.py

Las respuestas se guardan en tools/pokeapi_cache/, la misma cache que
gen_dex_data.py, asi que si ese ya corrio no hace falta red.
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CACHE = os.path.join(HERE, 'pokeapi_cache')
OUT = os.path.join(ROOT, 'catchrate.h')

_dexh = open(os.path.join(ROOT, 'dex.h')).read()
DEX_COUNT = int(re.search(r'#define DEX_COUNT (\d+)', _dexh).group(1))


def get(url):
    """GET con cache, igual que gen_dex_data.py (curl: ver gen_avatars.py)."""
    os.makedirs(CACHE, exist_ok=True)
    key = re.sub(r'[^a-z0-9]+', '_', url.split('/api/v2/')[1].strip('/'))
    path = os.path.join(CACHE, key + '.json')
    if os.path.exists(path):
        return json.load(open(path))
    r = subprocess.run(['curl', '-fsSL', '--retry', '2', url], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('fetch failed: %s' % url)
    d = json.loads(r.stdout)
    json.dump(d, open(path, 'w'))
    return d


def main():
    rates = [0]
    for n in range(1, DEX_COUNT + 1):
        sp = get('https://pokeapi.co/api/v2/pokemon-species/%d' % n)
        rates.append(int(sp.get('capture_rate', 45)))
        sys.stderr.write('\r%d/%d' % (n, DEX_COUNT))
        sys.stderr.flush()
    sys.stderr.write('\n')

    lines = []
    for i in range(0, len(rates), 16):
        lines.append('  ' + ', '.join('%3d' % r for r in rates[i:i + 16]) + ',')
    with open(OUT, 'w', newline='\r\n') as f:
        f.write('#pragma once\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "dex.h"\n\n')
        f.write('// Ratio de captura real de cada especie (0-255), de PokeAPI.\n')
        f.write('// Generado por tools/gen_catch.py: no editar a mano.\n')
        f.write('static const uint8_t CATCH_RATE[DEX_COUNT + 1] = {\n')
        f.write('\n'.join(lines) + '\n')
        f.write('};\n')
    print('guardado %s: %d especies' % (OUT, DEX_COUNT))


if __name__ == '__main__':
    main()
