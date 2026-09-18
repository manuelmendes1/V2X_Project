"""
cli.py — interface de linha de comandos.

Uso:
    python -m v2x_analyzer output_log.txt [output_log2.txt ...]
    python -m v2x_analyzer --json output_log.txt
"""

from __future__ import annotations

import argparse
import sys

from .parser import load_log
from .report import build_report, render_text, render_json


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="v2x_analyzer",
        description="Analisador de logs V2X (ns-3 + SUMO + Vanetza CAM).",
    )
    ap.add_argument(
        "logfiles",
        nargs="+",
        help="Um ou mais ficheiros de log, lidos pela ordem dada.",
    )
    ap.add_argument(
        "--json",
        action="store_true",
        help="Escreve o relatório em JSON em vez de texto legível.",
    )
    ap.add_argument(
        "-o", "--output",
        help="Escreve o relatório neste ficheiro em vez de stdout.",
    )
    args = ap.parse_args(argv)

    entries, stats = load_log(args.logfiles)
    if not entries:
        print("AVISO: nenhuma linha reconhecida em nenhum ficheiro dado.", file=sys.stderr)

    report = build_report(entries, stats)
    text = render_json(report) if args.json else render_text(report)

    if args.output:
        with open(args.output, "w") as f:
            f.write(text + "\n")
    else:
        print(text)

    return 0 if report.exit_.all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
