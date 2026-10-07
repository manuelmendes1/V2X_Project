cmake_minimum_required(VERSION 3.16)

build_exec(
    EXECNAME sumo_spatem_signal_parser_test
    EXECNAME_PREFIX scratch_
    SOURCE_FILES
        sumo_spatem_signal_parser_test.cc
        ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_parser.cc
    LIBRARIES_TO_LINK
        ${ns3-libs}
        ${ns3-contrib-libs}
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch
    IGNORE_PCH
)

target_include_directories(
    scratch_sumo_spatem_signal_parser_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/scratch
        ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
)
