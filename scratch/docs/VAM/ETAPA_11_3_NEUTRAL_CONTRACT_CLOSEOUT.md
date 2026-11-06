---
type: project_closeout
project: VAM
stage: "11.3"
status: "PARTIAL / VALIDATED — NEUTRAL DOMAIN CONTRACT"
date: 2026-11-06
branch: feature/vam-etapa11-3-neutral-contract
baseline_commit: 6da8aa1a5f3152183053a736c68982bfd6096908
tags:
  - v2x
  - vam
  - closeout
  - neutral-domain-model
  - identity-separation
related:
  - "[[ETAPA_11_VAM_PLANO]]"
  - "[[ETAPA_11_1_AUDITORIA_CLOSEOUT]]"
  - "[[ETAPA_11_2_PERFIL_PAPEL_VRU_CLOSEOUT]]"
---

# Etapa 11.3 — Closeout do modelo neutro e separação de identidade

## 1. Decisão

**Estado: PARTIAL / VALIDATED — NEUTRAL DOMAIN CONTRACT**

Fica aceite o âmbito isolado do contrato de domínio VAM: modelo sem dependência direta de ASN.1, separação entre input e mensagem recebida, representação de duas origens ITS-S e separação estrutural das categorias de identidade.

Este closeout não declara concluída a gestão de identidade ITS. O critério original de unicidade e não colisão entre identidades não está integralmente demonstrado. A autoridade de identidade, o ciclo de vida de pseudónimos e as transições de identidade associadas a clusters permanecem por implementar e validar.

## 2. Baseline e estado Git

| Elemento | Evidência |
|---|---|
| Data da validação | 2026-11-06 |
| Branch | `feature/vam-etapa11-3-neutral-contract` |
| HEAD de baseline | `6da8aa1a5f3152183053a736c68982bfd6096908` |
| Referência remota na criação do branch | `origin/main` |
| Estado observado antes do commit | Ficheiros da implementação e testes não rastreados; sem alterações rastreadas reportadas |
| Commit da implementação | Ainda não criado à data desta validação |

Os artefactos existentes em `sumo/` foram preservados e não fazem parte do âmbito desta alteração.

**Nota de tracking:** o repositório utiliza uma regra `.gitignore` que ignora tudo por omissão (`*`). Por isso, os novos ficheiros, incluindo este documento, não aparecem automaticamente como ficheiros não rastreados. A inclusão no Git terá de ser explícita e limitada aos caminhos desta etapa. Não utilizar `git add .` nem `git add -A`.

## 3. Âmbito aceite

### 3.1 Modelo semântico neutro

Foram criados:

- `scratch/vanetza_integration/vam/types/vam_identity_types.h`
- `scratch/vanetza_integration/vam/types/vam_domain_types.h`
- `scratch/vanetza_integration/vam/types/vam_input.h`
- `scratch/vanetza_integration/vam/types/vam_message.h`

`VamSemanticData` representa o contrato semântico partilhado, incluindo a estação emissora, perfil e papel VRU, tempos, posição de referência, estado cinemático HF e extensões opcionais.

O modelo não depende diretamente de `VehicleState`, TraCI, Vanetza ou headers ASN.1 gerados. As categorias de identidade e os metadados operacionais de transporte e segurança não devem ser confundidos com os dados semânticos da mensagem.

`VamStationId` é um tipo próprio com representação `std::uint32_t`, sem conversão implícita para ou de um inteiro genérico.

### 3.2 Input e mensagem recebida

`VamInput` deriva de `VamSemanticData` e não acrescenta timestamp de receção.

`VamMessage` deriva do mesmo contrato semântico e acrescenta `receivedAtUnixMs`, representado por `std::int64_t`.

O timestamp de receção local permanece distinto dos tempos ITS de geração e de referência da posição.

### 3.3 Contexto de origem

Foi criado:

`scratch/vanetza_integration/vam/context/vam_origin_context.h`

`VamOriginContext` é uma variante com duas alternativas:

- `SumoPersonOrigin`: referência SUMO Person, referência ao nó ns-3 próprio e referência à fonte de mobilidade.
- `IndependentItsStationOrigin`: referência lógica à ITS-S independente e referência à sua fonte de mobilidade.

A proveniência permanece fora de `VamSemanticData`. As duas origens destinam-se a convergir para o mesmo contrato semântico, sem introduzir um pipeline VAM paralelo.

A existência destes tipos valida a representação estrutural das origens no contrato. Não demonstra a integração funcional SUMO Person → nó ns-3 → ITS-S.

### 3.4 Separação estrutural das identidades

Foram definidos cinco tipos fortes:

| Tipo | Significado |
|---|---|
| `VamStationId` | Identificador ITS da estação |
| `SumoPersonReference` | Referência à entidade SUMO Person |
| `Ns3NodeReference` | Referência ao nó ns-3 |
| `MobilitySourceReference` | Referência à fonte de mobilidade |
| `LogicalItsStationReference` | Referência lógica a uma ITS-S independente |

Os testes verificam as dez combinações entre estes cinco tipos, confirmando que são tipos distintos e não convertíveis implicitamente em qualquer direção.

Também verificam que `VruProfile` e `VruRoleSnapshot` não são `VamStationId` nem se convertem implicitamente nesse tipo.

O teste confirma ainda a ausência, diretamente em `VamSemanticData`, de campos com os nomes `gnAddress`, `linkMac`, `packetId`, `certificatePseudonym`, `sumoPersonId`, `vehicleId` e das referências de integração especificamente verificadas pelo teste.

Esta evidência demonstra separação estrutural de categorias. Não demonstra unicidade de valores, ausência de colisões semânticas em runtime ou correção de uma autoridade de alocação.

## 4. Testes e resultados

### 4.1 Modelo neutro

Runner:

`scratch/test/Red/VAM/test_vam_neutral_model_contract.py`

Fonte:

`scratch/test/Red/VAM/test_vam_neutral_model_contract.cc`

Resultado observado na execução final de 2026-11-06:

```text
VAM_NEUTRAL_MODEL_COMPILER=/usr/bin/g++
VAM_NEUTRAL_MODEL_CONTRACT_BUILD=PASS
test_neutral_model_contract ... ok

Ran 1 test
OK
VAM_NEUTRAL_MODEL_EXIT_CODE=0
```

O runner compila com C++17 e as opções `-Wall -Wextra -Wpedantic -Werror`, num diretório temporário, e executa o binário compilado.

### 4.2 Separação de identidade

Runner:

`scratch/test/Red/VAM/test_vam_identity_separation.py`

Fonte:

`scratch/test/Red/VAM/test_vam_identity_separation.cc`

Resultado observado na execução final de 2026-11-06:

```text
VAM_IDENTITY_SEPARATION_BUILD=PASS
test_identity_categories_are_separated ... ok

Ran 1 test
OK
VAM_IDENTITY_SEPARATION_EXIT_CODE=0
```

O runner utiliza as mesmas opções de compilação e estratégia de execução temporária.

### 4.3 Resultado agregado

```text
VAM_ETAPA_11_3_CONTRACT_VALIDATION=PASS
```

Os dois runners passaram na execução final. Este resultado cobre os contratos autónomos testados; não equivale à validação integral da Etapa 11.3 nem à validação de runtime.

## 5. Critérios ainda não demonstrados

### 5.1 Unicidade e não colisão de identidade

Não existe ainda uma `ITS Station Identity Authority` implementada.

A tipagem forte não impede que duas ITS-S recebam o mesmo valor numérico de `stationId`. Também não prova unicidade entre estações ativas nem ausência de colisões semânticas entre identificadores de origem.

Consequentemente, a unicidade e a não colisão em runtime permanecem **NOT VALIDATED**.

### 5.2 Pseudónimos

Não existe teste de rotação de pseudónimo, transição de credenciais ou invariantes de privacidade entre pseudónimos.

A implementação de um `Pseudonym Lifecycle Manager` fica adiada até existir decisão normativa e contrato de segurança aprovado.

### 5.3 Clusters

`VamClusterInformation`, `VamClusterOperation` e `VamMotionPrediction` são atualmente placeholders, sem campos funcionais validados nesta etapa.

Não foram implementadas nem testadas transições de identidade cluster/individual. Estas dependem da decisão normativa da Etapa 11.1 e da fase de clustering aplicável.

## 6. Limitações do modelo atual

O modelo semântico é uma base neutra, não uma representação VAM completa e normativamente congelada.

A seleção final de ASN.1/CDD, os campos, as unidades, os limites, os enums e as condições de presença continuam sujeitos ao gate da Etapa 11.1.

A representação cinemática atual é limitada a heading, velocidade e aceleração longitudinal opcionais. Outros atributos conceptuais, como posição na via, dinâmica angular, dimensões, iluminação e dados de ambiente, não estão implementados.

A viabilidade do fluxo SUMO Person → nó ns-3 próprio → ITS-S também não foi demonstrada por este conjunto de testes.

## 7. Matriz de decisão

| Critério | Estado |
|---|---|
| Modelo neutro sem dependência ASN.1 direta | VALIDATED |
| Contratos `VamInput` / `VamMessage` | VALIDATED |
| Duas alternativas em `VamOriginContext` | VALIDATED |
| Proveniência fora de `VamSemanticData` | VALIDATED |
| Separação estrutural dos cinco tipos de identidade | VALIDATED |
| Separação estrutural entre perfil/papel e `stationId` | VALIDATED |
| Unicidade de `stationId` entre ITS-S ativas | NOT VALIDATED |
| Não colisão de valores de identidade em runtime | NOT VALIDATED |
| Rotação de pseudónimo e privacidade | DEFERRED |
| Transições de identidade cluster/individual | DEFERRED |
| Integração funcional SUMO Person → ITS-S | NOT VALIDATED |
| Mapeamento ASN.1 normativo definitivo | BLOCKED BY 11.1 |

## 8. Dependências e próximos gates

1. Retomar a Etapa 11.1 e resolver a decisão normativa VAM/ASN.1/CDD e o contrato de segurança.
2. Definir a autoridade partilhada de identidade ITS, incluindo regras de unicidade e ciclo de vida, sem alterar prematuramente o caminho de identidade dos veículos.
3. Confirmar as regras normativas de `stationId` em mudanças de pseudónimo.
4. Desenvolver a gestão de clusters na fase prevista, incluindo as invariantes de identidade nas transições.
5. Validar separadamente a associação SUMO Person → nó ns-3 próprio → ITS-S.

Não iniciar a implementação de allocator, ciclo de vida de pseudónimos ou transições de identidade cluster/individual com base em pressupostos normativos ainda não confirmados.

## 9. Conclusão

O contrato neutro de domínio VAM e a separação estrutural de identidade estão validados por testes autónomos de compilação e execução.

A Etapa 11.3 permanece parcialmente concluída porque os critérios de unicidade e não colisão em runtime não foram demonstrados. Esta limitação é explícita e não deve ser convertida num PASS por inferência a partir dos `static_assert`.

**Estado final registado: PARTIAL / VALIDATED — NEUTRAL DOMAIN CONTRACT.**
