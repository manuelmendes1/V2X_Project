# SUMO — versão oficial de trabalho

Estado inicial: cópia de migração, não georreferenciada.

- `integration/`: bridge e integração SUMO/TraCI.
- `scenario/legacy-local/`: cenários locais existentes, preservados.
- `scenario/georeferenced/`: destino para uma nova rede georreferenciada.
- `evidence/`: resultados e relatórios persistentes.

A bridge poderá ainda referenciar caminhos antigos. Auditar e validar
os caminhos de runtime antes de alterar a origem usada pelas execuções.
Não atribuir WGS84 ou orientação ENU às coordenadas locais sem evidência.

## Evidências E2E de transporte SUMO/NS-3

As evidências de transporte estão organizadas pelo sentido da comunicação.
Os dois sentidos foram validados em campanhas E2E distintas; não representam
um único ensaio bidirecional simultâneo.

### SUMO/TraCI → ns-3

- [E2E de estado de veículo com coordenadas WGS84](evidence/e2e-famalicao-wgs84-state/20261022T091547Z/README.md): validação do transporte e persistência das coordenadas geográficas.
- [E2E de estado de veículo e timestamp lógico](evidence/e2e-famalicao-udp-ns3/20261022-tls-11645226075/README.md): validação da receção UDP e correlação dos timestamps SUMO.

### ns-3 → Python bridge

- [E2E do dispatcher de retorno — 2026-11-20](evidence/e2e-ns3-return-dispatch/20261120T121453Z/README.md): 39 datagramas enviados, 39 recebidos, 39 reconciliados e 39 estados aceites; regressão da bridge com 50/50 testes PASS.
- [Validação complementar do contrato C++/Python — 2026-11-20](evidence/e2e-ns3-return-dispatch/contract-validation/20261120T122200Z/README.md): três testes específicos e regressão completa da bridge com 53/53 testes PASS.
- [Manifesto SHA-256](evidence/e2e-ns3-return-dispatch/20261120T121453Z/MANIFEST.sha256): integridade dos artefactos arquivados.
- [Verificador independente](evidence/e2e-ns3-return-dispatch/20261120T121453Z/verify_e2e.py): permite recalcular a reconciliação a partir da captura `strace` e do log da bridge.

O dispatcher de retorno é exclusivamente observacional. O payload contém
seis campos (`vehicle_id,x,y,speed,heading,accel`), sem timestamp lógico SUMO.
A evidência não demonstra atuação sobre a mobilidade TraCI, sincronização
temporal individual nem garantias gerais de entrega UDP.
