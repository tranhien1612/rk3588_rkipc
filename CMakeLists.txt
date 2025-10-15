cmake_minimum_required(VERSION 3.5)
project(main)

include_directories(
    # rkaiq header
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/algos
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/xcore
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/common
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/uAPI2
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/iq_parser
    ${PROJECT_SOURCE_DIR}/include/rkaiq/include/iq_parser_v2

    # rockit header
    ${PROJECT_SOURCE_DIR}/include/rockit/include
    # rtsp header
    ${PROJECT_SOURCE_DIR}/include/rtsp/include

    ${PROJECT_SOURCE_DIR}/include
)

set(DEP_COMMON_LIBS
    # rtsp libs
    ${CMAKE_CURRENT_LIST_DIR}/include/rtsp/libs/librtsp.a
    # rockit libs
    ${CMAKE_CURRENT_LIST_DIR}/include/rockit/libs/librockit.so
    ${CMAKE_CURRENT_LIST_DIR}/include/rockit/libs/libgraphic_lsf.so
    ${CMAKE_CURRENT_LIST_DIR}/include/rockit/libs/librkgfx_avs.so
    ${CMAKE_CURRENT_LIST_DIR}/include/rockit/libs/libRkSwCac.so
    # rkaiq libs
    ${CMAKE_CURRENT_LIST_DIR}/include/rkaiq/libs/librkaiq.so
    #rkaiq 
    pthread
)

# aux_source_directory(./ SRCS)
aux_source_directory(source SRCS)

add_executable(${PROJECT_NAME} main.cpp ${SRCS})
target_link_libraries(${PROJECT_NAME} ${DEP_COMMON_LIBS})