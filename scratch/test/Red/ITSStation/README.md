# Etapa 11.5.1 — RED tests ITS Station

Este patch acrescenta apenas testes. Não altera os headers, implementações, o CMake de produção, os runtimes CPM/SPATEM ou qualquer configuração do build.

## Ficheiros

- `test_its_station_foundation.py` compila e liga o harness C++ temporário usando as receitas Ninja de `scratch_v2x_sumo` e executa os contratos de estado, GN_ADDR, registry, mobilidade fixa/dinâmica, PHY audit e `Ns3AccessAdapter`.
- `test_its_station_foundation.cc` contém as verificações comportamentais do harness.
- `test_its_station_cmake_wiring.py` verifica a futura biblioteca `vanetza_its_station_obj`, a remoção dos builders neutros de `vanetza_spatem_obj` e os novos caminhos de include.

## Execução no checkout ns-3.47

```bash
python3 scratch/test/Red/ITSStation/test_its_station_cmake_wiring.py
python3 scratch/test/Red/ITSStation/test_its_station_foundation.py
```

O resultado esperado nesta fase RED é que ambos falhem: a localização neutra, a nova API de mobilidade/PHY, o construtor genérico do Access Adapter e a biblioteca CMake ainda não foram implementados. A falha do harness deve ser de compilação, com mensagens concretas sobre os headers/API em falta; não é um teste de runtime até à fase GREEN.

Depois da implementação, ambos devem passar. O harness requer o build de produção já gerado em `cmake-cache/` e o executável `build/scratch/ns3.47-v2x_sumo-default`, tal como o teste E2E CPM usado como padrão.

## Aplicação

Na raiz do repositório e na branch de trabalho pretendida:

```bash
git apply --check etapa11_5_1_red_tests_v2.patch
git apply etapa11_5_1_red_tests_v2.patch
```

Não há commits incluídos neste patch.
