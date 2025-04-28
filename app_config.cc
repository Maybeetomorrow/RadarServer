#include "app_config.h"
#include <fstream>
#include <drogon/HttpAppFramework.h>
#include <json/json.h>
#include <regex>
#include <filesystem>

using namespace drogon;
namespace fs = std::filesystem;

// Основная функция конфигурации приложения
void configureApplication() {
    Json::Value config;
    try {
        // Проверка существования конфигурационного файла
        if(!fs::exists("config.json")) {
            throw std::runtime_error("Файл config.json не найден в рабочей директории");
        }

        // Открытие файла для чтения
        std::ifstream configFile("config.json");
        if(!configFile) {
            throw std::runtime_error("Не удалось открыть config.json");
        }

        // Настройка парсера JSON
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        
        // Парсинг JSON с обработкой ошибок
        if(!Json::parseFromStream(readerBuilder, configFile, &config, &errs)) {
            throw std::runtime_error("Ошибка парсинга config.json: " + errs);
        }

        // Валидация структуры конфига
        if(!config.isMember("server") || !config["server"].isObject()) {
            throw std::runtime_error("Некорректная секция server в конфигурации");
        }

        // Загрузка конфигурации в Drogon
        app().loadConfigJson(config);
    }
    catch(const std::exception& e) {
        LOG_FATAL << "Ошибка конфигурации: " << e.what();
        throw; // Пробрасываем исключение дальше
    }
}

// Настройка заголовков безопасности и CORS
void setupSecurityHeaders() {
    app().registerPostHandlingAdvice([](const HttpRequestPtr& req, const HttpResponsePtr& resp) {
        // Обработка CORS (Cross-Origin Resource Sharing)
        const auto& origin = req->getHeader("Origin");
        if(origin.empty()) return; // Игнорируем запросы без Origin

        // Получаем список разрешенных источников из конфига
        const Json::Value& allowed = app().getCustomConfig()["security"]["allowed_origins"];
        
        // Проверяем каждый шаблон из конфига
        for(const auto& pattern : allowed) {
            try {
                // Преобразование wildcard-шаблона в regex
                std::string regexStr = std::regex_replace(
                    pattern.asString(),
                    std::regex("\\*"), // Заменяем звездочки
                    ".*"               // На regex-эквивалент
                );
                
                // Компилируем regex с игнорированием регистра
                std::regex re("^" + regexStr + "$", std::regex::icase);
                
                // Проверяем совпадение с origin
                if(std::regex_match(origin, re)) {
                    // Устанавливаем заголовки CORS
                    resp->addHeader("Access-Control-Allow-Origin", origin);
                    resp->addHeader("Vary", "Origin"); // Для кеширования
                    break;
                }
            }
            catch(const std::regex_error& e) {
                // Логируем ошибки некорректных regex
                LOG_ERROR << "Некорректный regex-шаблон: " 
                         << pattern.asString() << " - " << e.what();
            }
        }
            
        // Устанавливаем security headers:
        // Защита от MIME-sniffing
        resp->addHeader("X-Content-Type-Options", "nosniff");
        
        // Запрет встраивания в фреймы
        resp->addHeader("X-Frame-Options", "DENY");
        
        // Включение XSS-фильтра
        resp->addHeader("X-XSS-Protection", "1; mode=block");
    });
}