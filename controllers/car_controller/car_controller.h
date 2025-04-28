#pragma once

// Заголовочные файлы
#include <drogon/HttpController.h>  // Базовый класс для HTTP контроллеров
#include <drogon/drogon.h>          // Основная библиотека Drogon
#include "../../struct_data/car_struct.h"             // Структура CarDetails
#include <mutex>                    // Мьютекс для синхронизации
#include <openssl/evp.h>            // OpenSSL функции шифрования
#include <vector>                   // Контейнер vector
#include <json/json.h>              // Работа с JSON
#include <unistd.h>                 // POSIX API (для close)

// Контроллер для работы с данными автомобиля
class CarController : public drogon::HttpController<CarController> {
public:
    CarController();  // Конструктор
    
    METHOD_LIST_BEGIN
        // Регистрация методов API:
        ADD_METHOD_TO(CarController::createCarFile, "/car/create", drogon::Post);
        ADD_METHOD_TO(CarController::getCarInfo, "/car/info", drogon::Get);
        ADD_METHOD_TO(CarController::updateCarFile, "/car/update", drogon::Patch);
    METHOD_LIST_END

    // Методы обработки запросов:
    void createCarFile(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    void getCarInfo(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    // Структура для хранения секретов шифрования
    struct EncryptionSecrets {
        std::string key;   // Ключ шифрования
        std::string salt;   // Соль для генерации ключа
    };

    std::mutex fileMutex_;          // Мьютекс для защиты доступа к файлу
    EncryptionSecrets secrets_;     // Секреты шифрования

    // Вспомогательные методы:
    void writeOrThrow(int fd, const void* data, size_t size, const char* errorMsg);
    std::vector<unsigned char> computeSHA256(const CarDetails& data);
    uint32_t computeCRC32(const CarDetails& data);
    void sendResponse(Json::Value& response, drogon::HttpStatusCode code,
                    std::function<void(const drogon::HttpResponsePtr&)>& callback);
    void parseJsonToStruct(const Json::Value& json, CarDetails& details);
    void validateVIN(const char* vin);
    void convertToJson(const CarDetails& details, Json::Value& json);
    std::pair<std::vector<unsigned char>, std::vector<unsigned char>> encryptData(const CarDetails& data);
    CarDetails decryptData(const std::vector<unsigned char>& ciphertext, const std::vector<unsigned char>& iv);
    void deriveKey(unsigned char* key);
    std::pair<std::string, std::string> getSsidAndPassword();
    void updateCarFile(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

};