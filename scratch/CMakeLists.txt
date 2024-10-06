find_package(PkgConfig REQUIRED)

add_executable(scratch scratch.cpp)

target_include_directories(scratch PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(cnats CONFIG REQUIRED)
option(NATS_BUILD_WITH_TLS "Build NATS with TLS support" OFF)
target_link_libraries(scratch PRIVATE cnats::nats_static)
# target_compile_definitions(scratch PRIVATE
#     ASIO_STANDALONE=1
#     ASIO_NO_TYPEID=1
# )
