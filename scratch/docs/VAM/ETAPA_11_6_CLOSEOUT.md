---
title: "Etapa 11.6 — VAM Transaction Lifecycle — Closeout"
date: 2026-11-16
status: validated_with_explicit_scope
tags:
  - v2x
  - vam
  - ns-3
  - validation
  - closeout
related:
  - "[[2026-11-16-0845-feat-vam-transaction-lifecycle-plan]]"
---

# Etapa 11.6 — VAM Transaction Lifecycle — Closeout

## 1. Resultado

**Estado: VALIDATED, com limites de evidência explicitados.**

A Etapa 11.6 implementa e valida os contratos de ciclo de vida das transações VAM: identidade transacional composta, invalidação de presenças, retenção de transações cujo aborto falhou, recuperação por retry, drain global e barreiras fail-aware para reconfiguração, desativação e shutdown.

A validação integrada confirma que o comportamento de produção permanece em `EVALUATION_ONLY`: os candidatos avaliados são abortados, não são confirmados como transmissões bem-sucedidas e não entram no pipeline TX VAM.

A etapa **não implementa nem declara operacional o TX VAM**. Encode, assinatura, BTP, GeoNetworking, ACCESS e transmissão permanecem fora do âmbito.

Plano de referência: `2026-11-16-0845-feat-vam-transaction-lifecycle-plan.md`.

## 2. Resultado por unidade

| Unidade | Resultado | Evidência principal |
|---|---|---|
| U1 — Identity-aware transaction handles | PASS | Harness C++ do scheduler e lifecycle; rejeição de handles obsoletos e colisões de identificadores locais |
| U2 — Retired transaction ledger, REMOVE e quarentena | PASS | Testes C++ com falhas de aborto, retenção e retry; harness UDP com REMOVE e reentrada |
| U3 — Drain, reconfiguração, desativação e shutdown | PASS | Testes C++ de drain, incluindo falhas injetadas; testes de wiring do controlador |
| U4 — Regressão e aceitação de produção | PASS, com evidência separada por camada | 278 testes, build GCC 11 e E2E com SUMO real |

## 3. Evidência de ciclo de vida

### 3.1 Identidade transacional — U1

O handle de geração identifica a instância do scheduler, a presença concreta da pessoa e a transação local do manager.

Os testes verificam que:
- Um handle de uma presença anterior não pode confirmar uma transação após REMOVE e reentrada.
- A reutilização do identificador local pelo novo manager não torna válido um handle antigo.
- A substituição da instância do scheduler não permite reutilizar handles anteriores.
- A identidade ITS permanece separada da identidade de transporte e do `stationId`.

O contrato de identidade continua a respeitar:

`Link MAC != GN_ADDR.MID != stationId`

### 3.2 REMOVE, quarentena e reentrada — U2

A remoção aceite invalida a presença VAM antes da sincronização do sujeito VRU derivado. A falha de sincronização VRU não anula a remoção aceite.

Os testes cobrem:
- Retenção de uma transação cujo aborto falhou.
- Quarentena da pessoa afetada, sem bloquear a avaliação dos restantes sujeitos.
- Retry de abortos pendentes.
- Recuperação após aborto bem-sucedido.
- Criação de um runtime e histórico novos para a presença reintroduzida.
- Preservação da reserva de identidade ITS durante remoção, recuperação e reentrada.

O harness UDP utiliza o `SumoAdapter` real e datagramas enviados por um injector local. Demonstra a sequência de presença, `STEP_END`, avaliação/aborto, REMOVE e reentrada, incluindo a preservação da reserva de identidade e a ausência de candidatos pendentes no fluxo normal.

**Limite da evidência:** este harness valida a integração UDP e os callbacks do adapter, mas não representa uma execução TraCI real do SUMO a gerar a sequência REMOVE/reentrada.

### 3.3 Drain e operações globais — U3

O harness C++ de lifecycle valida:
- Drain das transações ativas e retiradas.
- Rejeição de handles obsoletos depois do drain.
- Preservação das reservas de identidade ITS.
- Idempotência do drain.
- Falhas parciais de aborto com retenção dos registos ainda não resolvidos.
- Recuperação através de tentativas posteriores, sem perder managers ou transações ainda relevantes.

Os testes de wiring do controlador verificam a ordem das operações:
- Desativação apenas depois de drain bem-sucedido.
- Construção/validação da nova configuração antes da substituição do scheduler atual.
- Shutdown fail-aware, sem prosseguir com a destruição global dos recursos se o shutdown VAM falhar.
- Ausência de limpeza prematura da autoridade global de identidade.

Os testes de wiring são verificações estruturais do código. Não constituem, por si só, uma demonstração runtime de falhas injetadas diretamente no controlador.

## 4. Regressão integrada

Execução:

`python3 scratch/test/run_tests.py --suite all`

Resultado:

```text
Ran 278 tests
OK
Resumo: 278 executado(s), 0 falha(s), 0 erro(s).
VAM_FULL_GREEN_RED_REGRESSION_RC=0
```

A regressão inclui os testes VAM, os contratos de scheduler e lifecycle, os testes de identidade e as suites Green e Red existentes.

Os testes focados de scheduler, lifecycle e wiring também passaram, incluindo os marcadores:

```text
VAM_GENERATION_SCHEDULER=PASS
VAM_TRANSACTION_LIFECYCLE=PASS
VAM_TRANSACTION_RETIREMENT=PASS
VAM_TRANSACTION_DRAIN=PASS
```

## 5. Build de produção

Foi executada uma compilação limpa do target de produção com GCC 11.

Target:

`ns3.47-v2x_sumo-default`

Resultado:

```text
[1134/1134] Linking ... ns3.47-v2x_sumo-default
VAM_PRODUCTION_BUILD_RC=0
VAM_PRODUCTION_EXECUTABLE=PASS
```

A compilação e ligação completas passaram.

## 6. Aceitação runtime com SUMO real

Foi executado o executável de produção com a bridge VAM e uma instância real de SUMO.

Resultado:

```text
VAM_SUMO_BRIDGE_RC=0
VAM_NS3_RUNTIME_RC=0
VAM_STEP_EVALUATED_COUNT=120
VAM_CANDIDATE_EVALUATED_COUNT=120
VAM_CANDIDATE_ABORTED_COUNT=120
VAM_SHUTDOWN_COMPLETE_COUNT=1
SUMO_STEP_PUBLISHED_COUNT=120
VAM_TX_MARKER=NONE
VAM_SUMO_PRODUCTION_E2E=PASS
```

A execução confirma:
- 120 passos SUMO publicados.
- 120 avaliações VAM.
- 120 candidatos abortados.
- Encerramento normal da bridge e do processo ns-3.
- Um shutdown VAM concluído.
- Ausência de marcadores de transmissão VAM.

### Limite da execução SUMO

O cenário executado teve duração de 12 segundos de simulação. Os peões não chegaram naturalmente ao fim do percurso nesse intervalo; por isso, esta execução não produziu um evento REMOVE/reentrada natural.

A sequência REMOVE/reentrada foi validada separadamente pelo harness UDP descrito na secção 3.2. Não se deve interpretar o E2E com SUMO real como evidência de que esses eventos ocorreram durante os 120 passos.

As falhas injetadas de aborto e a recuperação foram demonstradas nos harnesses C++, não nesta execução normal de produção.

## 7. Verificação de EVALUATION_ONLY e ausência de TX

A Etapa 11.6 mantém o contrato `EVALUATION_ONLY`.

A auditoria dos logs E2E apresentou:

```text
CANDIDATE_ABORT_FAILED_COUNT=0
SHUTDOWN_BLOCKED_COUNT=0
CANDIDATE_HANDLE_MISSING_COUNT=0
VAM_CONFIGURATION_REJECTED_COUNT=0
VAM_TX_STAGE_COUNT=0
BRIDGE_CLOSED_COUNT=1
```

Os candidatos foram abortados e não confirmados como transmissões bem-sucedidas.

Não existe evidência de execução de encode, assinatura, BTP, GeoNetworking, ACCESS ou envio VAM. A ausência desses marcadores é coerente com a fronteira funcional desta etapa.

## 8. Integridade documental e da árvore SUMO

### 8.1 Plano e whitelist documental

O conteúdo decisório do plano permaneceu inalterado. Na preparação do commit foi removida apenas a linha vazia excedente no fim do ficheiro:

```text
SHA256:
6096fcf745f468159c9c87fcddf40e75c5f4e8b1c655cf5c359d44089d8b3d54
```

O `.gitignore` contém exceções específicas apenas para:

- `scratch/docs/VAM/2026-11-16-0845-feat-vam-transaction-lifecycle-plan.md`
- `scratch/docs/VAM/ETAPA_11_6_CLOSEOUT.md`

O teste de whitelist confirmou que ambos os documentos são visíveis ao Git e que um documento VAM fora da whitelist continua ignorado.

### 8.2 Integridade de SUMO

O ficheiro autorizado para inspeção read-only manteve o hash:

```text
e9d517f95037fb8766c210b676d01aaffa52a1f3538df1982ec9c97596414d61
```

As verificações do worktree e do index para `sumo/` devolveram `RC=0`. Não foram necessárias alterações na árvore SUMO.

### 8.3 Higiene

As verificações reportaram:

```text
TRACKED_DIFF_CHECK_RC=0
STAGED_DIFF_CHECK_RC=0
UNTRACKED_TEST_WHITESPACE=PASS
```

Os quatro novos ficheiros de teste VAM passaram a verificação de whitespace.

## 9. Limites e interpretação final

A evidência desta etapa deve ser interpretada por camada:

1. Os harnesses C++ demonstram os contratos de identidade, lifecycle, drain, falhas injetadas e recuperação.
2. O harness UDP demonstra a integração com o `SumoAdapter` e a sequência REMOVE/reentrada através de datagramas controlados.
3. O E2E de produção demonstra a execução com bridge e SUMO reais, com 120 avaliações/abortos e shutdown normal.
4. Os testes de wiring demonstram a ligação estrutural das barreiras de lifecycle ao controlador; não substituem testes runtime de falha do controlador.
5. Nenhuma destas evidências demonstra ou autoriza TX VAM.

Os logs brutos do E2E foram produzidos em:

- `/tmp/vam-etapa11-6-sumo-e2e.VeWmrm/ns3.log`
- `/tmp/vam-etapa11-6-sumo-e2e.VeWmrm/bridge.log`

Estes caminhos são temporários. Os logs devem ser preservados numa localização de evidência apropriada antes de qualquer limpeza do diretório temporário, caso seja necessário manter os artefactos brutos para auditoria futura.

## 10. Decisão de closeout

**Etapa 11.6 validada para o âmbito de lifecycle transacional VAM e execução `EVALUATION_ONLY`.**

Os contratos de identidade, remoção, reentrada, retenção, retry, drain e shutdown foram validados através da combinação de testes comportamentais, harnesses de integração UDP, regressão completa, build de produção e E2E SUMO.

A etapa não implementa TX VAM e não deve ser interpretada como autorização para confirmar candidatos como transmissões concluídas.

Qualquer evolução para TX deve definir separadamente o evento de sucesso que autoriza `ConfirmGeneration`, a sua relação com `TX_COMPLETE` e os critérios de aceitação do pipeline de transmissão.

**Sem staging, commit ou push incluídos neste closeout.**
