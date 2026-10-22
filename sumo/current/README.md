# SUMO — versão oficial de trabalho

Estado inicial: cópia de migração, não georreferenciada.

- `integration/`: bridge e integração SUMO/TraCI.
- `scenario/legacy-local/`: cenários locais existentes, preservados.
- `scenario/georeferenced/`: destino para uma nova rede georreferenciada.
- `evidence/`: resultados e relatórios persistentes.

A bridge poderá ainda referenciar caminhos antigos. Auditar e validar
os caminhos de runtime antes de alterar a origem usada pelas execuções.
Não atribuir WGS84 ou orientação ENU às coordenadas locais sem evidência.
