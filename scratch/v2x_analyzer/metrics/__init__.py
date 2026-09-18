"""Subpacote de métricas — um módulo por secção do relatório.

Adicionar uma métrica nova = adicionar um ficheiro novo aqui com uma
função `compute(entries, ...) -> SomeDataclass`, e registá-la em
`report.py`. Nenhum módulo de métricas depende de outro, exceto de
`identity` (partilhado) e `aliases` (grupos de (módulo,evento)
equivalentes entre variantes do logger — ver v2x_analyzer/aliases.py) —
mantém-nos independentemente testáveis.

Regra de ouro desde esta versão: se um (módulo,evento) tiver mais do
que uma grafia possível consoante a variante do logger, a lista de
grafias vive em aliases.py, NUNCA copiada localmente num módulo de
métricas. Foi precisamente uma cópia local desincronizada (o módulo
"CAM" em vez de "CAM_RX" para os eventos de decode) que manteve
decode_ok/decode_fail_* sempre a zero nesta versão anterior, sem
qualquer aviso.
"""
