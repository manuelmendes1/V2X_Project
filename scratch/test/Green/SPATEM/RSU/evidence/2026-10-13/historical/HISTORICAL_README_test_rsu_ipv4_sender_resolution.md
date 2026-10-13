> Nota histórica: este documento pertence à organização anterior dos testes. As instruções atuais estão em `scratch/test/Green/SPATEM/RSU/README.md`.

# Teste SPATEM/RSU — Resolução da identidade IPv4 do emissor

## 1. Código testado

Ficheiro de teste:
`test_rsu_ipv4_sender_resolution.py`

O teste utiliza o módulo `unittest` do Python e inspeciona o
código-fonte do caminho de receção em:

`vanetza_runtime/vanetza_runtime.cc`

Procura a resolução do emissor IPv4 através de
`VehicleRegistry::GetByIpv4(sourceIp)`, com resultado do tipo
`VehicleState*`.

## 2. Objetivo

Verificar que a receção não depende exclusivamente de uma identidade
de veículo SUMO para representar o emissor de um pacote.

Uma RSU é uma estação ITS e não deve precisar de uma identidade
fictícia de veículo SUMO para ser identificada.

O objetivo final é permitir a resolução da identidade de veículos
e RSUs no caminho de receção, preservando as diferenças entre os
respetivos modelos de identidade.

## 3. Tipo de teste e limitações

Este é um teste estrutural: procura um padrão específico no código
C++.

Não compila nem executa o caminho de receção C++, não transmite
pacotes SPATEM e não demonstra receção funcional de uma RSU.

A ausência do padrão pesquisado também não prova, por si só, que
a nova resolução de identidade funciona corretamente. A
implementação terá de ser verificada posteriormente com testes
funcionais.

## 4. Comando para executar apenas este teste

A partir da raiz do ns-3:

```bash
python3 -u scratch/vanetza_integration/test/run_tests.py \
  --test red/spatem/rsu/test_rsu_ipv4_sender_resolution.py
```

## 5. Comando para executar e guardar o log

```bash
LOG="scratch/vanetza_integration/test/red/spatem/rsu/test_rsu_ipv4_sender_resolution.log"

python3 -u scratch/vanetza_integration/test/run_tests.py \
  --test red/spatem/rsu/test_rsu_ipv4_sender_resolution.py \
  2>&1 | tee "$LOG"

TEST_STATUS=${PIPESTATUS[0]}
printf '\nTEST_EXIT_CODE=%s\n' "$TEST_STATUS" | tee -a "$LOG"
```

O comando de captura deve ser executado em Bash. O ficheiro de log
é substituído a cada nova execução; o código de saída é acrescentado
no final.

## 6. Objetivos do log

O ficheiro `.log` deve preservar:

- O comando utilizado para executar o teste.
- O número de testes descobertos e executados.
- O resultado PASS ou FAIL.
- A mensagem de asserção e o traceback, caso exista uma falha.
- O código de saída do processo.

Este registo permite relacionar o resultado com o código testado,
comparar execuções anteriores e confirmar se a alteração futura
resolveu a condição detetada.

## 7. Resultado observado antes da implementação

Na execução inicial, foi executado um teste, que falhou com código
de saída 1.

A falha foi a esperada para a fase RED: o código-fonte ainda contém
a resolução IPv4 dependente de `VehicleState*`.

Isto documenta a condição encontrada, não uma falha do runner nem
uma prova de que a receção SPATEM por RSU já funciona.

## 8. Critério para continuar

Depois de modificar a implementação, voltar a executar exatamente
este teste.

A passagem do teste é necessária para remover o padrão estrutural
detetado, mas não é suficiente para declarar concluída a validação
funcional da identidade RSU.

## Configuração opcional do endpoint RSU

A inicialização da rede RSU é opt-in e está desativada por omissão.
Para a ativar, o executável aceita os parâmetros:

- `--enableSpatem=1`
- `--spatemStationId=<stationId exclusivo>`
- `--spatemIntersectionId=<ID entre 1 e 65535>`
- `--spatemTrafficLightId=<ID exato do semáforo SUMO>`

Os três identificadores são obrigatórios quando SPATEM está ativado.
A RSU é inicializada quando chega uma atualização SUMO com o ID configurado.

Esta etapa configura e regista o endpoint de rede. Não implementa ainda
a transmissão SPATEM através do serviço e do transporte Vanetza, nem prova
receção funcional por uma simulação.
