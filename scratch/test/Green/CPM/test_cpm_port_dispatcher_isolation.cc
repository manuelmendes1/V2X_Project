#include <vanetza/btp/data_interface.hpp>
#include <vanetza/btp/header.hpp>
#include <vanetza/btp/header_conversion.hpp>
#include <vanetza/btp/port_dispatcher.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/geonet/data_indication.hpp>
#include <vanetza/net/osi_layer.hpp>
#include <vanetza/net/packet.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

class CountingHandler final : public vanetza::btp::IndicationInterface
{
public:
    unsigned calls{0};
    uint16_t lastPort{0};
    bool lastPacketPresent{false};

    void indicate(
        const vanetza::btp::DataIndication& indication,
        std::unique_ptr<vanetza::UpPacket> packet) override
    {
        ++calls;
        lastPort = indication.destination_port.get();
        lastPacketPresent = static_cast<bool>(packet);
    }
};

static void InjectBtpB(
    vanetza::btp::PortDispatcher& dispatcher,
    uint16_t port)
{
    vanetza::ChunkPacket packet;

    vanetza::btp::HeaderB header{};
    header.destination_port = vanetza::btp::port_type(port);
    header.destination_port_info = vanetza::uint16be_t(0);

    packet.layer(vanetza::OsiLayer::Transport) = header;

    vanetza::ByteBuffer payload{0xCA, 0xFE};
    packet.layer(vanetza::OsiLayer::Application) = std::move(payload);

    // Diagnose the serialized transport header and parse it independently.
    // Work on a copy because parsing may consume or modify packet layers.
    vanetza::ByteBuffer transportBytes;
    packet.layer(vanetza::OsiLayer::Transport).convert(transportBytes);

    std::cout << "BTP_TRANSPORT_BYTES=" << transportBytes.size() << ":";
    for (const auto byte : transportBytes)
    {
        std::cout << std::hex
                  << static_cast<unsigned>(byte) << " ";
    }
    std::cout << std::dec << "\\n";

    vanetza::ChunkPacket parserProbe(packet);
    try
    {
        const auto parsedHeader = vanetza::btp::parse_btp_b(parserProbe);
        std::cout << "DIRECT_BTP_B_PARSE=PASS port="
                  << parsedHeader.destination_port.get() << "\\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "DIRECT_BTP_B_PARSE=FAIL reason="
                  << error.what() << "\\n";
        throw;
    }

    auto upPacket =
        std::make_unique<vanetza::UpPacket>(std::move(packet));

    vanetza::geonet::DataIndication geoIndication{};
    geoIndication.upper_protocol =
        vanetza::geonet::UpperProtocol::BTP_B;
    geoIndication.transport_type =
        vanetza::geonet::TransportType::SHB;

    dispatcher.indicate(geoIndication, std::move(upPacket));
}

int main()
{
    using vanetza::btp::PortDispatcher;

    PortDispatcher dispatcher;

    CountingHandler cam;
    CountingHandler denm;
    CountingHandler spatem;
    CountingHandler cpm;

    const std::array<CountingHandler*, 4> handlers{
        &cam, &denm, &spatem, &cpm
    };
    const std::array<uint16_t, 4> ports{2001, 2002, 2004, 2009};

    for (std::size_t i = 0; i < handlers.size(); ++i)
    {
        dispatcher.set_non_interactive_handler(
            vanetza::btp::port_type(ports[i]),
            handlers[i]);
    }

    unsigned undispatchedCalls = 0;
    uint16_t undispatchedPort = 0;
    bool undispatchedHeaderParsed = false;

    dispatcher.hook_undispatched =
        [&](const vanetza::geonet::DataIndication&,
            const vanetza::btp::DataIndication* indication)
        {
            ++undispatchedCalls;
            if (indication)
            {
                undispatchedHeaderParsed = true;
                undispatchedPort = indication->destination_port.get();
            }
        };

    for (std::size_t i = 0; i < ports.size(); ++i)
    {
        const std::array<unsigned, 4> callsBeforeInjection{
            cam.calls, denm.calls, spatem.calls, cpm.calls
        };

        InjectBtpB(dispatcher, ports[i]);

        for (std::size_t j = 0; j < handlers.size(); ++j)
        {
            const unsigned expectedCalls = (i == j) ? 1u : 0u;
            const unsigned actualDelta =
                handlers[j]->calls - callsBeforeInjection[j];

            if (actualDelta != expectedCalls)
            {
                std::cerr << "FAIL PORT_ISOLATION"
                          << " injected=" << ports[i]
                          << " handler_port=" << ports[j]
                          << " expected_delta=" << expectedCalls
                          << " actual_delta=" << actualDelta
                          << '\n';
                return 1;
            }
        }

        if (handlers[i]->lastPort != ports[i] ||
            !handlers[i]->lastPacketPresent)
        {
            std::cerr << "FAIL HANDLER_INDICATION"
                      << " port=" << ports[i] << '\n';
            return 2;
        }

        std::cout << "PASS BTP_PORT_" << ports[i] << "_ISOLATED\n";
    }

    const auto callsBeforeUnknown = std::array<unsigned, 4>{
        cam.calls, denm.calls, spatem.calls, cpm.calls
    };

    constexpr uint16_t unknownPort = 2500;
    InjectBtpB(dispatcher, unknownPort);

    if (undispatchedCalls != 1 ||
        !undispatchedHeaderParsed ||
        undispatchedPort != unknownPort)
    {
        std::cerr << "FAIL UNDISPATCHED_HOOK"
                  << " calls=" << undispatchedCalls
                  << " header_parsed=" << undispatchedHeaderParsed
                  << " port=" << undispatchedPort << '\n';
        return 3;
    }

    for (std::size_t i = 0; i < handlers.size(); ++i)
    {
        if (handlers[i]->calls != callsBeforeUnknown[i])
        {
            std::cerr << "FAIL UNKNOWN_PORT_REACHED_REGISTERED_HANDLER"
                      << " registered_port=" << ports[i] << '\n';
            return 4;
        }
    }

    std::cout << "PASS UNKNOWN_PORT_UNDISPATCHED\n";
    std::cout << "CPM_PORT_DISPATCHER_ISOLATION=PASS\n";
    return 0;
}
