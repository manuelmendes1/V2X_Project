#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace vanetza_integration
{
namespace vam
{

// Identificador ITS da estação.
// A representação é uint32_t, mas não existe conversão implícita
// entre este tipo e um inteiro genérico.
struct VamStationId
{
    std::uint32_t value{};
};

// Referências opacas e semanticamente distintas.
// Nenhuma destas referências representa stationId,
// identidade criptográfica ou identidade pessoal persistente.
struct SumoPersonReference
{
    std::string value;
};

struct Ns3NodeReference
{
    std::string value;
};

struct MobilitySourceReference
{
    std::string value;
};

struct LogicalItsStationReference
{
    std::string value;
};


// Identifica uma instância/incarnação do scheduler dentro deste processo.
// O valor zero é reservado para representar esgotamento/inexistência.
struct VamGenerationSchedulerInstanceId
{
    std::uint64_t value{};

    friend bool operator==(const VamGenerationSchedulerInstanceId&,
                           const VamGenerationSchedulerInstanceId&) = default;
};

// Marcador da presença SUMO atual. O valor zero é válido.
struct VamPersonPresenceEpoch
{
    std::uint64_t value{};

    friend bool operator==(const VamPersonPresenceEpoch&,
                           const VamPersonPresenceEpoch&) = default;
};

class VamGenerationTransactionHandle
{
public:
    VamGenerationSchedulerInstanceId SchedulerInstanceId() const noexcept
    {
        return schedulerInstanceId_;
    }

    const std::string& PersonId() const noexcept
    {
        return personId_;
    }

    VamPersonPresenceEpoch PresenceEpoch() const noexcept
    {
        return presenceEpoch_;
    }

    std::uint64_t TransactionId() const noexcept
    {
        return transactionId_;
    }

    friend bool operator==(const VamGenerationTransactionHandle&,
                           const VamGenerationTransactionHandle&) = default;

private:
    friend class VamGenerationScheduler;

    VamGenerationTransactionHandle(
        VamGenerationSchedulerInstanceId schedulerInstanceId,
        std::string personId,
        VamPersonPresenceEpoch presenceEpoch,
        std::uint64_t transactionId)
        : schedulerInstanceId_(schedulerInstanceId),
          personId_(std::move(personId)),
          presenceEpoch_(presenceEpoch),
          transactionId_(transactionId)
    {
    }

    VamGenerationSchedulerInstanceId schedulerInstanceId_{};
    std::string personId_;
    VamPersonPresenceEpoch presenceEpoch_{};
    std::uint64_t transactionId_{};
};

namespace detail
{

inline bool HasValidVamGenerationSchedulerInstanceId(
    const std::optional<VamGenerationSchedulerInstanceId>& identity) noexcept
{
    return identity.has_value() && identity->value != 0;
}

// Pure allocation primitive. Keeping the sequence as an argument permits a
// deterministic exhaustion test without exposing runtime fault configuration.
inline bool TryAllocateVamGenerationSchedulerInstanceId(
    std::atomic<std::uint64_t>& nextValue,
    VamGenerationSchedulerInstanceId& output) noexcept
{
    auto current = nextValue.load(std::memory_order_relaxed);

    while (current != 0)
    {
        const auto next = current == std::numeric_limits<std::uint64_t>::max()
            ? 0
            : current + 1;

        if (nextValue.compare_exchange_weak(
                current,
                next,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
        {
            output.value = current;
            return true;
        }
    }

    return false;
}

} // namespace detail

} // namespace vam
} // namespace vanetza_integration
