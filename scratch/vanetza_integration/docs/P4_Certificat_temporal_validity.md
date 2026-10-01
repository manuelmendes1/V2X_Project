P4 — Certificate Temporal Validity + ITS Time Mapping

Commit: 484565a95c2694a554b8afa574f1068c56486e95
Estado: CLOSED
Data: 2026-09-25

Objetivo

Introduzir um domínio temporal ITS absoluto e determinístico na simulação ns-3, permitindo que a validação temporal dos certificados DENM V3 seja aplicada efetivamente durante a assinatura.

Antes do P4, o ManualRuntime de Vanetza iniciava no epoch ITS (2004-01-01), enquanto os certificados reais do PKI tinham datas de validade em 2026. Para evitar essa incompatibilidade, a validação temporal estava desativada no signer.

O P4 substitui esse comportamento por:

--itsStartTime
      ↓
SimulationContext::itsStartTime
      ↓
Vanetza::Clock::at(...)
      ↓
ManualRuntime(ITS start)
      ↓
ITS start + Simulator::Now()
      ↓
DefaultCertificateValidator::valid_at_timepoint()
      ↓
DENM V3 SIGN
Ficheiros alterados
scratch/denm_v3_credential_rotation_test/denm_v3_credential_rotation_test.cc

Atualiza o teste de rotação para inicializar o ManualRuntime dentro da janela temporal do PKI.

scratch/v2x/v2x_sumo.cc

Adiciona a opção:

--itsStartTime=YYYY-MM-DDTHH:MM:SS

e publica o valor em SimulationContext.

scratch/vanetza_integration/context/simulation_context/simulation_context.h
scratch/vanetza_integration/context/simulation_context/simulation_context.cc

Adicionam o estado neutro:

std::string itsStartTime;

sem introduzir dependência de Vanetza no contexto genérico.

scratch/vanetza_integration/wrappers/vanetza/geonet/vanetza_geonet_adapter.cc

Implementa o mapeamento:

ITS absolute start + ns-3 simulation offset

Inicializa o ManualRuntime no instante ITS configurado e rejeita uma execução com credencial DENM real quando --itsStartTime não foi fornecido.

Também aceita o formato ISO-8601 com T, normalizando-o para o formato esperado por boost::posix_time.

scratch/vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.cc

Remove:

certificateValidator.disable_time_checks(true);

e passa a associar o DefaultCertificateValidator ao ManualRuntime:

certificateValidator.use_runtime(&m_runtime);
scratch/vanetza_integration/tests/test_denm_replay_protection.sh
scratch/vanetza_integration/tests/test_denm_invalid_signature.sh

Passam a fornecer explicitamente um itsStartTime determinístico aos testes DENM:

2026-09-24T13:38:21

com possibilidade de sobrescrita através de V2X_TEST_ITS_START_TIME.

Testes e evidências

Build

./ns3 build
BUILD_EXIT=0

Teste focal de validade temporal

VALIDITY_EXIT=0

Demonstrou:

certificado válido
→ assinatura DENM aceita

certificado expirado
→ assinatura DENM rejeitada

A rejeição após expiração produziu SignConfirmError::No_Certificate, conforme o comportamento do signer Vanetza.

Teste de rotação de credenciais

ROTATION_EXIT=0

Demonstrou:

assinatura inicial
→ mismatch certificado/chave rejeitado
→ signer anterior preservado
→ segunda credencial ativada
→ nova assinatura
→ identidade do certificado alterada

E2E DENM real

Com:

--enableDenm=1
--denmPkiDir=/home/mmendes/vanetza-pki-lab/validation-denm-pki
--itsStartTime=2026-09-24T13:38:21

foram demonstradas assinaturas V3 válidas, BTP-B port=2002, GeoNetworking GBC e repetição DENM.

A janela temporal dos certificados foi também diagnosticada diretamente. veh1/veh2 começam 13 008 s depois do início de veh0, e veh3 13 009 s depois. O instante 2026-09-24T13:38:21 foi escolhido como instante comum válido para os quatro ticket-000.

Replay protection

REPLAY_EXIT=0

Evidência:

denm_rx_count=70
verify_success_count=70
replay_ok_count=20
replay_drop_count=50
deliver_count=20

Invalid signature

INVALID_SIG_EXIT=0

Evidência:

tamper_count=32
verify_success_count=0
false_signature_count=1
dispatch_count=0
deliver_count=0

CAM regression

CAM_EXIT=0
CAM_ERRORS=0

A alteração temporal não quebrou a regressão CAM.

Patch integrity

git diff --check
→ PASS

O commit contém exatamente 8 ficheiros, 95 insertions / 14 deletions, e o working tree ficou limpo após o commit.

Limitações conhecidas

A validação temporal introduzida neste P4 cobre o caminho TX/signing. A validação temporal de certificados recebidos no RX continua fora do escopo e denm_v3_verify_service.cc mantém:

certificateValidator.disable_time_checks(true);

A política de validade por si só não resolve rotação automática de credenciais. A seleção/rotação automática baseada em expiração continua sendo responsabilidade de uma etapa posterior.

O itsStartTime é uma configuração explícita da simulação. Não existe uma data real implícita/default para credenciais DENM, evitando que o resultado dependa do relógio da máquina ou de uma data arbitrária.

A bridge SUMO continua a gerar DENM apenas quando o DenmHazardDetector identifica um novo hazard. Assim, um E2E pode produzir DENM para apenas alguns veículos dependendo do estado do cenário; isso não é uma limitação da política temporal.

O P4 também não introduz ainda políticas de revogação, cadeia de confiança, AID/SSP avançado ou critérios completos de trust validation no RX.

Estado final
P1  VehicleCredential                    CLOSED
P2  VehicleCredentialPool + Discovery     CLOSED
P3  Certificate/Private-Key Binding       CLOSED
P4  ITS Time Mapping + Temporal Validity  CLOSED

Commit P4: 484565a — feat(denm): enforce certificate temporal validity
