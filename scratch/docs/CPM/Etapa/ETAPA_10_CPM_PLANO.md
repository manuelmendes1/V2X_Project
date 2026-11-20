---
title: "Etapa 10 — CPM — Plano de Implementação e Validação"
status: "PROPOSED"
type: "FUNCTIONAL DEVELOPMENT AND VALIDATION PLAN"
date: 2026-10-19
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 10"
scope: "CPM — Collective Perception Message"
baseline: "POST-SPATEM-CLOSEOUT"
baseline_commit: "13788fb8ec770376e4cafd85f85c74c5977c142c"
previous_stage: "ETAPA 9 — SPATEM / CLOSED / VALIDATED"
next_stage: "ETAPA 10.0 — baseline freeze, pending plan approval"
related_plan:
  - "[[ETAPA_8_DENM_PLANO]]"
  - "[[ETAPA_9_SPATEM_PLANO]]"
related_state:
  - "[[CURRENT_STATE]]"
  - "[[DOCUMENT_STATUS]]"
---

# Etapa 10 — CPM — Plano de Implementação e Validação

## 1. Objetivo

A Etapa 10 tem como objetivo introduzir suporte funcional a **CPM — Collective Perception Message** na integração existente entre ns-3.47, Vanetza e SUMO.

O primeiro marco deverá demonstrar um fluxo CPM verificável, da construção de uma representação de perceção até à entrega dos objetos percebidos no recetor:

```text
perception source / deterministic object model
    -> CPM generation management
    -> neutral CPM model
    -> ASN.1 / UPER encode
    -> security
    -> BTP / GeoNetworking
    -> ns-3 access / channel
    -> GeoNetworking RX
    -> BTP RX
    -> security verification
    -> UPER decode
    -> semantic validation
    -> replay handling
    -> reception
    -> delivery of CPM / perceived objects
```

A implementação deverá seguir a arquitetura consolidada para CAM, DENM e SPATEM. CPM não deverá originar uma arquitetura paralela de transporte, identidade, `PacketContext`, segurança, logging, receção ou análise.

O primeiro baseline será deliberadamente limitado: um emissor, um recetor, uma fonte de perceção determinística e um conjunto pequeno de objetos conhecido pelo teste. O objetivo inicial é demonstrar a semântica da mensagem e o percurso de rede; não é implementar simultaneamente fusão de sensores, tracking avançado ou certificação de conformidade.

## 2. Princípios de implementação

Mantêm-se os princípios das Etapas 8 e 9:

```text
código plausível != comportamento demonstrado
build limpo != funcionalidade validada
run local != interoperabilidade externa
```

Cada unidade de trabalho deverá seguir a sequência:

```text
requisito / problema
    -> evidência do estado atual
    -> teste RED focado
    -> alteração mínima
    -> teste GREEN
    -> acceptance observável
    -> commit separado
```

Nenhum gate poderá ser marcado `PASS` apenas porque existe uma classe, um wrapper, um codec ou uma chamada de API. A aceitação exige evidência apropriada de fonte, build e runtime.

## 3. Pré-condição de sequência e estado de partida

### 3.1 Gate obrigatório de entrada

A Etapa 9 foi formalmente encerrada como `CLOSED / VALIDATED` e integrada em `main` pela PR #2. O commit de merge confirmado no GitHub é:

```text
13788fb8ec770376e4cafd85f85c74c5977c142c
```

A branch de trabalho CPM foi criada a partir deste baseline, com o working tree limpo no momento da criação. Portanto, o gate de encerramento SPATEM está satisfeito; a condição antiga que mantinha a Etapa 9 em curso foi substituída por esta evidência.

Isto não dispensa a Fase 10.0. Antes do primeiro commit de implementação CPM, a branch deverá voltar a registar e preservar a evidência do baseline efetivamente executado:

```text
Etapa 9 closeout = CLOSED / VALIDATED (PR #2 / merge 13788fb8)
HEAD pós-SPATEM registado
working tree inicial = CLEAN
build de produção = PASS
suite Green + Red = PASS
CAM baseline / enable flag = PASS
DENM baseline = PASS
SPATEM regressions = PASS
v2x_analyzer baseline = PASS
```

Os resultados de build e testes registados nos artefactos do closeout são evidência do estado de código incluído no merge. A Fase 10.0 deverá confirmar a reprodução no checkout local e registar os comandos, códigos de saída e logs próprios antes de se iniciar implementação funcional CPM.

O freeze não deve reutilizar `420db8a`: esse commit pertence ao baseline inicial da Etapa 9 e precede o closeout SPATEM.

### 3.2 Baseline e fonte de verdade

A fonte de verdade é o checkout Git atual do projeto, não snapshots históricos nem documentação antiga isolada. Registar pelo menos:

```bash
git rev-parse HEAD
git status --short --branch
git log -1 --oneline
cmake --build cmake-cache --clean-first --target ns3.47-v2x_sumo-default -j2
python3 scratch/test/run_tests.py --suite all
python3 -m pytest scratch/v2x_analyzer/tests
```

Os cenários CAM, DENM e SPATEM já aceites deverão ser identificados pelos respetivos comandos de teste atuais e preservados como regressão de referência.

### 3.3 Limite de build

A configuração de produção de `scratch/CMakeLists.txt` deverá manter a restrição deliberada ao executável de produção `v2x_sumo`. Não deverão ser adicionados executáveis CPM de diagnóstico ao build normal de produção apenas para facilitar os testes. Os testes CPM deverão seguir os mecanismos de teste existentes e ficar organizados na estrutura de testes do projeto.

## 4. Âmbito funcional

### 4.1 Incluído no primeiro baseline

```text
CPM PDU and chosen ASN.1 release
neutral application data model
station / object identity separation
reference position and coordinate conversion
reference time and measurement timing
perceived-object container handling
applicable station-data container
sensor-information handling where required by the selected profile
CPM inclusion / generation policy with bounded configuration
UPER encode / decode
BTP and GeoNetworking integration
TX and RX in runtime
security and replay through shared infrastructure
semantic validation
PacketContext / global TX-RX taxonomy
logging and v2x_analyzer
synthetic end-to-end scenario
controlled SUMO perception-source integration
Red / Green tests and regression
```

Os containers, campos obrigatórios, condições de presença e limites exatos deverão resultar da edição ASN.1 selecionada na fase 10.1. Esta lista é conceptual e não substitui a especificação normativa.

### 4.2 Fora do primeiro baseline

```text
real camera / radar / lidar hardware
real sensor fusion and production-grade object tracking
advanced cross-station data association or fusion
MAPEM generation or map matching as a prerequisite
vehicle control based on received CPM
real ITS-G5 hardware
OTA interoperability certification
formal ETSI conformance certification
multi-channel operation / MCO unless required by the audited baseline
full support for every optional container and extension in every release
```

A exclusão de um recurso opcional deverá ser documentada e não poderá justificar a emissão de mensagens inválidas segundo o perfil de CPM declarado.

## 5. Semântica CPM específica do projeto

CPM não é simplesmente um CAM com vários veículos no payload. A unidade de transmissão continua a ser uma mensagem CPM; a mensagem pode descrever um conjunto de objetos percebidos pela estação emissora.

O projeto deverá separar explicitamente:

```text
stationId          -> identidade ITS da estação de origem
GN_ADDR            -> identidade de rede GeoNetworking
Link MAC           -> identidade de camada de acesso
packetId           -> identidade física / rastreio do pacote
objectId           -> identidade do objeto percebido no contexto CPM
TraCI vehicle ID   -> identificador do veículo no SUMO
```

Estes identificadores não são intercambiáveis. Em particular, `objectId` não pode ser usado como `packetId`, nem o identificador TraCI pode ser copiado diretamente para o campo CPM sem uma política de atribuição e validade compatível com a especificação escolhida.

O recetor deverá ainda distinguir:

```text
número de objetos conhecidos pela fonte / LDM
!= necessariamente
número de objetos incluídos nesta mensagem CPM
```

O analyzer deverá contar separadamente mensagens, pacotes físicos e objetos. Um CPM com dez objetos representa uma mensagem recebida, não dez pacotes recebidos.

## 6. Sequência oficial proposta

```text
10.0  Gate de entrada e freeze pós-SPATEM
  |
10.1  Auditar API Vanetza, versão CPM e compatibilidade ASN.1 / CDD
  |
10.2  Definir estação de origem, identidades e perfil normativo
  |
10.3  Definir modelo de dados CPM neutro
  |
10.4  Definir source de perceção, referência espacial e temporal
  |
10.5  Definir regras de geração e inclusão de objetos
  |
10.6  Implementar / integrar codec ASN.1 / UPER
  |
10.7  Integrar BTP / GeoNetworking
  |
10.8  Implementar CPM TX
  |
10.9  Implementar CPM RX e validação semântica
  |
10.10 Integrar security / replay
  |
10.11 Alinhar PacketContext, logging e identidade física
  |
10.12 Estender v2x_analyzer
  |
10.13 Validar CPM sintético E2E
  |
10.14 Integrar fonte de perceção SUMO
  |
10.15 Validar cardinalidade, tamanho, segmentação e robustez
  |
10.16 Regressão CAM + DENM + SPATEM
  |
10.17 Closeout
```

A ordem só poderá ser alterada perante uma dependência técnica demonstrada. Nenhum código dependente da versão ASN.1, do tipo de estação ou do transporte poderá anteceder os respetivos gates.

## 7. Fase 10.0 — Gate de entrada e freeze pós-SPATEM

### Objetivo

Estabelecer um ponto de partida reproduzível e impedir que as alterações CPM confundam evidência ou regressões da Etapa 9.

### Ações

1. Referenciar o closeout SPATEM já concluído pela PR #2 e confirmar que o checkout parte do merge commit `13788fb8ec770376e4cafd85f85c74c5977c142c`.
2. Registar HEAD, branch, worktree e o estado das ferramentas e dependências relevantes.
3. Reexecutar o build de produção, a suite Green + Red, os testes do `v2x_analyzer` e os testes de regressão CAM/DENM/SPATEM identificados no checkout atual.
4. Guardar comandos, códigos de saída, contagens e logs antes de qualquer alteração funcional CPM.

### Critério de saída

A pré-condição de closeout SPATEM está satisfeita. A Fase 10.0 só passa quando a reprodução do baseline local estiver registada, o working tree inicial estiver limpo antes de criar este documento e não existir qualquer regressão nos gates anteriores. Até lá, não iniciar implementação funcional CPM.

## 8. Fase 10.1 — Auditoria da API, ASN.1, CDD e versão CPM

Esta fase é obrigatória antes de criar estruturas ou código dependentes da representação ASN.1.

### Objetivo

Identificar o suporte CPM **realmente instalado e ligado** no Vanetza e escolher uma combinação coerente de norma, módulos ASN.1, CDD, codecs e APIs.

### Auditoria local

Pesquisar nos headers, fontes e bibliotecas efetivamente usados pela build:

```text
CPM / CollectivePerceptionMessage
CPM-PDU-Descriptions
CpmManagementContainer / ManagementContainer
StationDataContainer / OriginatingVehicleContainer / OriginatingRSUContainer
SensorInformationContainer
PerceptionRegionContainer
PerceivedObjectContainer / PerceivedObject
ITS PDU Header / message ID / protocol version
ASN.1 generated types
UPER encoder / decoder
CDD imports and version
Vanetza Release 1 / Release 2 namespaces or aliases
BTP facilities API / port mapping
ITS-AID and security integration
GeoNetworking transport request API
```

Não assumir nomes de classes, namespaces ou ficheiros antes da inspeção local. A auditoria deverá identificar também de que prefixo de instalação e bibliotecas provém a API encontrada, para evitar comparar headers de uma versão com bibliotecas de outra.

### Decisão sobre a edição normativa

A referência inicial proposta é **ETSI TS 103 324 V2.1.1 (2023-06), Release 2**, uma edição publicada. A auditoria deverá confirmar a compatibilidade da implementação Vanetza instalada com essa edição e com a sua dependência CDD.

**Nota de atualidade normativa (verificação em 2026-10-19):** o Work Programme da ETSI para o item `RTS/ITS-001985` regista trabalho de revisão da TS 103 324; a página reportava a versão `0.0.24 Draft` como *Stable draft* em 2026-03-17 e uma proposta de stable draft anotada em 2026-05-05. Esta informação não transforma um draft numa edição normativa publicada. Na Fase 10.1, confirmar a publicação ETSI vigente e decidir expressamente se o baseline usa a edição publicada V2.1.1 ou outra edição formalmente publicada. Não implementar contra um draft de trabalho sem decisão explícita e sem identificar a versão exata dos módulos ASN.1 / CDD.

Existe histórico no upstream Vanetza sobre definições CPM antigas baseadas em ETSI TR 103 562 e sobre a introdução de CPM 2.2.1 / alinhamento do CDD. Isto não determina o que está instalado nesta máquina. Não misturar módulos CPM, CAM, DENM e CDD de releases incompatíveis, nem assumir que uma API observada no ramo `master` existe nas bibliotecas locais.

Se a instalação oferecer um perfil CPM diferente da referência proposta, interromper o avanço e registar uma decisão técnica explícita entre:

```text
A) usar a versão normativa inicialmente escolhida e suportada pela instalação;
B) adaptar/atualizar a integração para a versão escolhida;
C) estabelecer um perfil de compatibilidade isolado e demonstravelmente coerente.
```

A opção escolhida deverá fixar versões, fontes, origem dos módulos ASN.1 e estratégia de build. Não realizar upgrades globais do CDD ou alterações CAM/DENM como efeito lateral sem análise de impacto e testes específicos.

### Matriz de auditoria obrigatória

| Elemento | Evidência local necessária | Estado inicial |
|---|---|---|
| Edição CPM normativa | versão e documento ETSI selecionados | TBD |
| CPM PDU ASN.1 | header/fonte gerada e namespace | TBD |
| CDD / ITS containers | versão, imports e símbolos | TBD |
| UPER encoder | símbolo e API real | TBD |
| UPER decoder | símbolo e API real | TBD |
| CPM model / converter | API existente ou decisão de implementação | TBD |
| message ID / protocol version | definição da versão selecionada | TBD |
| BTP port / transport profile | fonte local e justificação | TBD |
| ITS-AID / security | configuração e implementação efetiva | TBD |
| CMake / link dependencies | targets, libs e includes reais | TBD |

### Critério de saída

A combinação CPM + CDD + codec deverá estar identificada, ser compilável num teste mínimo e estar aprovada como referência para as fases seguintes. Se não for possível estabelecer uma combinação coerente, parar nesta fase e documentar a decisão necessária.

## 9. Fase 10.2 — Estação de origem, identidades e perfil de mensagem

### Objetivo

Definir quem origina o CPM e como os papéis e identidades se relacionam com o modelo V2X existente.

### Decisões obrigatórias

- O primeiro cenário usará uma estação emissora de tipo veículo (Vehicle ITS-S), salvo incompatibilidade demonstrada pela implementação instalada.
- O emissor descreve objetos que percebe à sua volta; não se deverá incluir automaticamente o próprio veículo como um objeto remoto percebido sem uma regra semântica explícita.
- A escolha do station-data container deve corresponder ao tipo real da estação e à edição CPM auditada.
- `stationId`, `GN_ADDR`, MAC, `packetId`, `objectId` e ID TraCI continuam com significados separados.
- A política para criação, manutenção, reutilização e expiração de `objectId` deve respeitar a versão normativa escolhida. Os testes podem usar uma fonte de IDs controlada para reprodutibilidade, sem transformar um índice TraCI numa identidade protocolar por conveniência.

O eventual suporte a uma estação RSU deverá ser uma extensão posterior ou um caso adicional explicitamente aceite; não deve complicar o primeiro cenário sem necessidade.

### Critério de saída

Documento de decisão contendo perfil normativo, tipo de estação do caso de uso inicial, semântica de cada identificador e regras de associação entre objetos ao longo de mensagens.

## 10. Fase 10.3 — Modelo de dados neutro CPM

### Objetivo

Manter as aplicações e a fonte de perceção independentes das estruturas ASN.1 específicas do Vanetza.

### Modelo conceptual

Os nomes concretos devem seguir as convenções do repositório após auditoria. A representação lógica deverá incluir, conforme aplicável à edição selecionada:

```text
CpmInput / CpmMessage
├── originating station
│   ├── station identity
│   ├── station type
│   └── station-data fields applicable to that type
├── reference time
├── reference position
├── sensor information (when supplied / applicable)
├── perception regions (when supplied / applicable)
├── known-object count
└── included perceived objects
    ├── objectId
    ├── measurement time / measurement delta
    ├── object age, when available
    ├── relative position and confidence
    ├── motion / attitude and confidence, when available
    ├── dimensions, when available
    ├── classification / perception quality, when available
    └── sensor references, when available
```

Os campos, limites, unidades, enumerações e condições obrigatórias serão derivados do ASN.1 normativo auditado. Não introduzir campos apenas porque existem na versão antiga ou numa versão diferente do CDD.

### Fronteira de arquitetura

```text
SUMO / synthetic perception source
    -> neutral CPM domain model
    -> CPM generation service
    -> Vanetza CPM builder / converter / codec
    -> encoded payload
```

### Critério de saída

Tipos de domínio testáveis sem incluir headers ASN.1 no código de perceção e sem expor tipos gerados a componentes da aplicação que não necessitem deles.

## 11. Fase 10.4 — Fonte de perceção, coordenadas e tempo

### Objetivo

Definir uma fonte de objetos reproduzível, com semântica espacial e temporal explicitamente verificável.

### Fonte inicial

Antes da integração TraCI, usar uma fonte sintética de perceção com objetos conhecidos, coordenadas, timestamp/idade e propriedades controladas. Isto permite verificar a codificação CPM sem depender de sincronização ou georreferenciação do SUMO.

### Integração de coordenadas

Deverão ser documentadas e testadas as transformações entre:

```text
SUMO / TraCI coordinates
ns-3 local coordinates
WGS84 reference position
CPM local object coordinate frame (ENU)
```

Estes sistemas não são intercambiáveis. A posição de referência da estação e as posições relativas dos objetos devem ser convertidas com eixos, origem, sinais, unidades, orientação e quantização definidos. Para a edição de referência, a especificação CPM define a representação dos objetos num sistema ENU centrado na referência da estação emissora; a escolha do ponto de referência e a sua transformação deverão obedecer à cláusula aplicável ao tipo de estação.

### Semântica temporal

Separar e documentar:

```text
SUMO simulation time
ns-3 simulation / scheduler time
CPM reference time
object measurement time / delta
object age
```

A mensagem não pode alegar que uma medição é atual se os dados estiverem atrasados. O delta temporal deve corresponder ao instante de referência da medição/estado, incluindo o processamento definido para a fonte. Não inserir conversões temporais implícitas no encoder.

### Modelo de perceção sintético

O adaptador CPM deverá representar uma perceção configurável, não conhecimento omnisciente de todos os veículos SUMO. Definir pelo menos alcance e critérios de inclusão; o campo de visão pode ser adicionado no modelo inicial se necessário. O cenário idealizado sem ruído deverá ser identificado como tal e não descrito como uma simulação de sensor físico.

### Testes obrigatórios

- transformar posições conhecidas e verificar os valores relativos esperados;
- confirmar sinal/eixos para objetos à frente, atrás, à esquerda e à direita;
- confirmar limites e quantização do esquema;
- detetar coordenadas não finitas ou fora dos limites admitidos;
- verificar referência temporal, medição e idade com instantes conhecidos;
- comprovar que não são misturados relógios sem transformação explícita.

### Critério de saída

Transformações espaciais e temporais documentadas e testadas independentemente do codec.

## 12. Fase 10.5 — Regras CPM de geração e inclusão de objetos

### Objetivo

Implementar um primeiro esquema determinístico de generation management, separado da serialização e coerente com o perfil CPM escolhido.

CPM não deve ser implementado como emissão incondicional de todos os objetos a uma frequência arbitrária. A especificação selecionada define eventos periódicos, limites para o intervalo de geração e regras de inclusão de objetos, bem como gestão do tamanho/conteúdo da mensagem. Os valores concretos e as regras normativas deverão ser reproduzidos a partir da edição auditada.

### Baseline mínimo

- intervalo de geração controlado por configuração e limitado pelos valores do perfil normativo;
- conjunto de objetos disponíveis à fonte de perceção em cada instante;
- seleção determinística dos objetos a incluir;
- manutenção de estado mínimo para saber quais os objetos anteriormente transmitidos e quando foram incluídos;
- contador de objetos conhecidos separado do conjunto efetivamente incluído;
- limite configurado de cardinalidade / tamanho de payload;
- regra de atualização de dados quando o objeto permanece visível;
- política explícita de remoção/expiração quando um objeto deixa de ser percebido;
- registo do motivo de geração ou de não geração, para diagnóstico.

A seleção inicial poderá ser um subconjunto da política normativa, desde que o âmbito e as limitações fiquem documentados e não seja apresentada como implementação integral do serviço CPM. Os parâmetros de referência não devem ser copiados para constantes no código sem justificação.

### Critério de saída

Testes determinísticos para deteção de novo objeto, mudança de estado, atualização, intervalo máximo sem nova inclusão, ausência de objetos elegíveis e seleção sob limite de tamanho. O serviço de geração deve funcionar sem depender do codec ou da camada de rede.

## 13. Fase 10.6 — Codec ASN.1 / UPER

### Objetivo

Construir ou integrar o codec real da edição CPM aprovada, usando a fronteira Vanetza.

Local conceptual:

```text
scratch/vanetza_integration/wrappers/vanetza/cpm/
```

Os ficheiros finais deverão seguir as convenções existentes após a auditoria; não criar um segundo runtime ou um adapter de transporte só para CPM.

### Testes mínimos

```text
CpmInput
    -> build CPM ASN.1 object
    -> UPER encode
    -> bytes
    -> UPER decode
    -> CpmMessage
    -> compare semantic fields
```

O round-trip deve preservar os campos essenciais do perfil mínimo, incluindo identidade de origem, instante/referência, coordenadas da estação, número de objetos conhecidos, `objectId`, tempo de medição, posições relativas e os estados opcionais efetivamente fornecidos.

Além do round-trip, adicionar fixtures de bytes válidos conhecidos para a edição escolhida. Sempre que possível, incluir testes de ida e volta com o mesmo conjunto de ASN.1 compilado que está ligado à build do projeto.

### Red tests

- payload truncado ou malformado;
- conteúdo ASN.1 inválido;
- `messageId` ou versão incompatível;
- enumeração / valor fora do domínio permitido;
- lista de objetos ou containers com cardinalidade inválida;
- campos obrigatórios ausentes;
- combinação de containers inválida para o perfil escolhido;
- coordenadas e tempos fora dos limites ASN.1;
- campos opcionais interpretados segundo uma release errada.

### Critério de saída

Round-trip e fixtures da edição CPM escolhida passam; o decoder não aceita silenciosamente um payload inválido nem reporta sucesso parcial como mensagem válida.

## 14. Fase 10.7 — BTP / GeoNetworking

### Objetivo

Integrar CPM no caminho de transporte comum, sem copiar o adaptador CAM e substituir apenas a porta.

### Requisitos

- reutilizar `BtpTxRequest`, wrappers GeoNetworking, access adapter e caminho DCC já existentes quando aplicável;
- selecionar o mecanismo de transporte adequado ao caso de uso com base na API efetiva e na especificação/perfil usado;
- identificar em fonte local a porta BTP, ITS-AID, message ID, tipo de transporte, lifetime, hop limit, destination/area quando aplicáveis;
- preservar a separação entre payload CPM e parâmetros de encaminhamento;
- não fixar valores no código antes da auditoria 10.1;
- verificar que o dispatch RX distingue CPM de CAM, DENM e SPATEM sem alterar handlers válidos.

### Critério de saída

Teste de integração que demonstre a composição efetiva do pedido de transmissão e o dispatch correspondente, com parâmetros e payload compatíveis com a configuração aprovada.

## 15. Fase 10.8 — CPM TX

### Fluxo esperado

```text
CPM generation event
    -> select eligible objects
    -> GENERATE
    -> ENCODE
    -> SIGN
    -> BTP_TX
    -> GEONET_TX
    -> ACCESS_TX
    -> SEND
    -> TX_COMPLETE
```

Os nomes de evento específicos são auxiliares; a taxonomia global continua a ser a referência.

### Cenário inicial

Uma estação Vehicle A recebe da fonte sintética um conjunto pequeno e conhecido de objetos percebidos. O teste deverá confirmar o conteúdo semântico antes de introduzir SUMO ou tráfego multi-veículo.

### Evidência mínima

```text
CPM generation event observable
selected-object count
encoded payload length
BTP / GeoNetworking request
ACCESS_TX / SEND
TX_COMPLETE RESULT=OK
semantic decoded view of the generated payload
```

### Critério de saída

Uma mensagem CPM validada e transmitida pelo caminho de produção, com origem, referência temporal/espacial e objetos coerentes com o input.

## 16. Fase 10.9 — CPM RX e validação semântica

### Pipeline obrigatório

```text
RAW_RX
    -> ACCESS_RX
    -> GEONET_RX
    -> BTP_RX
    -> VERIFY
    -> DECODE
    -> REPLAY
    -> RECEPTION
    -> DELIVER
```

### Requisitos RX

- dispatch explícito para CPM sem quebrar CAM, DENM ou SPATEM;
- decoder da versão selecionada;
- validação de header e tipo de mensagem;
- validação de containers e limites definidos pela edição auditada;
- verificação da coerência entre estação, posição de referência e objetos;
- validação de unidades, sinais, campos obrigatórios, tempos e cardinalidades;
- preservação do tipo de erro: transporte, security, decode, semântica, replay ou delivery;
- entrega de um `CpmMessage` neutro à aplicação superior, em vez de expor objetos ASN.1 ao consumidor.

Não assumir que toda mensagem CPM tem de conter sempre a mesma combinação de containers: a presença de containers e de listas deve respeitar as regras condicionais do perfil selecionado.

### Contrato de entrega

No baseline, a entrega deverá expor o conjunto de objetos efetivamente transportados e os metadados necessários para interpretar as medições. A implementação de fusão de vários emissores, associação probabilística de tracks ou controlo do veículo é fora do primeiro baseline.

### Critério de saída

Um CPM transmitido pelo teste chega à aplicação do recetor, é descodificado, validado e entregue com o mesmo conteúdo semântico. Mensagens rejeitadas não devem gerar `DELIVER_OK`.

## 17. Fase 10.10 — Security e replay

### Objetivo

Reutilizar a infraestrutura de segurança consolidada e demonstrar os seus resultados para CPM.

Separar evidências de:

```text
signature generation
signature verification
replay acceptance
replay rejection
application delivery
```

### Requisitos

- usar a configuração de credenciais/security efetivamente disponível;
- não adicionar uma implementação criptográfica CPM específica sem necessidade demonstrada;
- validar emissor/credencial segundo as regras já existentes;
- demonstrar uma receção legítima e uma mensagem rejeitada por falha de verificação, quando o harness de teste o permitir;
- retransmitir ou reinjetar um pacote assinado duplicado para comprovar a decisão replay da infraestrutura;
- manter separadas identidade física do pacote, identidade da estação e identidade dos objetos incluídos.

A política de replay deve ser coerente com o envelope de segurança e com o mecanismo comum do projeto. `objectId` isolado não é uma chave de replay válida para a mensagem inteira.

### Critério de saída

A aceitação, rejeição e entrega são visíveis em runtime e a mensagem rejeitada não chega ao consumidor CPM. Se alguma parte da segurança estiver explicitamente limitada por restrições do ambiente, a limitação deve ser evidenciada e o gate não deve ser apresentado como `PASS` completo.

## 18. Fase 10.11 — PacketContext e logging

### Objetivo

Preservar a observabilidade comum do percurso TX/RX.

Taxonomia TX:

```text
GENERATE -> ENCODE -> SIGN -> BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE
```

Taxonomia RX:

```text
RAW_RX -> ACCESS_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE -> REPLAY -> RECEPTION -> DELIVER
```

Podem ser adicionados eventos auxiliares, por exemplo `CPM_OBJECT_SELECTED`, `CPM_DECODE_OK`, `CPM_INVALID_OBJECT`, `CPM_INVALID_TIMING` ou `CPM_SEGMENT_REJECTED`, desde que não criem uma segunda taxonomia concorrente.

### Campos úteis

Quando disponíveis e compatíveis com o esquema global, os logs deverão tornar possível relacionar:

```text
packetId
stationId / source identity
CPM message type and profile
reference time
known-object count
included-object count
object identifiers (as diagnostic fields, not packet IDs)
encoded byte count
BTP / GN metadata
security / replay result
RX delivery result
```

### Regra fundamental

`PacketContext` continua a ser a fonte canónica de contexto físico por pacote. Não criar um `PacketContext` por objeto, nem somar objetos como pacotes. Se ocorrer segmentação física/lógica, documentar a relação entre segmentos e dados CPM conforme a versão normativa escolhida.

### Critério de saída

É possível seguir um único pacote desde TX até RX pelo `packetId`/contexto físico e verificar separadamente os objetos associados ao CPM. A taxonomia global e os testes existentes permanecem válidos.

### Evidência de validação local - 2026-10-27

**Resultado:** `PASS` para a validação local da Fase 10.11.

#### Build e testes

- Build de produção: `PASS`, código 0.
- Logging CPM: 6 testes aprovados.
- Serviço e gateway CPM: `PASS`.
- Pipeline RX: 36 verificações, zero falhas.
- Segurança: 11 testes aprovados.
- `git diff --check`: `PASS`.

#### Runtime SUMO/ns-3

- Geração CPM: 150 sucessos, zero falhas.
- Codificação CPM: 150 sucessos, zero falhas.
- Início TX: 150, zero falhas.
- Decode RX: 92 sucessos.
- Replay RX: 92 sucessos.
- Reception RX: 92 sucessos.
- Delivery RX: 92 sucessos.

#### Auditoria

- Metadados TX: 0 erros em 450 registos.
- Metadados RX: 0 erros em 736 registos.
- Verificação criptográfica: 0 erros em 368 registos.
- Correlação semântica de `PKT=1`: `PASS`.

Os metadados auditados apresentam porta BTP 2009,
ITS-AID 639, protocolo BTP-B e transporte SHB.

Para `PKT=1`, TX, decode e delivery preservam
o mesmo reference time, contagem e object IDs.

A entrega observada foi de 92/150 pacotes (61,3%).
A causa dos restantes 58 pacotes não foi determinada.

#### Eventos de diagnóstico

Os eventos auxiliares *ad hoc* complementam a
taxonomia TX/RX. Não a substituem nem alteram a
regra de um `PacketContext` por pacote físico.

#### Evidência preservada

Diretoria:
`test-results/cpm_etapa10_11_runtime_verify_4kVYT9/`

Contém os logs ns-3 e SUMO, o resumo de validação
e o manifesto `SHA256SUMS`.

#### Limites

Esta evidência demonstra integração local no
cenário executado. Não prova interoperabilidade
OTA nem conformidade ETSI.

A análise CPM no `v2x_analyzer` pertence à
Fase 10.12. As restantes fases continuam pendentes.
O estado global permanece `PROPOSED`.


## 19. Fase 10.12 — `v2x_analyzer`

### Objetivo

Adicionar análise CPM ao analyzer atual sem duplicar o parser ou a taxonomia global.

### Métricas mínimas

```text
CPM messages generated
CPM messages encoded / TX complete
CPM physical packets received
CPM decode failures
CPM semantic validation failures
CPM security failures
CPM replay drops
CPM messages delivered
known-object count (reported by source)
included-object count (in payload)
object count delivered
encoded size distribution / maximum observed size
generation interval / inter-message timing
```

As métricas de mensagem, pacote e objeto deverão ser distintas. A quantidade de objetos incluídos pode diferir da quantidade de objetos conhecidos pela fonte, conforme a regra de inclusão e o perfil CPM selecionado.

### Testes

- manter os testes atuais do analyzer sem alterações semânticas não justificadas;
- adicionar fixtures com CAM, DENM, SPATEM e CPM combinados;
- testar falta de campos, ordem de eventos incompleta, valores duplicados e payloads malformados;
- comprovar que uma mensagem com vários objetos aumenta a métrica de objetos, mas não a contagem de pacotes físicos;
- testar métricas de rejeição e delivery sem confundir as causas.

### Critério de saída

Suite atual + novos testes CPM passa; o relatório distingue as etapas do pipeline e as contagens CPM específicas.

### Evidência de validação local — 2026-10-27

Resultado: PASS na validação funcional local.

- Testes disponíveis: 11 aprovados.
- CLI do v2x_analyzer: código de saída 0.
- Critérios P3: 5/5 PASS.
- TX: 150 gerações, codificações e transmissões completas.
- RX: 92 receções brutas, descodificações e entregas.
- Falhas TX/RX contabilizadas: zero.
- Duplicados: zero.
- `git diff --check`: PASS.

Limitação: a suite histórica de 26 testes referida no
README não existe na checkout inspecionada. Esta evidência
cobre os 11 testes atuais e o runtime validado.

Esta validação não demonstra conformidade ETSI nem
interoperabilidade OTA. O estado global do plano mantém-se
PROPOSED; as fases seguintes continuam pendentes.

## 20. Fase 10.13 — CPM sintético E2E

### Objetivo

Demonstrar o caminho CPM completo em runtime, sem inicializar SUMO ou depender de callbacks SUMO. O cenário utiliza observações sintéticas determinísticas e o caminho real de transporte e receção de Vanetza através do canal ns-3.

### Cenário executado

A estação RSU emissora utiliza stationId 9001. O envio percorre CpmSyntheticPerceptionSource, CpmRsuRuntime, codificação CPM R2 UPER, BTP-B, GeoNetworking SHB, segurança V3, acesso Wi-Fi ns-3 e o runtime Vanetza do recetor, com stationId 1.

O harness não inicializa o SumoAdapter, não utiliza callbacks SUMO para desencadear a transmissão e não injeta mensagens diretamente na fila de receção. O envio é agendado pelo tempo de simulação ns-3.

O cenário contém dois objetos conhecidos. O objeto 501, na posição local ENU X=12,5 m e Y=-3,25 m, é incluído por estar dentro do alcance de 50 m. O objeto 502 está fora do alcance e é excluído da lista transmitida. Ambos contam para knownObjectCount=2.

A posição geográfica da RSU é fornecida explicitamente como latitude 0 e longitude 0, sem ser inferida das coordenadas ENU. A altura dos objetos fica indisponível, respeitando o perfil horizontal implementado pelo encoder.

### Evidência TX

A mensagem CPM foi codificada em 388 bits, ocupando 49 bytes UPER. Utilizou BTP port 2009 e ITS-AID 639. A trama serializada recebida no limite RX tinha 314 bytes.

O contexto TX utilizou packet_id=1. O evento CPM_ENCODE_OK registou um fingerprint FNV-1a de 64 bits calculado sobre os bytes UPER antes de o PacketContext poder conter a trama serializada.

### Evidência RX e conteúdo semântico

O evento CPM_RX_DISPATCH registou packet_id=1. O evento CPM_BTP_RX_OK calculou o fingerprint sobre os bytes CPM recebidos em BTP.

Na execução aprovada, os fingerprints TX e RX coincidiram: f124f6dd4a05c05e. Este valor documenta aquela execução, não constitui uma constante do cenário.

A verificação de segurança registou cryptographic=PASS, AID 639 e context=BOUND.

O harness confirmou na entrega:
- stationId de origem 9001 e origem RSU;
- referência temporal consistente entre TX e RX;
- knownObjectCount=2 e um objeto incluído;
- objectId=501;
- measurementDeltaTimeMs=-50;
- objectAgeMs=1050;
- posição horizontal X=12,5 m e Y=-3,25 m.

O runtime emitiu CPM_SYNTHETIC_E2E_SEMANTICS=PASS, CPM_SYNTHETIC_E2E_SECURITY=PASS e CPM_SYNTHETIC_E2E=PASS.

### Testes e build

Resultados observados em 2026-10-27:
- Teste test_signed_synthetic_cpm_crosses_ns3_and_is_delivered: PASS.
- Runtime E2E: CPM_SYNTHETIC_E2E_RUNTIME_RC=0.
- Regressão CPM Green + Red: 54 testes e 159 subtestes aprovados.
- Regressão do analyzer: 11 testes aprovados.
- Target ns3.47-v2x_sumo-default: build concluído; repetição posterior indicou ninja: no work to do.
- git diff --check: PASS.

O teste está em scratch/test/Red/CPM/test_cpm_synthetic_e2e.py e utiliza o harness C++ scratch/test/Red/CPM/test_cpm_synthetic_e2e.cc. O runner utiliza temporariamente os comandos compile/link extraídos do grafo Ninja, sem acrescentar um target de produção ao scratch/CMakeLists.txt.

### Limites e interpretação

- A igualdade do fingerprint FNV-1a de 64 bits não é uma comparação byte a byte independente. O fingerprint coincidente, a correlação packet_id e a entrega através do canal constituem a evidência usada nesta fase.
- A verificação criptográfica foi observada no recetor real. Não se afirma interoperabilidade OTA nem conformidade ETSI completa.
- Esta fase demonstra a transmissão sintética através da stack real sem dependência do SUMO. Não demonstra que o caminho de perceção de produção funcione independentemente do callback SUMO.
- O ambiente virtual ~/final/ns3-venv aponta para /usr/bin/python3.12, que não está disponível. As regressões Python foram executadas com /usr/bin/python3 3.14.4, o pytest existente, PYTHONPATH explícito e PYTEST_DISABLE_PLUGIN_AUTOLOAD=1. A reparação do ambiente virtual fica como tarefa de reprodutibilidade separada.

### Critério de saída

PASS — Fase 10.13 validada em runtime. O caminho sintético atravessou ns-3 e Vanetza, passou pela verificação criptográfica e entregou o conteúdo semântico esperado. A correlação packet_id e os fingerprints UPER TX/RX coincidiram na execução aprovada.

Este resultado fecha a evidência da Fase 10.13, mas não fecha a Etapa 10 global nem altera o estado PROPOSED do plano geral.

## 21. Fase 10.14 — Integração SUMO como fonte de perceção

A ligação a SUMO só deverá começar depois de o E2E sintético estar validado.

### Fluxo

```text
SUMO / TraCI world state
    -> transform coordinates and simulation time
    -> configured perception model / field of view
    -> visible object set for the originating station
    -> neutral CPM model
    -> generation management
    -> Vanetza / ns-3 TX
```

### Limites do adaptador inicial

- escolher uma estação emissora e uma lista controlada de veículos-alvo;
- excluir o emissor da lista de objetos remotos salvo requisito explícito e justificado;
- usar um alcance configurável; campo de visão configurável se fizer parte do caso de teste;
- definir quais os atributos extraídos de SUMO e quais são aproximações do modelo de perceção;
- declarar se a perceção é idealizada, sem ruído e sem oclusão;
- transformar posição e estado cinemático para o sistema CPM de forma explícita;
- mapear cada veículo TraCI a um `objectId` protocolar através de uma política de atribuição com ciclo de vida, sem copiar o ID arbitrariamente;
- usar o tempo de simulação como entrada do modelo e convertê-lo para o domínio temporal CPM aprovado;
- manter o mesmo percurso comum de transporte e security.

Não afirmar que a integração SUMO simula uma câmara, radar ou lidar real. A primeira versão é uma fonte determinística de objetos percecionados a partir de informação de simulação.

### Critério de saída

Em runtime, mudanças previsíveis da cena SUMO alteram o conjunto de objetos CPM de forma verificável; as coordenadas, o tempo, a identidade e as contagens são coerentes entre TraCI, CPM TX, receção e analyzer.

## 22. Fase 10.15 — Robustez, cardinalidade, tamanho e segmentação

### Objetivo

Demonstrar que CPM lida com dados inválidos, conjunto variável de objetos e limites de tamanho sem corromper o estado ou o pipeline comum.

### Casos mínimos

```text
no eligible perceived objects
single object
multiple objects
maximum accepted test cardinality
object enters perception area
object leaves perception area / expires
object reappears after expiry
object identifier collision or invalid reuse
duplicate objectId within one message, if prohibited by selected profile
invalid reference position
invalid relative coordinates
invalid / inconsistent time values
unknown or unsupported object classification
missing conditional / mandatory container components
truncated UPER payload
wrong message ID / incompatible protocol version
wrong BTP dispatch
security rejection
replay duplicate
payload at and above configured size boundary
message segmentation and reconstruction, where applicable
physical packet duplicate versus repeated semantic object
```

Os testes devem distinguir explicitamente:

```text
transport failure
security verification failure
UPER decode failure
semantic validation failure
replay rejection
delivery rejection
object-selection omission
message-size / segmentation failure
```

### Segmentação e tamanho

O comportamento face a mensagens demasiado grandes deve seguir a edição CPM selecionada. Se o perfil prevê informação de segmentação e uma seleção de objetos resultar em mais do que uma mensagem, implementar e testar a semântica necessária ou limitar de forma explícita o primeiro baseline a payloads de uma única mensagem. Nunca truncar silenciosamente a lista de objetos ou emitir segmentação inválida. Uma limitação do MVP não pode ser confundida com conformidade integral ao serviço CPM.

### Critério de saída

Todos os casos têm resultado determinístico e razão de aceitação/rejeição observável. Nenhum caso malformado termina em `DELIVER_OK`; testes de payload grande demonstram o comportamento permitido pelo âmbito declarado.

## 23. Fase 10.16 — Regressão CAM + DENM + SPATEM

### Objetivo

Confirmar que CPM não degrada as funcionalidades já aceites.

### Regressão obrigatória

Executar o build e os conjuntos de testes atualmente aprovados, incluindo testes do analyzer e os cenários runtime de CAM, DENM e SPATEM definidos no estado oficial do projeto.

Quando o harness o permitir, executar também combinações:

```text
CAM-only
DENM-only
SPATEM-only
CPM-only
CAM + CPM
DENM + CPM
SPATEM + CPM
CAM + DENM + SPATEM + CPM
```

Validar que se mantêm:

```text
BTP dispatch and existing ports
message classification
PacketContext semantics and packetId
station identity / GN_ADDR binding
TX/RX event taxonomy
security and replay behavior
DCC / transport behavior
v2x_analyzer existing metrics and tests
scratch/CMakeLists.txt production target restriction
```

Se algum cenário combinado não estiver suportado pelo harness atual, registar a limitação e testar a compatibilidade dos handlers/caminhos afetados sem alegar um E2E que não foi executado.

### Critério de saída

Build, suite atual, testes CPM e cenários de regressão passam; qualquer diferença face à baseline é explicada e aceite tecnicamente antes do closeout.

## 24. Fase 10.17 — Closeout

A Etapa 10 só será considerada `CLOSED / VALIDATED` quando os gates obrigatórios da matriz de aceitação estiverem concluídos e as limitações remanescentes estiverem descritas com precisão.

O closeout deverá conter:

```text
selected ETSI CPM edition and ASN.1 / CDD compatibility decision
source and build evidence
unit / RED / GREEN evidence
clean build
codec known-vector and round-trip results
TX / RX runtime evidence
security and replay evidence or explicit bounded limitations
coordinate and temporal conversion evidence
synthetic E2E evidence
SUMO source integration evidence
robustness / size / segmentation results
CAM + DENM + SPATEM regression results
v2x_analyzer report and test results
final git diff / status / commit IDs
updated project state and documentation
```

Não declarar interoperabilidade OTA ou conformidade ETSI se apenas o teste E2E local tiver sido executado.

## 25. Matriz de aceitação

| ID | Critério | Evidência esperada | Estado inicial |
|---|---|---|---|
| CPM-01 | Gate Etapa 9 | closeout SPATEM formal e verificável — PR #2 / merge `13788fb8` | PASS |
| CPM-02 | Baseline pós-SPATEM | commit, worktree, build e testes registados | TBD |
| CPM-03 | Edição CPM | norma e perfil escolhidos explicitamente | TBD |
| CPM-04 | Vanetza API / ASN.1 / CDD | matriz de headers, símbolos e dependências reais | TBD |
| CPM-05 | Compatibilidade do codec | UPER encode/decode ligado ao perfil escolhido | TBD |
| CPM-06 | Estação e identidade | station type e separação dos IDs demonstrados | TBD |
| CPM-07 | Modelo neutro | tipos de domínio independentes do ASN.1 | TBD |
| CPM-08 | Coordenadas | transformação SUMO/local/WGS84/ENU testada | TBD |
| CPM-09 | Tempo | referência, medição e idade/delta testados | TBD |
| CPM-10 | Objeto percebido | objectId lifecycle e campos essenciais válidos | TBD |
| CPM-11 | Geração / inclusão | regras determinísticas, contagens e scheduler testados | TBD |
| CPM-12 | Codec round-trip | campos essenciais preservados | TBD |
| CPM-13 | BTP / GeoNetworking | transporte e dispatch validados em runtime/teste | TBD |
| CPM-14 | CPM TX | TX completo no caminho de produção | TBD |
| CPM-15 | CPM RX | decode, validação e entrega em runtime | TBD |
| CPM-16 | Security | resultado de geração/verificação de assinatura demonstrado | TBD |
| CPM-17 | Replay | duplicado aceite/rejeitado conforme política demonstrada | TBD |
| CPM-18 | PacketContext / logging | continuidade do pacote e taxonomia global preservadas | PASS - validacao local 10.11 |
| CPM-19 | Analyzer | métricas CPM e testes sem regressão | TBD |
| CPM-20 | Synthetic E2E | A -> ns-3 -> B com conteúdo conferido | TBD |
| CPM-21 | SUMO | objetos percebidos coerentes com a cena simulada | TBD |
| CPM-22 | Robustez | matriz Red/Green e rejeições diferenciadas | TBD |
| CPM-23 | Tamanho / segmentação | limites e comportamento sem truncagem silenciosa | TBD |
| CPM-24 | CAM regression | baseline CAM preservado | TBD |
| CPM-25 | DENM regression | baseline DENM preservado | TBD |
| CPM-26 | SPATEM regression | baseline SPATEM preservado | TBD |
| CPM-27 | Production build | `./ns3 build` PASS | TBD |
| CPM-28 | Runtime evidence | logs, contagens e resultados reproduzíveis | TBD |
| CPM-29 | Documentation | plano, estado, comandos e evidência atualizados | TBD |
| CPM-30 | Closeout | todos os gates obrigatórios aceites | TBD |

## 26. Evidência mínima por fase

Guardar, conforme aplicável:

```text
comandos executados e exit codes
commit / git status / diff
headers e símbolos encontrados
versões das especificações e módulos ASN.1 / CDD
fixtures de bytes CPM válidos
resultados RED e GREEN
resultados de build e link
payload size / byte count
logs TX/RX com PacketContext e packetId
valores semânticos de referência / objetos
resultados security / replay
relatório do v2x_analyzer
resultados E2E sintético e SUMO
resultados da regressão CAM / DENM / SPATEM
hashes/checksums dos artefactos de evidência, quando usados no projeto
```

A evidência deve distinguir:

```text
SOURCE EVIDENCE  -> a implementação corresponde ao requisito
BUILD EVIDENCE   -> o código compila e liga
RUNTIME EVIDENCE -> o comportamento ocorreu e foi observado
```

Para marcar um gate `PASS`, incluir o comando ou teste, resultado, artefacto e critério verificado. Não reutilizar logs de uma versão anterior como prova da versão atual.

## 27. Organização conceptual dos artefactos

Os nomes concretos dependem do código e API auditados. A estrutura esperada deverá seguir as convenções atuais sem criar módulos paralelos:

```text
scratch/vanetza_integration/
├── cpm/                         # domínio, geração e receção CPM
├── wrappers/
│   └── vanetza/
│       └── cpm/                 # builder / converter / codec específicos
├── context/                     # PacketContext partilhado, sem cópia CPM
├── rx/                          # pipeline comum e dispatch CPM
├── security/                    # infraestrutura partilhada
├── vanetza_runtime/             # orquestração existente
├── sumo_adapter/                # integração TraCI existente/extensão mínima
└── ...

scratch/test/
├── Green/CPM/
└── Red/CPM/
```

Não criar um `CpmBtpAdapter` estruturalmente copiado do CAM se o caminho comum puder ser configurado através da abstração existente. Não acrescentar targets temporários ao build de produção sem uma decisão arquitetural explícita.

## 28. Estratégia de commits

Cada gate funcional aceite deverá ter um commit separado, por exemplo:

```text
docs(cpm): define implementation and validation plan
test(cpm): capture post-spatem baseline
docs(cpm): record selected ASN.1 and CDD profile
test(cpm): add neutral object and coordinate fixtures
feat(cpm): add neutral CPM data model
test(cpm): validate UPER roundtrip and malformed payloads
feat(cpm): integrate CPM codec wrapper
test(cpm): verify common transport dispatch
feat(cpm): add CPM transmit path
feat(cpm): add CPM reception and semantic validation
feat(cpm): integrate common security and replay
test(cpm): add synthetic end-to-end scenario
feat(cpm): connect SUMO perception source
test(cpm): cover robustness and regressions
docs(cpm): close out implementation evidence
```

Os exemplos são indicativos e deverão ser ajustados ao conteúdo real do commit. Não acumular todas as fases num único commit grande.

## 29. Riscos técnicos e mitigação

### 29.1 Versões CPM / CDD incompatíveis

**Risco:** compilar com módulos ASN.1 de uma release e CDD/headers de outra, ou presumir que o upstream coincide com a instalação local.

**Mitigação:** gate 10.1, inventário da origem das bibliotecas, decisão escrita do perfil e fixtures do codec.

### 29.2 Coordenadas ou tempos semanticamente errados

**Risco:** payload válido em UPER com objetos colocados no lado errado da estação ou tempos de medição incorretos.

**Mitigação:** testes independentes de conversão espacial/temporal antes do E2E.

### 29.3 Confusão entre objectId e IDs de rede

**Risco:** reutilizar stationId, packetId ou TraCI ID diretamente como identidade CPM do objeto.

**Mitigação:** modelo neutro com identidades separadas, allocator explícito e testes de ciclo de vida.

### 29.4 CPM tratado como uma lista de todos os veículos SUMO

**Risco:** o payload representar estado global de simulação em vez da região/conjunto de objetos percebidos pela estação.

**Mitigação:** adapter de perceção determinístico e configurável, com alcance/regras e limitações documentadas.

### 29.5 Implementar só o codec

**Risco:** confundir um round-trip local com suporte CPM funcional.

**Mitigação:** exigir TX/RX no caminho comum, security/replay, PacketContext, analyzer e E2E.

### 29.6 Contar objetos como pacotes

**Risco:** métricas e identidade física incorretas para um payload com múltiplos objetos.

**Mitigação:** contadores e chaves separados para pacote, mensagem e objeto.

### 29.7 Crescimento excessivo do payload

**Risco:** truncagem silenciosa, segmentação errada ou envio de um conjunto de objetos que não cabe no perfil definido.

**Mitigação:** limites explícitos, teste de tamanho e comportamento de segmentação/seleção definido pela edição escolhida.

### 29.8 Regressões no restante projeto

**Risco:** alterações ao CDD, RX dispatch, security ou transporte afetarem CAM, DENM ou SPATEM.

**Mitigação:** baseline após Etapa 9 e regressão obrigatória na fase 10.16.

## 30. Definição de DONE

CPM `DONE` significa que o projeto demonstra, em runtime, um percurso coerente:

```text
known perception input
    -> object selection / CPM generation
    -> valid CPM matching declared ETSI profile
    -> UPER encode
    -> shared security path
    -> BTP / GeoNetworking
    -> ns-3 channel
    -> GeoNetworking / BTP RX
    -> verification / decode / semantic validation
    -> replay decision
    -> CPM / perceived-object delivery
```

com:

```text
reference position and time verified
object coordinate conversion verified
objectId semantics verified
PacketContext / packetId continuity verified
TX/RX taxonomy and logging preserved
v2x_analyzer CPM metrics validated
synthetic E2E PASS
SUMO perception-source E2E PASS
Red / Green robustness suite PASS
CAM / DENM / SPATEM regression PASS
production build PASS
evidence and documentation complete
```

O estado `DONE` não implica, por si só, interoperabilidade com equipamentos externos nem certificação de conformidade ETSI. Essas são atividades separadas e devem ter critérios e evidências próprios.

## 31. Próximo passo após aprovação

Depois de a Etapa 9 estar formalmente encerrada e este plano ser aprovado, o próximo passo é **10.0 — Gate de entrada e freeze pós-SPATEM**. Em seguida, executar **10.1 — Auditoria da API Vanetza, versão CPM e compatibilidade ASN.1 / CDD**.

Não criar modelos ASN.1, codecs ou handlers CPM antes de a fase 10.1 identificar e fixar a versão realmente usada.

## 32. Referências técnicas

- ETSI TS 103 324 V2.1.1 (2023-06), *Intelligent Transport System (ITS); Vehicular Communications; Basic Set of Applications; Collective Perception Service; Release 2*: https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf
- ETSI Work Programme, item `RTS/ITS-001985` — estado do trabalho de revisão TS 103 324 / Release 2: https://portal.etsi.org/webapp/WorkProgram/Report_WorkItem.asp?WKI_ID=69494
- Repositório ASN.1 ETSI para CPM TS 103 324: https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324
- Discussão upstream Vanetza sobre CPM TS 103 324 / versões ASN.1 e CDD: https://github.com/riebl/vanetza/issues/194
- Definição ASN.1 CPM antiga identificada no repositório Vanetza (usar apenas como pista de auditoria, não como prova da versão instalada): https://github.com/riebl/vanetza/blob/master/asn1/TR103562v211-CPM.asn
- Plano DENM de referência: [[ETAPA_8_DENM_PLANO]]
- Plano SPATEM de referência: [[ETAPA_9_SPATEM_PLANO]]
- Arquitetura Vanetza do projeto: `scratch/vanetza_integration/docs/vanetza_architecture.md` (confirmar caminho/nome no checkout atual)
- Estado atual do projeto: `scratch/vanetza_integration/docs/CURRENT_STATE.md` (confirmar revisão no checkout pós-SPATEM)

---

**Estado do documento:** PROPOSED — sujeito a aprovação; gate de encerramento da Etapa 9 satisfeito
**Próxima fase após aprovação:** 10.0 — freeze pós-SPATEM e reprodução do baseline local
