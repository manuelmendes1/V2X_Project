---
title: "Etapa 10 — CPM — Closeout"
project: "V2X_Project"
stage: "ETAPA_10"
status: "CLOSED_WITH_SCOPE_LIMITS"
date: "2026-11-03"
baseline_commit: "449f51a86ce880fe6d0b7f13969010ad5f6f8bf1"
branch: "docs/etapa10-cpm-closeout"
---

# Etapa 10 — CPM — Relatório de Encerramento e Limitações

## 1. Objetivo e decisão de encerramento

Este documento consolida o estado da Etapa 10 — CPM no baseline `449f51a`, após a integração da PR #18.

A decisão registada é **CLOSED_WITH_SCOPE_LIMITS**: a baseline de implementação CPM e as respetivas evidências de validação ficam documentadas e estabilizadas para dar continuidade ao projeto. Esta decisão não representa certificação ETSI, interoperabilidade OTA formal, segurança criptográfica de produção para todos os protocolos ou validação de todas as combinações simultâneas de serviços.

O closeout distingue:

- comportamento demonstrado por testes e execuções registadas;
- comportamento parcialmente validado ou condicionado pelo cenário;
- capacidades para as quais não existe evidência suficiente neste baseline.

A existência de código, compilação ou contadores de runtime, isoladamente, não é aceite como prova de comportamento funcional completo.

## 2. Baseline e alterações integradas

| Elemento | Referência |
|---|---|
| Repositório | `manuelmendes1/V2X_Project` |
| Baseline de `main` | `449f51a86ce880fe6d0b7f13969010ad5f6f8bf1` |
| Integração mais recente | PR #18 |
| PR #18 | [feat(denm): support scheduled events and configurable initial TX delay](https://github.com/manuelmendes1/V2X_Project/pull/18) |
| Commit de implementação da PR #18 | `4f897424d4f323dcdc9b2d61da036450dedd1ecd` |
| Commit de merge | `449f51a86ce880fe6d0b7f13969010ad5f6f8bf1` |

A PR #18 integrou suporte a eventos DENM agendados e a um atraso inicial configurável de transmissão. A alteração não substitui o timestamp de trigger do evento.

O parâmetro `--denmInitialTxDelayUs` define o atraso inicial, em microssegundos, entre a aceitação do evento e a primeira transmissão. O valor por omissão é zero.

A separação de origem dos eventos foi preservada:

- eventos originados pelo cenário são submetidos quando o instante configurado de trigger é atingido e mantêm a origem `SCENARIO`;
- o caminho legado `DENM_EVENT` mantém o comportamento imediato e a origem `SUMO`.

## 3. Evidência documental por fase

Os relatórios abaixo são a fonte primária para os detalhes técnicos, limites e evidências específicas de cada fase. Este closeout não substitui esses relatórios.

| Fase | Evidência | Leitura para o closeout |
|---|---|---|
| 10.1 — Auditoria API R2 / ASN.1 | [ETAPA_10_1_API_R2_AUDIT.md](ETAPA_10_1_API_R2_AUDIT.md) | Auditoria da API R2, módulos ASN.1 e bibliotecas Vanetza utilizadas. |
| 10.2 — Estações e identidades | [ETAPA_10_2_ESTACAO_IDENTIDADES.md](ETAPA_10_2_ESTACAO_IDENTIDADES.md) | Modelo de identidade e separação entre identidade ITS, endereço GeoNetworking e identificador SUMO/ns-3. |
| 10.3 — Modelo neutro | [ETAPA_10_3_MODELO_NEUTRO.md](ETAPA_10_3_MODELO_NEUTRO.md) | Contrato semântico independente do ASN.1 e separação entre dados de entrada e mensagem. |
| 10.4 — Coordenadas e tempo | [ETAPA_10_4_FONTE_COORDENADAS_TEMPO.md](ETAPA_10_4_FONTE_COORDENADAS_TEMPO.md) | Validou componentes de transformação e quantização, mas mantém limitações de integração geográfica e temporal descritas no próprio relatório. |
| 10.5 — Gestão da geração | [ETAPA_10_5_GESTAO_GERACAO.md](ETAPA_10_5_GESTAO_GERACAO.md) | Evidência da gestão de geração e inclusão de objetos. |
| 10.6 — Codec ASN.1 / UPER | [ETAPA_10_6_CODEC_ASN1_UPER.md](ETAPA_10_6_CODEC_ASN1_UPER.md) | Evidência do codec e dos contratos de codificação/descodificação. |
| 10.9 — Receção e entrega | [ETAPA_10_9_RECECAO_ENTREGA.md](ETAPA_10_9_RECECAO_ENTREGA.md) | Receção CPM validada no cenário documentado, com limitações explícitas de alcance e contexto. |
| 10.10 — Segurança e replay | [ETAPA_10_10_SEGURANCA_REPLAY.md](ETAPA_10_10_SEGURANCA_REPLAY.md) | Evidência de verificação criptográfica e proteção contra replay para os AIDs efetivamente cobertos. |
| 10.14 — SUMO perception E2E | [ETAPA_10_14_SUMO_PERCEPTION_E2E.md](ETAPA_10_14_SUMO_PERCEPTION_E2E.md) | Evidência da integração da fonte de perceção com o cenário SUMO, segundo o âmbito e as condições do relatório. |
| 10.16 — Regressão runtime | [ETAPA_10_16_REGRESSAO_RUNTIME_CAM_DENM_SPATEM.md](ETAPA_10_16_REGRESSAO_RUNTIME_CAM_DENM_SPATEM.md) | Relatório histórico da regressão CAM/DENM/SPATEM. Deve ser lido em conjunto com as evidências posteriores desta secção. |

### 3.1. Coordenadas e tempo

A implementação passou a dispor de transformações geodésicas e temporais dedicadas, incluindo a transformação WGS84 → ENU com GeoLib e histórico persistente de tracks.

O relatório da fase 10.4 identifica, contudo, limitações que não devem ser apagadas por este closeout: a origem geográfica e a orientação efetivas no contexto SUMO, a semântica temporal contínua/preditiva e a relação entre os relógios SUMO, ns-3 e ITS devem ser avaliadas de acordo com a evidência específica da integração.

A existência de testes unitários de transformação não prova, por si só, que todas as referências geográficas e temporais de um cenário real estão corretamente alinhadas.

### 3.2. Receção e entrega CPM

O relatório 10.9 regista uma execução de curto alcance com 92 entregas em 92 receções contabilizadas nesse cenário.

Este resultado é válido para as condições e geometria testadas. Não deve ser generalizado para todas as distâncias, densidades, condições de rádio ou topologias.

## 4. Segurança e replay

O relatório 10.10 documenta a utilização de assinatura/verificação Vanetza V3, cadeia de certificados e TrustStore para CPM (AID 639), incluindo:

- teste de mensagens adulteradas: 70 de 70 casos rejeitados na verificação criptográfica;
- teste de replay: 70 duplicados rejeitados;
- teste de mensagens não repetidas: 70 entregas normais no cenário correspondente;
- verificação criptográfica DENM no AID 37.

Estes resultados demonstram os caminhos cobertos pelos testes, não uma garantia global de segurança para todos os AIDs.

O mesmo relatório identifica uma limitação importante: AIDs sem integração criptográfica específica podem recorrer ao `DummyVerifyService`. Por isso, não se deve afirmar que CAM, SPATEM, CPM e DENM dispõem todos do mesmo nível de verificação criptográfica de produção.

A validação de segurança deve continuar a distinguir claramente:

- assinatura e verificação criptográfica efetivamente executadas;
- aceitação por um serviço de verificação de teste;
- autorização/validação de permissões e certificados;
- deteção de replay;
- entrega à aplicação.

Uma passagem num destes passos não prova automaticamente os restantes.

## 5. Regressão runtime CAM, DENM e SPATEM

### 5.1. Execução tripla sem supressão SPATEM

Na execução registada em 2026-11-03:

`test-results/cpm_etapa10_18_triple_denm_phase50ms/triple-event3s.85ht9d`

foram observados os seguintes contadores:

| Serviço / contador | Valor |
|---|---:|
| CAM TX | 85 |
| CAM RX | 77 |
| CAM DELIVER | 77 |
| DENM INGEST | 1 |
| DENM TX | 4 |
| DENM BTP RX | 40 |
| DENM DELIVER | 1 |
| SPATEM TX | 56 |
| SPATEM DELIVER | 74 |
| PHY RX DROP | 87 |

Os três serviços apresentaram atividade de transmissão e entrega nesta execução. Os contadores são métricas observadas pelo respetivo executor/analisador e não devem ser interpretados como uma correspondência universal de um para um entre transmissões e entregas. Em particular, `PHY_RX_DROP` é um contador de perdas na camada PHY e não identifica, por si só, o protocolo ou a causa de cada perda.

Esta evidência atualiza o diagnóstico runtime anterior, que tinha identificado ausência de entrega CAM e DENM na combinação tripla então testada. O relatório 10.16 mantém valor histórico; os resultados novos devem ser considerados em conjunto com ele, não usados para alterar retroativamente o que foi observado na execução antiga.

### 5.2. Execução com atraso inicial DENM

Na validação do parâmetro `--denmInitialTxDelayUs=25000`, foram observados:

| Serviço / contador | Valor |
|---|---:|
| CAM TX | 85 |
| CAM RX | 77 |
| CAM DELIVER | 77 |
| DENM INGEST | 1 |
| DENM TX | 4 |
| DENM BTP RX | 31 |
| DENM DELIVER | 1 |
| SPATEM TX | 56 |
| SPATEM DELIVER | 73 |
| PHY RX DROP | 90 |

O evento tinha timestamp de trigger em 6.050 s. A primeira transmissão DENM foi observada em 6.080 s, após a aceitação do evento em 6.050 s.

O atraso observado foi de aproximadamente 30 ms para um atraso configurado de 25 ms, compatível com a granularidade de polling do scheduler de 10 ms. O timestamp de trigger não foi reescrito pelo atraso inicial.

A primeira mensagem percorreu verificação, descodificação, controlo de replay e entrega. As transmissões repetidas seguintes foram rejeitadas pelo mecanismo de replay, de acordo com a evidência registada. A existência de quatro transmissões não deve ser confundida com quatro entregas à aplicação.

### 5.3. Ensaio diagnóstico com supressão SPATEM

A execução:

`test-results/cpm_etapa10_18_triple_denm_phase50ms/triple-event3s.nsUCqI`

foi realizada com `V2X_SPATEM_SUPPRESS_TX=1`.

Nessa variante, SPATEM TX e entrega foram zero, enquanto CAM e DENM continuaram ativos. Trata-se de um ensaio diagnóstico do caminho de supressão, **não** de uma execução tripla válida para demonstrar funcionamento simultâneo dos três serviços.

## 6. Suite de testes e build

A execução Green registada para esta baseline utilizou:

```bash
python3.14 scratch/test/run_tests.py
```

Resultado registado:

- 30 ficheiros de teste;
- 44 testes executados;
- 0 falhas;
- 0 erros.

A validação da PR #18 também registou build do target de produção. O build limpo foi documentado na validação anterior da entrada de eventos de cenário; a alteração do atraso inicial foi validada com build incremental. Este closeout não afirma que foi repetido um clean build integral depois de todas as alterações da PR #18.

A execução da suite Green não substitui a validação específica do `v2x_analyzer`. Não se declara aqui uma passagem integral e atualizada de todos os testes e métricas do analyzer sem evidência explícita dessa execução.

## 7. Limitações que permanecem

As seguintes limitações devem acompanhar a baseline e não podem ser interpretadas como funcionalidades já demonstradas:

1. **Segurança por protocolo:** a cobertura criptográfica comprovada é específica dos AIDs e serviços identificados no relatório 10.10. Não há evidência para afirmar verificação criptográfica de produção uniforme em todos os protocolos.
2. **PKI por protocolo:** cada protocolo que necessite de credenciais deve ter um espaço de certificados próprio, permissões e trust anchors adequados. Não reutilizar automaticamente certificados DENM para VAM ou para outro protocolo.
3. **Georreferenciação e tempo:** a validade de transformações unitárias não elimina as limitações de integração geográfica, orientação e sincronização temporal identificadas nos relatórios 10.4 e 10.14.
4. **Combinações runtime:** há evidência de execução tripla CAM/DENM/SPATEM e evidência CPM em campanhas próprias. Este closeout não declara demonstrada uma execução simultânea CAM+DENM+SPATEM+CPM.
5. **Analyzer:** não se declara concluída uma validação integral e atualizada do `v2x_analyzer` sem relatório de execução correspondente.
6. **Conformidade e interoperabilidade:** não foi demonstrada, por este conjunto de evidências, certificação ETSI, ensaio formal de conformidade ou interoperabilidade OTA com equipamentos independentes.
7. **Escala e condições de rádio:** os contadores observados pertencem aos cenários documentados e não constituem uma caracterização geral de desempenho, escalabilidade ou robustez de rádio.

## 8. Requisitos a transportar para a etapa VAM

A etapa VAM deve preservar a arquitetura comum de transporte, identidade, segurança, receção, replay, `PacketContext`, logging e análise. Não deve introduzir um pipeline alternativo específico de VAM.

A segurança é um requisito obrigatório de desenho e validação. O plano VAM deve incluir uma fase explícita de auditoria e provisionamento de PKI específica do protocolo.

A organização proposta é um subdiretório próprio, por exemplo `certificates/VAM/`, separado da estrutura `validation-denm-pki/` existente no laboratório. O caminho definitivo deve ser confirmado contra a estrutura real do repositório/laboratório antes de criar os artefactos.

O plano deve definir, para VAM:

- trust anchors e cadeia de autoridade aplicável;
- credenciais e permissões ITS-AID específicas;
- associação entre credencial, tipo de estação e perfil VRU;
- validade, expiração, rotação de pseudónimos e política de revogação;
- testes de assinatura, verificação, autorização e replay;
- fixtures e chaves exclusivamente de teste, quando aplicável.

Chaves privadas, credenciais de produção e segredos não devem ser incluídos no Git.

## 9. Critérios de encerramento e interpretação

Este closeout fecha documentalmente a baseline de implementação CPM e regista os resultados comprovados até ao merge da PR #18.

Não transforma limitações identificadas em critérios satisfeitos. Em particular:

- a validação CPM em SUMO não equivale a uma execução simultânea dos quatro serviços;
- o teste de segurança CPM/DENM não prova segurança de produção em todos os AIDs;
- uma suite Green não substitui a validação do runtime, do analyzer ou de interoperabilidade;
- os contadores de runtime não substituem a análise de continuidade de pacote e de cada etapa do pipeline.

Qualquer evolução posterior que altere estes resultados deve acrescentar nova evidência datada e ligada ao commit, sem apagar os resultados históricos.

## 10. Referências

- [Plano da Etapa 10 — CPM](ETAPA_10_CPM_PLANO.md)
- [Arquitetura do projeto](../../ARCHITECTURE.md)
- [Auditoria API R2 / ASN.1](ETAPA_10_1_API_R2_AUDIT.md)
- [Estações e identidades](ETAPA_10_2_ESTACAO_IDENTIDADES.md)
- [Modelo neutro](ETAPA_10_3_MODELO_NEUTRO.md)
- [Fonte, coordenadas e tempo](ETAPA_10_4_FONTE_COORDENADAS_TEMPO.md)
- [Gestão de geração](ETAPA_10_5_GESTAO_GERACAO.md)
- [Codec ASN.1 / UPER](ETAPA_10_6_CODEC_ASN1_UPER.md)
- [Receção e entrega](ETAPA_10_9_RECECAO_ENTREGA.md)
- [Segurança e replay](ETAPA_10_10_SEGURANCA_REPLAY.md)
- [SUMO perception E2E](ETAPA_10_14_SUMO_PERCEPTION_E2E.md)
- [Regressão runtime CAM/DENM/SPATEM](ETAPA_10_16_REGRESSAO_RUNTIME_CAM_DENM_SPATEM.md)
- [PR #18 — scheduled events e atraso inicial DENM](https://github.com/manuelmendes1/V2X_Project/pull/18)
