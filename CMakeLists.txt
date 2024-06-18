cmake_minimum_required(VERSION 3.24)
project(ws_binance_time_diff_delay)
set(CMAKE_CXX_STANDARD 17)
add_definitions(
        -DBOOST_THREAD_PROVIDES_FUTURE
        -DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
        -DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
)

add_executable(ws_binance_time_diff_delay
        ws_diff_delay.cpp
)

target_include_directories(ws_binance_time_diff_delay PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(ws_binance_time_diff_delay PRIVATE "C:/Users/Mike_Wei/CLionProjects/commonUtils")

find_package(RapidJSON CONFIG REQUIRED)
target_link_libraries(ws_binance_time_diff_delay PRIVATE rapidjson)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread date_time log log_setup program_options)

target_include_directories(ws_binance_time_diff_delay PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(ws_binance_time_diff_delay PRIVATE ${Boost_LIBRARIES})

find_package(Threads REQUIRED)
target_include_directories(ws_binance_time_diff_delay PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(ws_binance_time_diff_delay PRIVATE
        Boost::system
        Boost::log
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

find_package(jsoncpp CONFIG REQUIRED)
target_link_libraries(ws_binance_time_diff_delay PRIVATE JsonCpp::JsonCpp)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(ws_binance_time_diff_delay PRIVATE fmt::fmt)