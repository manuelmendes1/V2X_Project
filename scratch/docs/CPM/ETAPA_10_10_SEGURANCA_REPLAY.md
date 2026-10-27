---
title: "Etapa 10.10 — CPM V3: Segurança e Replay"
status: "EVIDENCE_VALIDATED"
integration_status: "COMMITTED"
type: "FUNCTIONAL SECURITY VALIDATION REPORT"
date: "2026-10-27"
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
branch: "feature/cpm-etapa10-10-security-replay"
validation_head: "03b09a3"
security_aid: 639
btp_port: 2009
---

# Etapa 10.10 — CPM V3: Segurança e Replay

## 1. Estado e objetivo

A evidência funcional desta etapa foi validada em simulação E2E local. Foram demonstrados:

- transmissão CPM assinada e verificação criptográfica no recetor;
- rejeição de mensagens CPM após adulteração dos bytes transmitidos;
- rejeição de cópias exatas no filtro de replay do pipeline CPM;
- entrega das mensagens originais sem entrega adicional das cópias;
- preservação das regressões CPM abrangidas pelos testes executados.

**Estado:** evidência validada; revisão final do diff e integração no Git pendentes.

Esta classificação não equivale a encerramento formal da etapa, aprovação de interoperabilidade ETSI ou autorização para deployment.

A auditoria foi executada em 2026-10-27 sobre a branch indicada, com `HEAD=03b09a3` e alterações de trabalho ainda não commitadas.

## 2. Implementação abrangida

A integração CPM V3 utiliza serviços de assinatura e verificação Vanetza, incluindo validação de cadeia de certificados e TrustStore. O encaminhamento de verificação distingue o AID CPM 639 do fluxo DENM existente.

O código de integração e os testes abrangidos incluem:

- `scratch/vanetza_integration/wrappers/vanetza/security/cpm_v3_sign_service.cc`
- `scratch/vanetza_integration/wrappers/vanetza/security/cpm_v3_verify_service.cc`
- `scratch/vanetza_integration/wrappers/vanetza/security/v2x_v3_verify_service.cc`
- `scratch/vanetza_integration/cpm/rx/cpm_rx_pipeline.cc`
- `scratch/vanetza_integration/cpm/emitter/cpm_rsu_runtime.cc`
- `scratch/vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.cc`
- `scratch/vanetza_integration/wrappers/vanetza/geonet/vanetza_geonet_adapter.cc`
- integração de configuração em `scratch/v2x/v2x_sumo.cc` e no contexto de simulação.

O contrato de identidade mantém distintos `stationId`, GN_ADDR/MID e endereço MAC. A identidade da estação não é inferida do endereço MAC.

A fonte de dependências Vanetza é configurada através de `V2X_VANETZA_ROOT`, com bibliotecas de segurança R2 e dependências explícitas de ligação. A biblioteca Crypto++ está configurada através de `/usr/local/lib/libcryptopp.so` por omissão.

O `scratch/CMakeLists.txt` continua a definir `v2x_sumo` como o único executável de produção do diretório.

## 3. E2E de adulteração após assinatura

**Resultado: PASS**

Log de evidência:

`/tmp/cpm-v3-tamper-e2e.5GIfWP/ns3.log`

Parâmetros relevantes:

- `V2X_CPM_TAMPER=1`
- ITS-AID 639
- RSU `stationId=9001`
- porta BTP 2009

Resultados revalidados:

| Métrica | Resultado |
|---|---:|
| Transmissões aceites pelo emissor | 70 |
| Falhas de transmissão | 0 |
| Hooks de adulteração executados | 70 |
| Verificações criptográficas bem-sucedidas | 0 |
| Verificações criptográficas falhadas | 70 |
| Recepções CPM concluídas | 0 |
| Entregas CPM à aplicação | 0 |
| Objetos CPM entregues | 0 |

O hook modifica um byte do frame serializado após a assinatura e regista o offset e os valores original e adulterado. Em todas as 70 mensagens, o verificador registou `cryptographic=FAIL`. Não houve entregas à aplicação.

`TX_ACCEPTED=70` significa que o emissor aceitou as transmissões no seu pipeline; não significa que o recetor tenha aceite as mensagens adulteradas.

## 4. E2E de replay de cópias exatas

**Resultado: PASS**

Log de evidência:

`/tmp/cpm-v3-replay-e2e-2ms.xJTaev/ns3.log`

A duplicação é ativada apenas por `V2X_CPM_DUPLICATE=1`. Após a identificação de uma colisão na primeira execução com um atraso de 100 µs, o hook passou a agendar a cópia após 2000 µs. O novo E2E confirmou que o atraso resolve a lacuna observada nesta execução.

Resultados revalidados:

| Métrica | Resultado |
|---|---:|
| Transmissões originais aceites | 70 |
| Falhas de transmissão | 0 |
| Cópias emitidas | 70 |
| Cópias enviadas com 314 bytes | 70 |
| Verificações criptográficas bem-sucedidas | 140 |
| Verificações criptográficas falhadas | 0 |
| Rejeições `cpm_exact_payload_replay_detected` | 70 |
| Recepções CPM concluídas | 70 |
| Entregas CPM à aplicação | 70 |
| Objetos CPM entregues | 70 |

As 140 verificações correspondem às 70 mensagens originais e às 70 cópias. As cópias passaram pela verificação criptográfica, mas foram descartadas no estágio `REPLAY`, devido à deteção de payload repetido.

O resultado comprova 70 rejeições de replay e 70 entregas legítimas. Não comprova proteção contra todas as variantes de replay possíveis.

## 5. Testes, build e higiene

### Testes de segurança e wiring

Comando:

`python3 -m unittest scratch.test.Red.CPM.test_cpm_security_fault_injection scratch.test.Red.CPM.test_cpm_security_wiring -v`

Resultado: **11 testes passaram, 0 falharam**.

Cobertura relevante: hooks CPM de adulteração e duplicação, preservação dos hooks DENM, ligação do serviço de assinatura, ligação do verificador, encaminhamento por AID e integração das fontes no target CPM existente.

### Regressões CPM

Comando:

`python3 -m unittest scratch.test.Red.CPM.test_cpm_rx_pipeline scratch.test.Red.CPM.test_cpm_empty_perception_skip -v`

Resultado: **2 testes de unidade passaram**. O teste do pipeline registou 36 verificações aprovadas, incluindo rejeição de CPM não verificado, deteção de duplicados exatos, preservação de identidade, rejeição de mensagens malformadas e comportamento da fila de aplicação. O teste adicional verificou que uma perceção vazia é ignorada antes da inicialização do runtime e da transmissão.

### E2E de binding do certificado e chave CPM

**Resultados:** `CPM_CREDENTIAL_BINDING_NEGATIVE=PASS`;
`CPM_CREDENTIAL_BINDING_POSITIVE=PASS`.

**Artefactos:** `/tmp/cpm-key-binding-e2e.Vmoz3Z`.

No ensaio negativo, o par certificado/chave incompatível originou
70 rejeições por falha de binding e `RUNTIME_NOT_READY`, sem eventos
de verificação ou transmissões CPM aceites. No ensaio positivo,
o par válido produziu 70 verificações criptográficas PASS, 0 FAIL
e eventos de receção/entrega CPM. Os ficheiros originais da PKI
não foram substituídos.

### Builds com e sem hooks de teste

A build com `V2X_ENABLE_SECURITY_TEST_HOOKS=OFF` terminou com
código de saída 0. A inspeção de `build/scratch/ns3.47-v2x_sumo-default`
não encontrou marcadores de fault injection. As variáveis usadas
exclusivamente pelos hooks também não produziram os avisos de
variáveis não usadas.

A build com `V2X_ENABLE_SECURITY_TEST_HOOKS=ON` terminou igualmente
com código de saída 0 e contém os marcadores CPM, DENM e SPATEM esperados.

Os E2E pós-gate descritos abaixo foram executados com a opção ON. Após os ensaios, a cache foi reconfigurada e recompilada com a opção OFF; a build final não contém os marcadores de fault injection.

### Build de produção

Comando:

`cmake --build cmake-cache --target ns3.47-v2x_sumo-default -j2`

Resultado: código de saída 0. Na última auditoria incremental, Ninja indicou `no work to do`. O mesmo target foi ligado com sucesso após a alteração do atraso do hook CPM.

### Higiene

- `git diff --check`: PASS.
- Estado de `sumo/` antes e depois da auditoria: inalterado.
- Os hooks de fault injection CPM, DENM e SPATEM só são compilados
  com `V2X_ENABLE_SECURITY_TEST_HOOKS=ON`; o valor por omissão é `OFF`.
- Os marcadores estão ausentes na build `OFF` e presentes na build `ON`.
- Os E2E após o compile gate passaram e a build final ficou em OFF.

A ausência de alterações em `sumo/` nesta auditoria não significa que essa diretoria esteja limpa ou sem ficheiros locais; significa que o seu estado não mudou durante a auditoria.


### Regressão E2E após o compile gate

As execuções seguintes foram feitas com `V2X_ENABLE_SECURITY_TEST_HOOKS=ON`.
O bridge foi iniciado em diretórios temporários e com o caminho absoluto do
ficheiro `.sumocfg`, evitando usar `sumo/` como diretório de trabalho.

**Adulteração após assinatura — `CPM_TAMPER_POST_GATE=PASS`**

| Métrica | Resultado |
|---|---:|
| Código de saída ns-3 / bridge | 0 / 0 |
| Eventos `CPM_V3_TAMPER` | 70 |
| Verificações criptográficas PASS / FAIL | 0 / 70 |
| Eventos `CPM_RECEPTION_OK` | 0 |
| Eventos `CPM_DELIVER_OK` | 0 |

**Replay de cópias exatas — `CPM_REPLAY_POST_GATE=PASS`**

| Métrica | Resultado |
|---|---:|
| Código de saída ns-3 / bridge | 0 / 0 |
| Eventos `CPM_V3_DUPLICATE` | 70 |
| Verificações criptográficas PASS / FAIL | 140 / 0 |
| Rejeições `cpm_exact_payload_replay_detected` | 70 |
| Eventos `CPM_RECEPTION_OK` | 70 |
| Eventos `CPM_DELIVER_OK` | 70 |

**Configuração final:** `V2X_ENABLE_SECURITY_TEST_HOOKS=OFF`. O target
`ns3.47-v2x_sumo-default` foi recompilado com código de saída 0; a inspeção
de strings confirmou a ausência dos nomes e marcadores dos hooks. O estado
Git de `sumo/` permaneceu inalterado e `git diff --check` passou.

**Artefactos pós-gate:**
- Adulteração: `/tmp/cpm-etapa10-10-post-gate.ccZJf7/tamper/ns3.log` e `/tmp/cpm-etapa10-10-post-gate.ccZJf7/tamper/bridge.log`
- Replay: `/tmp/cpm-etapa10-10-post-gate.ccZJf7/replay/ns3.log` e `/tmp/cpm-etapa10-10-post-gate.ccZJf7/replay/bridge.log`
- Build final OFF: `/tmp/cpm-etapa10-10-post-gate.ccZJf7/build-final-off.log`
- Binário final OFF, strings: `/tmp/cpm-etapa10-10-post-gate.ccZJf7/binary-final-off.strings`

## 6. Configuração e evidências reproduzíveis

Fixture PKI usada:

`test-results/cpm_security_pki_20261026T122926Z.K0l7UX`

Configuração SUMO utilizada:

`sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`

Bridge:

`sumo/current/integration/v2x_bridge_16.py`

Logs dos testes e build da auditoria:

- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/security-tests.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/cpm-regressions.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/configure-off.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/build-off.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/binary-off.strings`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/configure-on.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/build-on.log`
- `/tmp/cpm-etapa10-10-hook-gating-fix.OgKsWI/binary-on.strings`

Os caminhos `/tmp` são locais e temporários. Devem ser preservados ou arquivados separadamente se for necessário manter as provas depois de uma limpeza do sistema.

## 7. Limites explícitos da validação

As evidências desta etapa não demonstram:

- interoperabilidade com equipamento ITS externo;
- conformidade formal com todos os requisitos ETSI;
- validação E2E de todas as falhas possíveis de certificados, cadeia de confiança, expiração e restrições geográficas;
- rejeição de replays modificados, replays após reinício ou comportamento da proteção fora da janela de deteção;
- segurança operacional de deployment em produção.

O filtro demonstrado para replay é de payload exato. As verificações criptográficas aprovadas nas cópias não significam que o serviço Vanetza, isoladamente, as rejeite como duplicadas.

A fixture de certificado usada não contém um campo `region`; por isso, esta execução não prova a rejeição de certificados fora de uma região autorizada.

**Limitação herdada do dispatcher de verificação:** `V2xV3VerifyService`
encaminha CPM AID 639 para `CpmV3VerifyService` e os restantes AIDs para
`DenmV3VerifyService`. Este último usa `StraightVerifyService` para DENM
AID 37, mas mantém um fallback `DummyVerifyService` com resultado de sucesso
para CAM e outros AIDs. Assim, esta etapa demonstra verificação criptográfica
real para CPM AID 639 e DENM AID 37, não para todos os AIDs V2X. Os restantes
AIDs necessitam de verificadores específicos ou de rejeição *fail-closed*
antes de qualquer deployment que dependa da sua autenticidade.


Os hooks permanecem no código-fonte para testes controlados, mas os blocos de
fault injection são excluídos da compilação quando
`V2X_ENABLE_SECURITY_TEST_HOOKS=OFF`. A opção está definida como `OFF`
por omissão. Os E2E após esta alteração passaram com o compile gate ON; a build foi depois reconfigurada para OFF.

A configuração de dependências contém caminhos específicos do ambiente local. A portabilidade para outro host requer validar os caminhos de Vanetza, Crypto++ e GeographicLib.

## 8. Integração no Git

A implementação e este relatório acompanham o commit local desta etapa.
O relatório foi incluído explicitamente porque a regra abrangente
`.gitignore:6:*` o ignora; essa regra geral não foi alterada.

O commit contém apenas os 22 caminhos explicitamente revistos e validados.
Os ficheiros locais e assets sob `sumo/` permanecem fora do commit.
Não foi realizado push.

O estado documental `COMMITTED` indica a integração no histórico Git;
não representa aprovação de interoperabilidade ETSI nem prontidão para
deployment.

## 9. Veredito

**EVIDENCE_VALIDATED — Etapa 10.10, segurança CPM V3 e replay.**

A assinatura e verificação criptográfica CPM, a rejeição de mensagens
adulteradas e a rejeição de cópias exatas foram demonstradas em simulação
E2E. Os testes de segurança, as regressões CPM e a regressão DENM abrangida
foram aprovados. A build final foi recompilada com
`V2X_ENABLE_SECURITY_TEST_HOOKS=OFF`, sem marcadores de fault injection.

A implementação e este relatório estão registados no commit local desta
etapa. Não foi feito push.

A limitação relativa aos restantes AIDs permanece: a evidência criptográfica
real abrange CPM AID 639 e DENM AID 37; não demonstra verificação
criptográfica universal nem segurança operacional para deployment.

<!-- BEGIN DENM_E2E_CURRENT_PKI -->

## Regressão E2E DENM com PKI temporalmente válida

**Resultado:** `DENM_E2E_REGRESSION=PASS`

**Execução:** 27 de outubro de 2026, com `itsStartTime=2026-10-27T10:11:03`.

A PKI temporária foi provisionada com a ferramenta
`tools/provision-denm-multi-vehicle.sh`. As cadeias dos tickets de
`veh0` e `veh1` foram validadas para AID 37 no instante de referência
configurado, com ambos os comandos de validação a terminarem com código 0.
Os ficheiros originais da PKI não foram substituídos.

O cenário de regressão utilizou o injector DENM histórico com dois veículos
a 20 metros de distância, CAM, SPATEM e CPM desativados, DENM ativado e
raio GBC de 1.000 metros.

| Métrica | Resultado |
|---|---:|
| Provisionamento da PKI | RC 0 |
| Validação de cadeia para AID 37 | 2/2 passaram |
| Inicializações do signer DENM real | 2 |
| Assinaturas DENM | 4 |
| Transmissões `ACCESS_TX_OK` | 4 |
| Eventos `ReceivePacket called` | 52 |
| Eventos `DENM_RX_DISPATCH` | 40 |
| Verificações criptográficas AID 37 — PASS | 40 |
| Verificações criptográficas AID 37 — FAIL | 0 |
| Eventos `DENM_DELIVER_OK` | 1 |
| Rejeições por credencial ativa ausente | 0 |
| Processo ns-3 / injector | RC 0 / RC 0 |

A evidência demonstra assinatura e transmissão DENM, verificação criptográfica
bem-sucedida e pelo menos uma entrega local. Os 40 eventos de verificação
não devem ser interpretados como 40 mensagens únicas entregues.

**Artefactos:** `/tmp/denm-current-pki-e2e.KRLQdu/ns3.log` e
`/tmp/denm-current-pki-e2e.KRLQdu/injector.log`.

**Limites:** esta execução valida o percurso local da integração. Não demonstra
interoperabilidade com equipamento ITS-G5 externo nem conformidade normativa.

O estado global da integração está registado como `COMMITTED`; os limites de interoperabilidade e segurança descritos neste relatório mantêm-se.
<!-- END DENM_E2E_CURRENT_PKI -->
