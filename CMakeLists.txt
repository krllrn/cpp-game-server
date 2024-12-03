cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

#find_package(Boost 1.78.0 REQUIRED)
#if(Boost_FOUND)
#  include_directories(${Boost_INCLUDE_DIRS})
#endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Добавляем библиотеку, указывая, что она статическая.
add_library(game_lib STATIC
	src/postgres.h
	src/postgres.cpp
	src/extra_data.h
	src/connection_pool.h
	src/sdk.h
	src/sdk.cpp
	src/model.h
	src/model.cpp
	src/tagged.h
	src/json_loader.h
	src/json_loader.cpp
	src/dog.cpp
	src/dog.h
	src/game_session.cpp
	src/game_session.h
	src/map.cpp
	src/map.h
	src/players.cpp
	src/players.h
	src/loot_generator.cpp
	src/loot_generator.h
	src/application.cpp
	src/application.h
)

target_include_directories(game_lib PUBLIC CONAN_PKG::boost CONAN_PKG::libpq CONAN_PKG::libpqxx)
target_link_libraries(game_lib PUBLIC Threads::Threads PUBLIC CONAN_PKG::boost CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/request_handler.cpp
	src/request_handler.h
	
)
add_executable(game_server_tests
	src/http_server.cpp
	src/http_server.h
	src/request_handler.cpp
	src/request_handler.h
    tests/loot_generator_tests.cpp
)

target_link_libraries(game_server game_lib)

target_link_libraries(game_server_tests CONAN_PKG::catch2 game_lib) 