# Etapa 9 — pacote de evidências de regressão e closeout

Data: 2026-10-16
Registo: SPATEM_9_15_MATRIX_CLOSEOUT_2026-10-16

## Evidências por cenário

- CAM-only: 22 CAM transmitidos, 19 receções CAM.
- DENM-only: 4 DENM gerados, 1 entrega à aplicação.
- SPATEM-only: 53 transmissões BTP/2004, 90 descodificações e 90 entregas;
  zero gerações CAM/DENM e zero descartes PHY.
- CAM + DENM: 22 CAM transmitidos, 19 receções, 4 DENM gerados e 1 entrega.
- CAM + SPATEM: 28 CAM gerados, 21 receções, 61 transmissões SPATEM e
  87 entregas SPATEM.
- DENM + SPATEM: 4 DENM gerados, 1 entrega, 62 transmissões SPATEM e
  90 entregas SPATEM; 12 descartes PHY registados.
- CAM + DENM + SPATEM: 28 CAM gerados, 25 receções, 4 DENM gerados,
  1 entrega DENM, 94 transmissões SPATEM e 169 entregas; 16 descartes PHY.

## Build e suite

- Build limpo: `cmake --build cmake-cache --clean-first --target ns3.47-v2x_sumo-default -j2` — exit code 0.
- Suite: `python3 scratch/test/run_tests.py --suite all` — 39 testes,
  0 falhas, 0 erros; exit code 0.
- SPATEM-only e combinação tripla: simulador, injector e publisher com
  códigos de saída 0.
- Os logs individuais são preservados nas subdiretorias deste pacote.
- As contagens são observações das execuções registadas; não implicam
  entrega garantida de todos os pacotes.

## Limites assumidos no closeout

- SPATEM RSU usa `DummySignService`; não se afirma assinatura SPATEM
  com PKI de produção.
- O estágio VERIFY de SPATEM verifica o ITS-AID de transporte 137;
  não demonstra verificação criptográfica SPATEM ponta a ponta.
- Replay com timestamp semântico rejeita timestamps antigos e duplicados.
  Sem timestamp semântico, a supressão de duplicados usa uma janela de
  1000 us; o mesmo payload pode ser aceite após essa janela.
- Não foi demonstrada interoperabilidade externa nem execução em
  hardware ITS-G5.
- O closeout não cria commit nem altera o estado Git das alterações locais.
