#include "replay_protection.h"
#include "vanetza_integration/context/vehicle_security/replay/replay_context.h"

#include "vanetza_integration/core/v2x_log.h"

namespace {

    constexpr uint64_t REPLAY_WINDOW_US = 32768ULL * 1000ULL;

    inline int16_t CompareGenerationDeltaTime(
        uint16_t current,
        uint16_t previous)
    {
        return static_cast<int16_t>(current - previous);
    }

    inline uint64_t Fnv1a64(
        const std::vector<uint8_t>& bytes,
        std::size_t length)
    {
        constexpr uint64_t offsetBasis =
            14695981039346656037ULL;

        constexpr uint64_t prime =
            1099511628211ULL;

        uint64_t hash = offsetBasis;

        for (std::size_t i = 0; i < length; ++i) {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= prime;
        }

        return hash;
    }

} // anonymous namespace

namespace ns3 {

    bool ReplayProtection::Process(
        PacketContext& ctx,
        const CamMessage& message,
        ReplayContext& replay)
    {
        if (IsReplay(ctx, replay, message)) {

            ctx.MarkDrop();
            ctx.reason = "replay_detected";

            V2XWriteLog(
                ctx,
                V2XLogLevel::WARN,
                "REPLAY",
                "REPLAY_DROP",
                "station=" +
                    std::to_string(message.stationId) +
                " gdt=" +
                    std::to_string(message.generationDeltaTime));

            return false;
        }

        Update(ctx, replay, message);

        return true;
    }

    bool ReplayProtection::IsReplay(
        const PacketContext& ctx,
        const ReplayContext& replay,
        const CamMessage& message)
    {
        auto it =
            replay.database.find(message.stationId);

        if (it == replay.database.end()) {
            return false;
        }

        const ReplayEntry& entry = it->second;

        // =====================================================================
        // Expiração do estado
        //
        // GenerationDeltaTime é um contador de 16 bits.
        // Após metade do espaço modular (32768 ms), a relação temporal entre
        // o GDT atual e o anterior deixa de ser determinável de forma segura.
        //
        // Nesse caso, o estado antigo expira e a mensagem atual cria uma
        // nova âncora de replay.
        // =====================================================================

        if (ctx.timestamp >= entry.lastReceptionTimestamp &&
            (ctx.timestamp - entry.lastReceptionTimestamp) >=
                REPLAY_WINDOW_US)
        {
            return false;
        }

        const int16_t diff =
            CompareGenerationDeltaTime(
                message.generationDeltaTime,
                entry.lastGenerationDeltaTime);

        // Novo GDT dentro da janela válida.
        if (diff > 0) {
            return false;
        }

        // GDT anterior.
        if (diff < 0) {
            return true;
        }

        // Mesmo GDT:
        // replay somente se a mesma CAM já tiver sido aceite.
        const uint64_t fingerprint =
            ComputeCamFingerprint(ctx);

        return entry.fingerprints.find(fingerprint) !=
               entry.fingerprints.end();
    }

    void ReplayProtection::Update(
        const PacketContext& ctx,
        ReplayContext& replay,
        const CamMessage& message)
    {
        ReplayEntry& entry =
            replay.database[message.stationId];

        const uint64_t fingerprint =
            ComputeCamFingerprint(ctx);

        bool expired = false;

        if (entry.lastReceptionTimestamp != 0 &&
            ctx.timestamp >= entry.lastReceptionTimestamp &&
            (ctx.timestamp - entry.lastReceptionTimestamp) >=
                REPLAY_WINDOW_US)
        {
            expired = true;
        }

        const int16_t diff =
            CompareGenerationDeltaTime(
                message.generationDeltaTime,
                entry.lastGenerationDeltaTime);

        // =====================================================================
        // Primeira mensagem ou estado expirado.
        // =====================================================================

        if (entry.fingerprints.empty() || expired) {

            entry.lastGenerationDeltaTime =
                message.generationDeltaTime;

            entry.lastReceptionTimestamp =
                ctx.timestamp;

            entry.fingerprints.clear();
            entry.fingerprints.insert(fingerprint);

            return;
        }

        // =====================================================================
        // Novo GDT.
        // =====================================================================

        if (diff > 0) {

            entry.lastGenerationDeltaTime =
                message.generationDeltaTime;

            entry.lastReceptionTimestamp =
                ctx.timestamp;

            entry.fingerprints.clear();
            entry.fingerprints.insert(fingerprint);

            return;
        }

        // =====================================================================
        // Mesmo GDT.
        //
        // Uma CAM diferente pode legitimamente possuir o mesmo GDT.
        // =====================================================================

        if (diff == 0) {

            entry.fingerprints.insert(fingerprint);
            entry.lastReceptionTimestamp =
                ctx.timestamp;
        }
    }

    uint64_t ReplayProtection::ComputeCamFingerprint(
        const PacketContext& ctx)
    {
        const std::vector<uint8_t>& bytes =
            ctx.payload.bytes;

        const std::size_t signatureSize =
            static_cast<std::size_t>(
                ctx.payload.signatureSize);

        if (bytes.empty()) {
            return 0;
        }

        // Payload RX:
        //
        //     [CAM ASN.1][signature]
        //
        // Apenas os bytes CAM participam no fingerprint.

        if (signatureSize > bytes.size()) {
            return 0;
        }

        const std::size_t camSize =
            bytes.size() - signatureSize;

        return Fnv1a64(bytes, camSize);
    }

} // namespace ns3
