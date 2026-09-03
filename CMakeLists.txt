cmake_minimum_required(VERSION 3.24)

project(aircraft_detector
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(
    BUILD_AIRCRAFT_DETECTOR
    "Build the production aircraft_detector executable and eCAL integration"
    ON
)

# ---------------------------------------------------------------------------
# Dependencies

find_package(Protobuf REQUIRED)
find_package(yaml-cpp REQUIRED)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
endif()

if(BUILD_AIRCRAFT_DETECTOR)
    find_package(eCAL REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Protobuf

set(RADAR_PROTO
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/radar.proto
)

protobuf_generate_cpp(
    RADAR_PROTO_SRCS
    RADAR_PROTO_HDRS
    ${RADAR_PROTO}
)

add_library(
    radar_proto
    ${RADAR_PROTO_SRCS}
    ${RADAR_PROTO_HDRS}
)

target_link_libraries(
    radar_proto
    PUBLIC
        protobuf::libprotobuf
)

target_include_directories(
    radar_proto
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

# ---------------------------------------------------------------------------
# Detector core

add_library(
    detector_lib
    src/detector.cpp
    src/detector_config.cpp
)

target_include_directories(
    detector_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    detector_lib
    PUBLIC
        radar_proto
        yaml-cpp::yaml-cpp
)

# ---------------------------------------------------------------------------
# Production with eCAL

if(BUILD_AIRCRAFT_DETECTOR)

    add_library(
        ecal_adapter_lib
        src/ecal_adapter.cpp
    )

    target_include_directories(
        ecal_adapter_lib
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(
        ecal_adapter_lib
        PUBLIC
            detector_lib
            eCAL::core
    )

    add_executable(
        aircraft_detector
        src/main.cpp
    )

    target_link_libraries(
        aircraft_detector
        PRIVATE
            ecal_adapter_lib
    )

endif()

# ---------------------------------------------------------------------------
# Tests

if(BUILD_TESTING)

    include(GoogleTest)

# -----------------------------------------------------------------------
# Unit tests

    add_executable(
        detector_unit_tests
        tests/unit/detector_test.cpp
    )

    target_include_directories(
        detector_unit_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(
        detector_unit_tests
        PRIVATE
            detector_lib
            GTest::gtest_main
    )

    gtest_discover_tests(
        detector_unit_tests
        PROPERTIES
            ENVIRONMENT
                "AIRCRAFT_DETECTOR_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/config/detector.yaml"
    )

# -----------------------------------------------------------------------
# Integration tests

    if(BUILD_AIRCRAFT_DETECTOR)

        add_executable(
            detector_integration_tests
            tests/integration/ecal_detector_test.cpp
            tests/integration/ecal_test_bus.cpp
            tests/integration/ecal_test_main.cpp
            tests/integration/aircraft_detector_process.cpp
        )

        target_include_directories(
            detector_integration_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/tests
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration
        )

        target_link_libraries(
            detector_integration_tests
            PRIVATE
                detector_lib
                eCAL::core
                GTest::gtest
        )

        gtest_discover_tests(
            detector_integration_tests
            PROPERTIES
                ENVIRONMENT
                    "AIRCRAFT_DETECTOR=$<TARGET_FILE:aircraft_detector>\;AIRCRAFT_DETECTOR_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/config/detector.yaml"
        )

        add_custom_target(
            integration
            COMMAND
                ${CMAKE_CTEST_COMMAND}
                --test-dir ${CMAKE_BINARY_DIR}
                --output-on-failure
            DEPENDS
                aircraft_detector
                detector_integration_tests
            USES_TERMINAL
        )

    endif()

endif()