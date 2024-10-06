find_package(PkgConfig REQUIRED)

add_executable(bench_lat_pub_sub bench_lat_pub_sub.cpp)

target_include_directories(bench_lat_pub_sub PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(cnats CONFIG REQUIRED)
option(NATS_BUILD_WITH_TLS "Build NATS with TLS support" OFF)
target_link_libraries(bench_lat_pub_sub PRIVATE cnats::nats_static)
# target_compile_definitions(bench_lat_pub_sub PRIVATE
#     ASIO_STANDALONE=1
#     ASIO_NO_TYPEID=1
# )
