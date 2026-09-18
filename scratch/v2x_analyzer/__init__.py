"""
v2x_analyzer — analisador modular de logs V2X (ns-3 + SUMO + Vanetza CAM).

Versão 1.1.0 — corrige o parser (que estava a rejeitar 100% das linhas
do log real, ver README §"Novidades desta versão") e mais 6 achados
concretos de contagem encontrados durante a reescrita. Ver README.md
para a lista completa e a evidência de cada achado.

Uso rápido:
    from v2x_analyzer.parser import load_log
    from v2x_analyzer.report import build_report, render_text

    entries, stats = load_log(["output_log.txt"])
    report = build_report(entries, stats)
    print(render_text(report))

Ou pela linha de comandos:
    python -m v2x_analyzer output_log.txt
"""

__version__ = "1.1.0"
