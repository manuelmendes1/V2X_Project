cmake_minimum_required(VERSION 3.16)

build_exec(
    EXECNAME spatem_synthetic_e2e_test
    EXECNAME_PREFIX scratch_
    SOURCE_FILES spatem_synthetic_e2e_test.cc
    LIBRARIES_TO_LINK
        ${ns3-libs}
        ${ns3-contrib-libs}
        ${VANETZA_LIBS}
        ${V2X_GEOGRAPHICLIB_LIBRARY}
        OpenSSL::SSL
        OpenSSL::Crypto
        $<TARGET_OBJECTS:vanetza_cam_encoder_obj>
        $<TARGET_OBJECTS:vanetza_denm_obj>
        $<TARGET_OBJECTS:vanetza_spatem_obj>
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch
    IGNORE_PCH
)

target_include_directories(
    scratch_spatem_synthetic_e2e_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/scratch
        ${CMAKE_SOURCE_DIR}/scratch/vanetza_integration
        /usr/local/include/vanetza
)
