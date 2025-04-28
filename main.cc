#include "controllers/car_controller/car_controller.h"
#include "utilities.h"
#include "app_config.h"
#include "sd_bus.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include "controllers/date_controller/date_controller.h"
#include "controllers/node_controller/node_controller.h"
#include "controllers/report_controller/report_controller.h"
#include "controllers/maintenance_report_controller/maintenance_report_controller.h"
#include "controllers/daily_report_controller/daily_report_controller.h"
#include "controllers/period_report_controller/period_report_controller.h"
#include "controllers/maintenance_controller/maintenance_controller.h"
#include "controllers/service_controller/service_controller.h"

using namespace drogon;
using namespace drogon::orm;

int main() {
    // Установка локали
    LocaleGuard localeGuard;

    // Понижение привилегий для безопасности
    if (geteuid() == 0) {
        if(setgid(1000) != 0 || setuid(1000) != 0) {
            LOG_FATAL << "Не удалось понизить привилегии";
            return EXIT_FAILURE;
        }
        LOG_INFO << "Привилегии успешно понижены";
    }

    // Проверка обязательных переменных окружения
    const std::vector<const char*> requiredEnv = {
        "CAR_ENCRYPTION_KEY",
        "CAR_ENCRYPTION_SALT",
        "DB_HOST",
        "DB_PORT",
        "DB_NAME",
        "DB_USER",
        "DB_PASSWORD",
        "DB_MAX_CONNECTIONS"
    };

    for (const auto& var : requiredEnv) {
        if (!getenv(var)) {
            LOG_FATAL << "Отсутствует обязательная переменная окружения: " << var;
            return EXIT_FAILURE;
        }
    }

    try {
        // Загрузка конфигурации приложения
        configureApplication();

        // Настройка безопасности
        setupSecurityHeaders();

        // Парсинг параметров БД
        const std::string dbHost = getenv("DB_HOST");
        const std::string dbName = getenv("DB_NAME");
        const std::string dbUser = getenv("DB_USER");
        const std::string dbPassword = getenv("DB_PASSWORD");
        
        uint16_t dbPort;
        size_t maxConnections;
        
        try {
            dbPort = static_cast<uint16_t>(std::stoi(getenv("DB_PORT")));
            maxConnections = static_cast<size_t>(std::stoi(getenv("DB_MAX_CONNECTIONS")));
        } catch (const std::exception& e) {
            LOG_FATAL << "Ошибка конвертации числовых параметров: " << e.what();
            return EXIT_FAILURE;
        }

        // Формирование строки подключения
        std::ostringstream connectionString;
        connectionString << "host=" << dbHost
                        << " port=" << dbPort
                        << " dbname=" << dbName
                        << " user=" << dbUser
                        << " password=" << dbPassword;

        // Инициализация БД
        auto dbClient = DbClient::newPgClient(connectionString.str(), maxConnections);
        
        // Регистрация контроллеров
        auto registerController = [&dbClient](auto controller) {
            app().registerController(controller);
            return controller;
        };

        registerController(std::make_shared<DateController>(dbClient));
        registerController(std::make_shared<NodeController>(dbClient));
        registerController(std::make_shared<ReportController>(dbClient));
        registerController(std::make_shared<MaintenanceReportController>(dbClient));
        registerController(std::make_shared<DailyReportController>(dbClient));
        registerController(std::make_shared<PeriodReportController>(dbClient));
        registerController(std::make_shared<MaintenanceController>(dbClient));
        registerController(std::make_shared<ServiceController>(dbClient));

        // Проверка подключения
        auto result = dbClient->execSqlSync("SELECT 1 AS connection_test;");
        LOG_INFO << "Проверка подключения к БД выполнена успешно";

    } catch(const std::exception& e) {
        LOG_FATAL << "Ошибка инициализации БД: " << e.what();
        return EXIT_FAILURE;
    }

    // Запуск системных сервисов
    if(!startService("setup_ap.service")) {
        LOG_FATAL << "Не удалось запустить сетевой сервис";
        return EXIT_FAILURE;
    }

    // Настройка обработчиков сигналов
    auto signalHandler = [](int sig) {
        LOG_INFO << "Получен сигнал " << sig << ", остановка сервера...";
        app().quit();
    };
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Получение параметров сервера из конфига
        const auto& config = app().getCustomConfig();
        const auto& serverConfig = config["server"];
        
        const std::string address = serverConfig.get("listen_address", "0.0.0.0").asString();
        const uint16_t port = serverConfig.get("port", 8080).asUInt();
        const unsigned threadNum = serverConfig.get("threads", std::thread::hardware_concurrency()).asUInt();

        LOG_INFO << "Запуск сервера на " << address << ":" << port;
        LOG_INFO << "Количество рабочих потоков: " << threadNum;

        app().setLogPath("./logs")
            .setLogLevel(trantor::Logger::kWarn)
            .addListener(address, port)
            .setThreadNum(threadNum)
            .run();

    } catch(const std::exception& e) {
        LOG_FATAL << "Ошибка запуска сервера: " << e.what();
        return EXIT_FAILURE;
    }

    LOG_INFO << "Сервер остановлен";
    return EXIT_SUCCESS;
}