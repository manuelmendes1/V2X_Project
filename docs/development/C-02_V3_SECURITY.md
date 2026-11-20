# C-02 — Segurança ETSI ITS Security V3

## Objetivo e estado

C-02 elimina os caminhos de assinatura/verificação dummy do runtime de produção e torna explícita a seleção do verificador por ITS-AID. Uma assinatura de aplicação CAM por OpenSSL permanece como verificação adicional legada, mas não substitui a verificação do envelope ETSI Security V3.

A configuração é *fail-closed*: uma credencial ETSI, trust anchor ou resultado de verificação em falta não é convertido num sucesso dummy. Para compatibilidade com a assinatura CAM de aplicação já existente, o código continua a construir um X.509 OpenSSL local a partir da chave privada do ticket; esse certificado local não valida a cadeia ETSI e só é usado depois de a verificação V3 do envelope ter passado.

## Perfis de segurança

| ITS-AID | Serviço de assinatura TX | Serviço de verificação RX | Domínio PKI |
|---:|---|---|---|
| 36 — CAM | Ticket V3 de veículo | Verificador V3 de veículo | `--denmPkiDir` |
| 37 — DENM | Ticket V3 de veículo | Verificador V3 de veículo | `--denmPkiDir` |
| 137 — SPATEM | Ticket V3 dedicado da RSU | Verificador V3 dedicado SPATEM | `--spatemPkiDir` |
| 639 — CPM | Ticket V3 dedicado da RSU | Verificador V3 CPM existente | `--cpmPkiDir` |
| outros | rejeitado | `Configuration_Problem` | não configurado |

A partilha do domínio de veículo entre CAM e DENM não concede automaticamente os dois AIDs: o ticket tem de conter a autorização para cada protocolo que o veículo transmite. O serviço V3 aplica a permissão do AID solicitado durante a assinatura.

## Estrutura dos diretórios PKI

### Veículos — `--denmPkiDir`

O diretório tem de conter:

- `denm-root.cert`: trust anchor raiz.
- `denm-aa.cert`: certificado da autoridade emissora, carregado no issuer lookup.
- Um par de ticket/chave por veículo, em qualquer dos formatos suportados:
  - `<vehId>-ticket-<sequence>.cert` e `<vehId>-ticket-<sequence>.pem`;
  - ou o formato legado `<vehId>-ticket.cert` e `<vehId>-ticket.pem`.

A variável `V2X_DENM_PKI_DIR` continua a ser aceite quando `--denmPkiDir` não é fornecido. O runtime de veículo não inicia sem um par de ticket/chave validado.

### RSU SPATEM — `--spatemPkiDir`

O diretório tem de conter:

- `spatem-root.cert`: trust anchor raiz.
- `spatem-aa.cert`: emissor.
- `spatem-rsu-ticket-000.cert`: ticket autorizado para AID 137.
- `spatem-rsu-ticket-000.pem`: chave privada correspondente.

A validação do par certificado/chave é obrigatória. A opção é obrigatória quando `--enableSpatem=true`.

### RSU CPM — `--cpmPkiDir`

Mantém o contrato CPM já existente: `cpm-root.cert`, `cpm-aa.cert`, `cpm-rsu-ticket-000.cert` e `cpm-rsu-ticket-000.pem`. É obrigatório quando `--enableCpm=true`.

## Política de validação

Os verificadores V3 usam `StraightVerifyService`, cache de certificados, trust store com a raiz explicitamente confiada, issuer lookup, runtime ITS e posição WGS84. As verificações temporal e geográfica não são desactivadas. Os campos neutros `verificationPerformed`, `verificationSucceeded` e `verifiedItsAid` transportam o veredicto até ao pipeline de aplicação.

CAM e SPATEM recusam a entrega quando falta a prova V3 ou quando o AID verificado não coincide com o perfil esperado. CAM compara ainda o `stationId` ASN.1 com a identidade do emissor resolvida no boundary GeoNetworking através do MID e do `VehicleRegistry`; isto é uma verificação de consistência entre identidades, não uma afirmação de revogação nem uma validação independente da associação certificado-MID. A assinatura OpenSSL CAM preexistente continua depois desse gate como integridade adicional. O X.509 local criado a partir da chave do ticket não é uma trust anchor nem uma alternativa à cadeia V3.

A verificação CPM existente mantém-se separada, com domínio PKI próprio.

## Limites conhecidos

- Não foi acrescentada validação CRL/OCSP; a revogação não deve ser declarada coberta por C-02.
- VAM não tem perfil V3 de produção nesta alteração. AID 638 permanece não suportado e falha fechado.
- Os certificados e chaves são fornecidos externamente e não são guardados no repositório.
- Os testes de contrato e encaminhamento não substituem a compilação do target de produção ns-3/Vanetza nem um ensaio E2E com PKI real. Essa validação continua obrigatória antes de afirmar a segurança operacional do binário.

## Validação automatizada

Executar:

```bash
scripts/problem-tests.sh fix/c-02-unified-etsi-v3-security
```

O perfil C-02 compila e executa o teste nativo de classificação AID, verifica os contratos dos serviços V3 e corre regressões do wiring CPM e dos hooks de fault injection. O CI instala o compilador e executa o mesmo perfil. A validação com certificados reais e a compilação integral do target de produção devem ser executadas no ambiente ns-3/Vanetza utilizado para as experiências.
