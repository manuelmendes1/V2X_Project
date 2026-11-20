#include <vanetza/btp/data_interface.hpp>
#include <vanetza/btp/header.hpp>
#include <vanetza/btp/header_conversion.hpp>
#include <vanetza/btp/port_dispatcher.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/geonet/data_indication.hpp>
#include <vanetza/net/chunk_packet.hpp>
#include <vanetza/net/osi_layer.hpp>
#include <vanetza/net/packet.hpp>

#include <boost/variant/get.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

class CountingHandler final : public vanetza::btp::IndicationInterface
{
public:
    unsigned calls{0};
    uint16_t lastPort{0};
    bool lastPacketPresent{false};
    std::vector<uint8_t> lastPayload;

    void indicate(
        const vanetza::btp::DataIndication& indication,
        std::unique_ptr<vanetza::UpPacket> packet) override
    {
        ++calls;
        lastPort = indication.destination_port.get();
        lastPacketPresent = static_cast<bool>(packet);
        lastPayload.clear();

        if (!packet) {
            return;
        }

        if (auto* chunk = boost::get<vanetza::ChunkPacket>(packet.get())) {
            vanetza::ByteBuffer payload;
            chunk->layer(vanetza::OsiLayer::Application).convert(payload);
            lastPayload.assign(payload.begin(), payload.end());
        }
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

    vanetza::ByteBuffer payload{0xCA, 0xFE, 0x63, 0x80};
    packet.layer(vanetza::OsiLayer::Application) = std::move(payload);

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
    CountingHandler vam;

    const std::array<CountingHandler*, 5> handlers{
        &cam, &denm, &spatem, &cpm, &vam
    };
    const std::array<uint16_t, 5> ports{
        2001, 2002, 2004, 2009, 2018
    };

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        dispatcher.set_non_interactive_handler(
            vanetza::btp::port_type(ports[i]),
            handlers[i]);
    }

    unsigned undispatchedCalls = 0;
    uint16_t undispatchedPort = 0;

    dispatcher.hook_undispatched =
        [&](const vanetza::geonet::DataIndication&,
            const vanetza::btp::DataIndication* indication)
        {
            ++undispatchedCalls;
            if (indication) {
                undispatchedPort = indication->destination_port.get();
            }
        };

    for (std::size_t i = 0; i < ports.size(); ++i) {
        std::array<unsigned, 5> before{};
        for (std::size_t j = 0; j < handlers.size(); ++j) {
            before[j] = handlers[j]->calls;
        }

        InjectBtpB(dispatcher, ports[i]);

        for (std::size_t j = 0; j < handlers.size(); ++j) {
            const unsigned expectedDelta = (i == j) ? 1u : 0u;
            const unsigned actualDelta = handlers[j]->calls - before[j];

            if (actualDelta != expectedDelta) {
                std::cerr << "FAIL BTP_PORT_ISOLATION"
                          << " injected=" << ports[i]
                          << " registered=" << ports[j]
                          << " expected_delta=" << expectedDelta
                          << " actual_delta=" << actualDelta << '\n';
                return 1;
            }
        }

        if (!handlers[i]->lastPacketPresent ||
            handlers[i]->lastPort != ports[i]) {
            std::cerr << "FAIL BTP_HANDLER_INDICATION port="
                      << ports[i] << '\n';
            return 2;
        }

        const std::vector<uint8_t> expectedPayload{
            0xCA, 0xFE, 0x63, 0x80
        };
        if (handlers[i]->lastPayload != expectedPayload) {
            std::cerr << "FAIL APPLICATION_PAYLOAD_PRESERVATION port="
                      << ports[i] << '\n';
            return 3;
        }

        std::cout << "PASS BTP_PORT_" << ports[i] << "_ISOLATED\n";
    }

    const std::array<unsigned, 5> beforeUnknown{
        cam.calls, denm.calls, spatem.calls, cpm.calls, vam.calls
    };

    constexpr uint16_t unknownPort = 2500;
    InjectBtpB(dispatcher, unknownPort);

    if (undispatchedCalls != 1 || undispatchedPort != unknownPort) {
        std::cerr << "FAIL UNKNOWN_PORT_UNDISPATCHED\n";
        return 4;
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        if (handlers[i]->calls != beforeUnknown[i]) {
            std::cerr << "FAIL UNKNOWN_PORT_REACHED_REGISTERED_HANDLER "
                      << ports[i] << '\n';
            return 5;
        }
    }

    std::cout << "PASS VAM_2018_APPLICATION_PAYLOAD_PRESERVED\n";
    std::cout << "PASS UNKNOWN_PORT_UNDISPATCHED\n";
    std::cout << "VAM_BTP_PORT_DISPATCHER=PASS\n";
    return 0;
}
