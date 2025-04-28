cmake_minimum_required(VERSION 3.5)
project(radar CXX)

# Настройка стандартов
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "Using C++${CMAKE_CXX_STANDARD} standard")

# Создание исполняемого файла
add_executable(${PROJECT_NAME}
    main.cc
    controllers/car_controller/car_controller.cc
    controllers/maintenance_controller/maintenance_controller.cc
    controllers/service_controller/service_controller.cc
    controllers/period_report_controller/period_report_controller.cc
    utilities.cc
    app_config.cc
    controllers/date_controller/date_controller.cc
    controllers/node_controller/node_controller.cc
    controllers/report_controller/report_controller.cc
    controllers/maintenance_report_controller/maintenance_report_controller.cc
    controllers/daily_report_controller/daily_report_controller.cc
    sd_bus.cc
)

# Подключение Drogon
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
)

# Линковка с filesystem (для GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# Зависимости
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
find_package(PostgreSQL REQUIRED)

# Линковка библиотек
target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    ${PostgreSQL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ${SYSTEMD_LIBRARIES}
)

# Включение заголовков PostgreSQL
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
)
