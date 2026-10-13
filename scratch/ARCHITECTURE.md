# Arquitetura oficial do projeto V2X

## Metadados
- Projeto: V2X_Project
- Repositorio: ns-3.47
- Branch de referencia: main
- Estado: arquitetura de referencia
- Ambito: integracao V2X, Vanetza, SUMO e testes CAM, DENM e SPATEM

## Estrutura de referencia

- `scratch/CMakeLists.txt`: configuracao dos alvos de compilacao.
- `scratch/vanetza_integration/`: integracao Vanetza e pipelines V2X.
- `scratch/v2x/`: aplicacoes e integracao V2X com SUMO.
- `scratch/v2x_analyzer/`: analise de logs e metricas V2X.
- `scratch/test/Green/CAM/`: testes CAM aprovados.
- `scratch/test/Green/DENM/`: testes DENM aprovados.
- `scratch/test/Green/SPATEM/`: testes SPATEM aprovados.
- `scratch/test/Red/CAM/`: testes negativos CAM.
- `scratch/test/Red/DENM/`: testes negativos DENM.
- `scratch/test/Red/SPATEM/`: testes negativos SPATEM.
- `scratch/vanetza_integration/docs/STAPEM.txt`: plano de referência SPATEM.
- `scratch/test/run_tests.py`: executor oficial das suites unittest.
- `scratch/test/Green/SPATEM/RSU/evidence/`: logs e evidência runtime.

## Principios arquiteturais

1. Preservar a separacao entre aplicacoes, integracao, testes e documentacao.
2. Distinguir testes positivos de testes negativos.
3. Preservar os alvos e dependencias definidos em CMake.
4. Manter rastreabilidade entre requisitos, testes, logs e evidencias.
5. Nao considerar um teste aprovado sem evidencia verificavel.
6. Nao eliminar historico ou testes existentes como efeito secundario
   de uma reorganizacao de diretorios.
7. Registar reorganizacoes de ficheiros com Git para preservar o historico.

## Limites de validacao

Este documento estabelece a arquitetura de referencia pretendida.
Nao demonstra, por si so, que todos os diretorios existem, que os
ficheiros foram migrados, que o projeto compila ou que os testes passam.
Esses resultados exigem verificacao independente.
