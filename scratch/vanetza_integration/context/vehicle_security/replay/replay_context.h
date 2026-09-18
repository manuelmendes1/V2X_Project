#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace ns3 {

    // ============================================================================
    // ReplayEntry
    // ----------------------------------------------------------------------------
    // Estado de replay de um determinado StationID.
    //
    // lastGenerationDeltaTime:
    //     último GDT aceite.
    //
    // lastReceptionTimestamp:
    //     instante local, em microssegundos, da última CAM aceite.
    //
    // fingerprints:
    //     fingerprints das CAMs aceites com o GDT atual.
    // ============================================================================

    struct ReplayEntry {
        uint16_t lastGenerationDeltaTime = 0;

        uint64_t lastReceptionTimestamp = 0;

        std::unordered_set<uint64_t> fingerprints;
    };

    // ============================================================================
    // ReplayContext
    // ----------------------------------------------------------------------------
    // Base de dados local da proteção contra replay.
    //
    // Chave:
    //     StationID do emissor.
    // ============================================================================

    struct ReplayContext {
        std::unordered_map<uint32_t, ReplayEntry> database;
    };

} // namespace ns3
