# SUMO — projeto V2X

`current/` é a área oficial de trabalho SUMO dentro do repositório ns-3.47.
`versions/` contém snapshots históricos; `scripts/snapshot-current.sh`
cria uma cópia com manifesto SHA-256 antes das alterações.

Antes de modificar, substituir ou gerar ficheiros em `current/`, executar
o script e confirmar `SNAPSHOT_SHA256=PASS`.

A primeira versão arquivada é a cópia inicial do projeto anterior.
O projeto em `~/final/v2x_sumo` permanece preservado durante a transição.

A rede em `current/scenario/legacy-local/` não está demonstrada como
georreferenciada. `current/scenario/georeferenced/` fica reservada a uma
rede construída a partir de cartografia documentada e validada.
