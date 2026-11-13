set(target_prefix scratch_)

option(
    V2X_ENABLE_SECURITY_TEST_HOOKS
    "Compile fault-injection hooks for controlled security E2E tests"
    OFF
)

set(V2X_VANETZA_ROOT
    "$ENV{HOME}/opt/vanetza-cpm-gcc11"
    CACHE PATH
    "Root of the GCC-11-compatible Vanetza installation"
)

if(NOT EXISTS "${V2X_VANETZA_ROOT}/include/vanetza/security/v3/trust_store.hpp"
   OR NOT EXISTS "${V2X_VANETZA_ROOT}/lib/libvanetza_asn1_security_r2.a")
    message(FATAL_ERROR
        "The configured Vanetza prefix lacks the CPM V3 trust API or "
        "security R2 library: ${V2X_VANETZA_ROOT}"
    )
endif()

# =========================================================
# GLOBAL INCLUDE PATHS (NS-3 + PROJECT)
# =========================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    ${V2X_VANETZA_ROOT}/include
    ${V2X_VANETZA_ROOT}/include/vanetza
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1/support
)

link_directories("${V2X_VANETZA_ROOT}/lib")

# =========================================================
# OPENSSL
# =========================================================

find_package(OpenSSL REQUIRED)

set(V2X_CRYPTOPP_LIBRARY
    "/usr/local/lib/libcryptopp.so"
    CACHE FILEPATH
    "Crypto++ library required by the selected Vanetza static security backend"
)

if(NOT EXISTS "${V2X_CRYPTOPP_LIBRARY}")
    message(FATAL_ERROR
        "Crypto++ library not found: ${V2X_CRYPTOPP_LIBRARY}"
    )
endif()

# =========================================================
# GEOGRAPHICLIB (GCC-11 COMPATIBLE)
# =========================================================

set(V2X_GEOGRAPHICLIB_ROOT
    "$ENV{HOME}/opt/geographiclib-gcc11"
    CACHE PATH
    "Root of the GCC-11-compatible GeographicLib installation"
)

find_library(V2X_GEOGRAPHICLIB_LIBRARY
    NAMES GeographicLib
    PATHS "${V2X_GEOGRAPHICLIB_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT V2X_GEOGRAPHICLIB_LIBRARY)
    message(FATAL_ERROR
        "GCC-11-compatible GeographicLib not found in "
        "${V2X_GEOGRAPHICLIB_ROOT}/lib"
    )
endif()

# =========================================================
# VANETZA LIBS
# =========================================================

set(VANETZA_LIBS
    vanetza_access
    vanetza_asn1
    vanetza_asn1_its
    vanetza_asn1_its_r2
    vanetza_asn1_security
    vanetza_asn1_support
    vanetza_btp
    vanetza_common
    vanetza_dcc
    vanetza_facilities
    vanetza_geonet
    vanetza_geodesy
    vanetza_gnss
    vanetza_net
    vanetza_security
    vanetza_asn1_security_r2
)

# =========================================================
# CAM OBJECT LIBRARY
# =========================================================

add_library(vanetza_cam_encoder_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/builder/cam_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/cam/cam_asn1_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/cam/cam_converter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/geonet_mid_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/geonet/vanetza_geonet_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/decoder/cam_decoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/encoder/cam_encoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/generation_context/cam_generation_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/security_context/cam_security_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/scheduler/cam_scheduler.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/service/cam_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/sender/cam_sender.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/reception/cam_reception.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/transmission/cam_transmission.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/cam/uper/cam_uper_encoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/cam/uper/cam_uper_decoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cam/validation/cam_validator.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/neighbor/neighbor_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/neighbor/neighbor_logger.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/packet/packet_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/packet/packet_payload.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/packet/packet_types.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/packet/packet_id_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/rx_bridge/rx_bridge_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/its_time/its_start_time_parser.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/simulation_context/simulation_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/simulation_controller/simulation_controller.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/simulation_export/simulation_export.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/simulation_step/simulation_step.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/simulation_sync/simulation_sync.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/tx_bridge/tx_bridge_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/tx_bridge/tx_completion.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/tx_bridge/tx_binding_outcome.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/tx_bridge/tx_packet_binding_registry.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_builder/vehicle_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_identity/vehicle_identity_initializer.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_network/vehicle_network_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_registry/vehicle_registry.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_mobility/vehicle_mobility_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_mobility/vehicle_mobility_updater/vehicle_mobility_updater.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_security/vehicle_security_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_security/replay/replay_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/security/replay/replay_protection.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/position/ns3_position_provider.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_factory/vehicle_factory.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/core/v2x_log.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/core/packet_id_tag/packet_id_tag.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_pipeline.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_callback/rx_callback.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_context/rx_context.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_context_builder/rx_context_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_validation/rx_validation.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_unpack/rx_unpack.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_verify/rx_verify.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_decode/rx_decode.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/rx/rx_reception/rx_reception.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/security/cam_security.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/security/vehicle_crypto/vehicle_crypto.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_parser.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_mapper.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_timing_mapper.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_spatem_mapper.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_spatem_intersection_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_parser.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_vehicle_datagram.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_person_datagram.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vru/profile/sumo_vru_profile_mapper.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vru/role/state_machine/vru_role_state_machine.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vru/role/policy/vru_role_policy.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/sumo_person_vru_subject_synchronizer/sumo_person_vru_subject_synchronizer.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/sumo_person_registry/sumo_person_registry.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_serializer.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_socket.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/dcc/ns3_channel_load_probe.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/vehicle_security/file_credential_material_resolver.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vanetza_runtime/vanetza_runtime.cc

)

#include "vanetza_integration/context/simulation_step/simulation_step.h"

target_include_directories(vanetza_cam_encoder_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    ${V2X_VANETZA_ROOT}/include/vanetza
)

if(V2X_ENABLE_SECURITY_TEST_HOOKS)
    target_compile_definitions(vanetza_cam_encoder_obj PRIVATE V2X_ENABLE_SECURITY_TEST_HOOKS=1)
endif()

target_link_libraries(vanetza_cam_encoder_obj
    ${VANETZA_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =========================================================
# DENM OBJECT LIBRARY
# =========================================================

add_library(vanetza_denm_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/action/denm_action_id_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/lifecycle/denm_lifecycle_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/trigger/denm_trigger.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/builder/denm_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/service/denm_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/sender/denm_sender.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/scheduler/denm_scheduler.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/scenario/denm_scenario_event_source.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/scenario/sumo_denm_event_parser.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/scenario/denm_scenario_event_source.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/scheduler/denm_repetition_policy.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/rx/denm_rx_pipeline.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/denm/replay/denm_replay_protection.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/denm/denm_asn1_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/denm/vanetza_denm_codec.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/denm/uper/denm_uper_encoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/denm/uper/denm_uper_decoder.cc
)

target_include_directories(vanetza_denm_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    ${V2X_VANETZA_ROOT}/include
    ${V2X_VANETZA_ROOT}/include/vanetza
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1/support
)

target_link_libraries(vanetza_denm_obj
    ${VANETZA_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
)



# =========================================================
# SPATEM OBJECT LIBRARY
# =========================================================

add_library(vanetza_spatem_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/spatem/spatem_converter.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/service/spatem_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/emitter/spatem_emitter_endpoint.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/emitter/spatem_rsu_runtime.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/emitter/spatem_rsu_position_provider.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/rx/spatem_rx_pipeline.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/spatem/replay/spatem_replay_protection.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_encoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_decoder.cc
)

target_include_directories(vanetza_spatem_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    ${V2X_VANETZA_ROOT}/include
    ${V2X_VANETZA_ROOT}/include/vanetza
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1/support
)

target_link_libraries(vanetza_spatem_obj
    ${VANETZA_LIBS}
)

# =========================================================
# NEUTRAL ITS STATION FOUNDATION OBJECT LIBRARY
# =========================================================

add_library(vanetza_its_station_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/its_station/address/its_station_address_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/its_station/network/its_station_network_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/context/its_station_identity/its_station_identity_authority.cc
)

target_include_directories(vanetza_its_station_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    ${V2X_VANETZA_ROOT}/include
    ${V2X_VANETZA_ROOT}/include/vanetza
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1
    ${V2X_VANETZA_ROOT}/include/vanetza/asn1/support
)

target_link_libraries(vanetza_its_station_obj
    ${VANETZA_LIBS}
)

# =========================================================
# CPM PERCEPTION OBJECT LIBRARY
# =========================================================

add_library(vanetza_cpm_perception_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/rx/cpm_rx_pipeline.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/security/cpm_v3_sign_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/security/cpm_v3_verify_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/wrappers/vanetza/security/v2x_v3_verify_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/emitter/cpm_emitter_endpoint.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/emitter/cpm_rsu_position_provider.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/emitter/cpm_rsu_runtime.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/service/cpm_service.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/codec/cpm_uper_encoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/codec/cpm_uper_decoder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_coordinate_quantizer.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_coordinate_transform.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_geodetic_transform.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_synthetic_perception_source.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_temporal_transform.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_track_history.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_generation_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/cpm/perception/cpm_vehicle_perception_source.cc
)

target_include_directories(vanetza_cpm_perception_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
    "${V2X_GEOGRAPHICLIB_ROOT}/include"
)

# =========================================================
# VAM OBJECT LIBRARY
# =========================================================

add_library(vanetza_vam_obj OBJECT
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vam/generation/vam_generation_manager.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vam/perception/vam_proximity_provider.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vam/perception/vam_proximity_snapshot_builder.cc
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/vam/perception/vam_generation_evaluation_adapter.cc
)

target_include_directories(vanetza_vam_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/scratch
    ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
)

# =========================================================
# SCRATCH EXEC BUILDER
# =========================================================
function(create_scratch source_files)

    list(LENGTH source_files number_sources)
    if(number_sources EQUAL 0)
        return()
    endif()

    set(valid_sources "")

    foreach(source_file ${source_files})
        file(READ ${source_file} content)

        if(content MATCHES "int[ \t\r\n]*main[ \t\r\n]*\\(")
            list(APPEND valid_sources ${source_file})
        endif()
    endforeach()

    list(LENGTH valid_sources len)

    if(len EQUAL 0)
        return()
    endif()

    if(len GREATER 1)
        message(FATAL_ERROR "Multiple main() functions detected: ${valid_sources}")
    endif()

    list(GET valid_sources 0 main_file)

    get_filename_component(name ${main_file} NAME_WLE)

    build_exec(
        EXECNAME ${name}
        EXECNAME_PREFIX ${target_prefix}
        SOURCE_FILES ${source_files}
        LIBRARIES_TO_LINK
            ${ns3-libs}
            ${ns3-contrib-libs}
            -Wl,--start-group
            ${VANETZA_LIBS}
            -Wl,--end-group
            ${V2X_CRYPTOPP_LIBRARY}
            ${V2X_GEOGRAPHICLIB_LIBRARY}
            OpenSSL::SSL
            OpenSSL::Crypto
            $<TARGET_OBJECTS:vanetza_cam_encoder_obj>
            $<TARGET_OBJECTS:vanetza_denm_obj>
            $<TARGET_OBJECTS:vanetza_spatem_obj>
            $<TARGET_OBJECTS:vanetza_cpm_perception_obj>
            $<TARGET_OBJECTS:vanetza_its_station_obj>
            $<TARGET_OBJECTS:vanetza_vam_obj>
        EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch
        IGNORE_PCH
    )

    target_include_directories(${target_prefix}${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/scratch
        ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
        ${V2X_VANETZA_ROOT}/include/vanetza
    )

endfunction()

# =========================================================
# SCRATCH FILES (.cc ROOT LEVEL)
# =========================================================

# =========================================================
# UNICO EXECUTAVEL DA APLICACAO
# =========================================================
# Não descobrir automaticamente subdiretórios nem criar alvos
# a partir de ficheiros .cc encontrados em scratch.
# O único executável deste CMake é v2x_sumo.

create_scratch("${CMAKE_CURRENT_SOURCE_DIR}/v2x/v2x_sumo.cc")
