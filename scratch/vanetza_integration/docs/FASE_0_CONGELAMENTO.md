# HISTORICAL — Fase 0: Congelamento da Arquitetura

> **STATUS: HISTORICAL**
>
> **TYPE: FREEZE / HISTORICAL BASELINE**
>
> This document describes the project state frozen on 2026-07-22
> (`v5-stable`). It is retained for historical traceability.
>
> It is **not** the current operational baseline.
>
> The current operational baseline is defined in:
> `CURRENT_STATE.md`

**Data:** 2026-07-22
**Tag:** v5-stable
**Branch:** feature/objetivo-7-geonet-btp

## Baseline

- 3 veiculos (veh0, veh1, veh2)
- Station IDs: 27331, 90119, 17069
- Velocidade: 13.9 m/s
- CAMs TX: 20 totais (veh0=12, veh1=7, veh2=1)
- Rececoes RX OK: 17/20 (85%)
- Latencia TX: ~1ms (agendado)
- Propagacao media: 0.662ms (0.658–0.666ms)
- SUMO bridge: UDP 7400/7000, step 100ms
- Seguranca: ECDSA P-256, assinaturas 70–72B
- Framing: Proprietario (Header binario + payload + assinatura)

## Componentes Congelados (NAO ALTERAR)

| Modulo | Ficheiros |
|--------|-----------|
| PacketContext | packet_context.h/cc, packet_types.h/cc, packet_payload.h/cc, packet_id_manager.h/cc |
| VehicleRegistry | vehicle_registry.h/cc |
| VehicleState | vehicle_state.h |
| Security | cam_security.h/cc, vehicle_crypto.h/cc, vehicle_security_context.h/cc, vehicle_security_initializer.h/cc |
| ReplayProtection | replay_protection.h/cc, replay_context.h/cc |
| CAM Core | cam_service.h/cc, cam_scheduler.h/cc, cam_transmission.h/cc, cam_builder.h/cc, cam_encoder.h/cc, cam_decoder.h/cc, cam_asn1_builder.h/cc, cam_converter.h/cc, cam_uper_encoder.h/cc, cam_uper_decoder.h/cc, cam_validator.h/cc, cam_generation_context.h/cc |
| RX Pipeline Legado | rx_pipeline.h/cc, rx_callback.h/cc, rx_context.h/cc, rx_unpack.h/cc, rx_verify.h/cc, rx_decode.h/cc, rx_reception.h/cc, rx_validation.h/cc |
| CAM Sender/Receiver | cam_sender.h/cc, cam_receiver.h/cc, cam_reception.h/cc |
| Logging | v2x_log.h/cc, packet_id_tag.h/cc |
| SUMO Adapter | sumo_adapter.h/cc, sumo_parser.h/cc, sumo_serializer.h/cc, sumo_socket.h/cc |
| SIM Control | simulation_controller.h/cc, simulation_step.h/cc, simulation_context.h/cc, simulation_sync.h/cc, simulation_export.h/cc |
| Vehicle Factory/Builder | vehicle_factory.h/cc, vehicle_builder.h/cc, vehicle_identity_initializer.h/cc, vehicle_network_builder.h/cc, vehicle_mobility_context.h/cc, vehicle_mobility_updater.h/cc, vehicle_state_updater.h/cc, vehicle_dispatcher.h/cc |
| Neighbor | neighbor_manager.h/cc, neighbor_types.h/cc |
| CMakeLists | CMakeLists.txt (adicoes permitidas, nao remocoes) |
| Main | v2x_sumo.cc |

## Metricas de Regressao (para comparacao na Fase 11)

| Metrica | Valor v5 |
|---------|----------|
| Rececao (3 veh, moveis) | 17/20 = 85% |
| Latencia propagacao | 0.662ms |
| Latencia TX total | ~1ms |
| Assinaturas validas | 100% |
| Replay detetados | 100% (GDT) |
| Crash | 0 |

## Notas

- Framing proprietario sera mantido como fallback ate Fase 9.
- PacketIdManager ja integrado no CamService (v6).
- NeighborLogger temporariamente desativado (nao afeta funcionalidade).
- Backup manual: ../Code_ns3_evolution/Project_version/v55
