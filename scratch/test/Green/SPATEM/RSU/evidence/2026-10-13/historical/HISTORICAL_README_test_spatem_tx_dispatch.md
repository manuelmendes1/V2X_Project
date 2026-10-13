> Nota histórica: este documento pertence à organização anterior dos testes. As instruções atuais estão em `scratch/test/Green/SPATEM/RSU/README.md`.

# Teste SPATEM — Despacho para o serviço de transmissão

## Objetivo

Verificar se existe um chamador ativo de `SpatemService::Send()`
fora da própria implementação do serviço.

A existência de `SpatemService`, do encoder e do endpoint RSU não
prova que o estado de um semáforo SUMO chega ao serviço de transmissão.

## Código testado

`test_spatem_tx_dispatch.py` procura referências C++ ao chamador
`SpatemService::Send()` em `scratch/v2x` e
`scratch/vanetza_integration`, excluindo a implementação do serviço.

## Executar apenas este teste

A partir da raiz do ns-3:

```bash
python3 -u scratch/vanetza_integration/test/run_tests.py \
  --test red/spatem/rsu/test_spatem_tx_dispatch.py
