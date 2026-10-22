#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_TRACK_HISTORY_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_TRACK_HISTORY_H

#include <cstdint>
#include <string>
#include <unordered_map>

class CpmTrackHistory
{
public:
    // Regista uma observação no domínio TimestampIts, em milissegundos.
    //
    // Na primeira observação, o instante inicial e o último instante
    // aceite são ambos definidos para observationTimeItsMs.
    //
    // Em observações posteriores, preserva o primeiro instante e atualiza
    // o último instante aceite, desde que o timestamp não regrida.
    //
    // Em caso de erro, devolve false, preenche error e preserva tanto
    // o histórico como outputFirstObservedTimeItsMs.
    bool Observe(
        std::uint16_t objectId,
        std::int64_t observationTimeItsMs,
        std::int64_t& outputFirstObservedTimeItsMs,
        std::string& error);

    // Remove explicitamente o histórico de uma pista.
    // Uma observação subsequente desse identificador inicia nova pista.
    bool Forget(
        std::uint16_t objectId,
        std::string& error);

private:
    struct Track
    {
        std::int64_t firstObservedTimeItsMs{};
        std::int64_t lastObservedTimeItsMs{};
    };

    std::unordered_map<std::uint16_t, Track> m_tracks;
};

#endif
