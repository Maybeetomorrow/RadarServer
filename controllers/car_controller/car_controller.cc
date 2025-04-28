#include "car_controller.h"
#include "../../utilities.h"
#include "../../sd_bus.h"
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <zlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <regex>
#include <cstring>
#include <algorithm>

using namespace drogon;
namespace fs = std::filesystem;

// Конструктор контроллера
CarController::CarController() {
    // Получение секретов шифрования из переменных окружения
    secrets_.key = std::getenv("CAR_ENCRYPTION_KEY");
    secrets_.salt = std::getenv("CAR_ENCRYPTION_SALT");
    
    // Проверка наличия обязательных секретов
    if (secrets_.key.empty() || secrets_.salt.empty()) {
        LOG_FATAL << "Encryption secrets not configured";
        throw std::runtime_error("Server misconfiguration");
    }
}

// Обработчик создания файла с данными автомобиля (POST /car/create)
void CarController::createCarFile(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
    Json::Value response;
    // Блокировка мьютекса для обеспечения потокобезопасности операций с файлом
    std::lock_guard<std::mutex> lock(fileMutex_);

    try {
        const std::string filename = "car_detail.bin";
        
        // Проверка существования файла
        if (fs::exists(filename)) {
            throw std::runtime_error("File already exists");
        }

        // Парсинг входящего JSON
        auto json = req->getJsonObject();
        if(!json) throw std::runtime_error("Invalid JSON format");

        // Заполнение структуры данными из JSON
        CarDetails details;
        parseJsonToStruct(*json, details);

        // Шифрование данных автомобиля
        auto [encrypted, iv] = encryptData(details);
        
        // Вычисление хеша SHA-256 для проверки целостности
        auto shaHash = computeSHA256(details);
        
        // Вычисление контрольной суммы CRC32
        uint32_t crc = computeCRC32(details);

        // Создание файла с помощью RAII-обертки для дескриптора
        FileDescriptorGuard fd(open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR));
        if(fd == -1) throw std::runtime_error(strerror(errno));

        // Последовательная запись компонентов в файл:
        // 1. Вектор инициализации (16 байт)
        writeOrThrow(fd, iv.data(), iv.size(), "IV write failed");
        // 2. Зашифрованные данные
        writeOrThrow(fd, encrypted.data(), encrypted.size(), "Data write failed");
        // 3. Хеш SHA-256 (32 байта)
        writeOrThrow(fd, shaHash.data(), shaHash.size(), "SHA-256 write failed");
        // 4. Контрольная сумма CRC32 (4 байта)
        writeOrThrow(fd, &crc, sizeof(crc), "CRC32 write failed");

        // Логирование успешной операции
        LOG_INFO << "File created by " << req->getPeerAddr().toIp();
        // Отправка успешного ответа
        sendResponse(response["status"] = "File created successfully", k201Created, callback);
    }
    catch(const std::exception& e) {
        // Обработка ошибок и отправка клиенту 500 ошибки
        LOG_ERROR << "Create error: " << e.what();
        sendResponse(response["error"] = e.what(), k500InternalServerError, callback);
    }
}

// Обработчик получения информации об автомобиле (GET /car/info)
void CarController::getCarInfo(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback) {
    Json::Value response;
    // Блокировка мьютекса для обеспечения потокобезопасности
    std::lock_guard<std::mutex> lock(fileMutex_);

    try {
        const std::string filename = "car_detail.bin";
        
        // Проверка существования файла
        if(!fs::exists(filename)) {
            throw std::runtime_error("File not found");
        }

        // Открытие файла в бинарном режиме
        std::ifstream file(filename, std::ios::binary);
        auto fileSize = fs::file_size(filename);
        
        // Проверка минимального размера файла
        const size_t minSize = 16 + SHA256_DIGEST_LENGTH + sizeof(uint32_t);
        if(fileSize < minSize) {
            throw std::runtime_error("Invalid file size");
        }

        // Чтение компонентов из файла:
        std::vector<unsigned char> iv(16); // Вектор инициализации
        file.read(reinterpret_cast<char*>(iv.data()), iv.size());
        
        // Расчет размера зашифрованных данных
        const size_t encryptedSize = fileSize - iv.size() - SHA256_DIGEST_LENGTH - sizeof(uint32_t);
        std::vector<unsigned char> encryptedData(encryptedSize);
        file.read(reinterpret_cast<char*>(encryptedData.data()), encryptedSize);
        
        // Чтение сохраненного SHA-256 хеша
        std::vector<unsigned char> storedSHA(SHA256_DIGEST_LENGTH);
        file.read(reinterpret_cast<char*>(storedSHA.data()), storedSHA.size());
        
        // Чтение сохраненной контрольной суммы
        uint32_t storedCRC;
        file.read(reinterpret_cast<char*>(&storedCRC), sizeof(storedCRC));

        // Расшифровка данных
        CarDetails details = decryptData(encryptedData, iv);

        // Вычисление текущих значений хеша и CRC
        auto computedSHA = computeSHA256(details);
        uint32_t computedCRC = computeCRC32(details);

        // Проверка целостности данных
        if(computedSHA != storedSHA) {
            throw std::runtime_error("SHA-256 mismatch");
        }

        if(computedCRC != storedCRC) {
            throw std::runtime_error("CRC32 mismatch");
        }

        // Конвертация структуры в JSON
        Json::Value jsonResponse;
        convertToJson(details, jsonResponse);
        sendResponse(jsonResponse, k200OK, callback);
    }
    catch(const std::exception& e) {
        // Обработка ошибок чтения
        LOG_ERROR << "Read error: " << e.what();
        sendResponse(response["error"] = e.what(), k500InternalServerError, callback);
    }
}

// Запись данных в файл с гарантией полной записи
void CarController::writeOrThrow(int fd, const void* data, size_t size, const char* errorMsg) {
    ssize_t written = write(fd, data, size);
    if(written != static_cast<ssize_t>(size)) {
        throw std::runtime_error(std::string(errorMsg) + ": " + strerror(errno));
    }
}

// Вычисление SHA-256 хеша структуры
std::vector<unsigned char> CarController::computeSHA256(const CarDetails& data) {
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    
    // Инициализация контекста хеширования
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    // Обновление хеша данными структуры
    EVP_DigestUpdate(ctx, &data, sizeof(data));
    // Финализация хеша
    EVP_DigestFinal_ex(ctx, hash.data(), nullptr);
    
    // Очистка ресурсов OpenSSL
    EVP_MD_CTX_free(ctx);
    return hash;
}

// Вычисление контрольной CRC32
uint32_t CarController::computeCRC32(const CarDetails& data) {
    return crc32(0, reinterpret_cast<const Bytef*>(&data), sizeof(data));
}

// Формирование HTTP-ответа
void CarController::sendResponse(Json::Value& response, HttpStatusCode code,
                    std::function<void(const HttpResponsePtr&)>& callback) {
    Json::StreamWriterBuilder builder;
    // Настройка форматера JSON
    builder["emitUTF8"] = true;      // Использовать UTF-8
    builder["indentation"] = "";     // Без отступов
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(Json::writeString(builder, response));
    // Установка заголовков
    resp->setContentTypeCodeAndCustomString(CT_APPLICATION_JSON, 
                                          "application/json; charset=utf-8");
    resp->setStatusCode(code);
    callback(resp);
}

// Парсинг JSON в структуру CarDetails
void CarController::parseJsonToStruct(const Json::Value& json, CarDetails& details) {
    // Получение учетных данных Wi-Fi
    auto [ssid, password] = getSsidAndPassword();

    // Лямбда для безопасного копирования строк
    auto safeCopy = [](const std::string& src, char* dest, size_t max) {
        if(src.length() >= max) throw std::runtime_error("Field length exceeded");
        snprintf(dest, max, "%s", src.c_str());
    };      

    // Копирование SSID и пароля
    safeCopy(ssid, details.wi_fi, sizeof(details.wi_fi));
    safeCopy(password, details.password, sizeof(details.password));

    // Лямбда для обработки полей JSON
    auto copyField = [&](const char* field, char* dest, size_t max) {
        if(!json.isMember(field)) throw std::runtime_error("Missing field: " + std::string(field));
        safeCopy(json[field].asString(), dest, max);
    };

    // Обработка каждого поля с валидацией
    copyField("vin", details.vin, 18);
    validateVIN(details.vin); // Валидация VIN
    
    copyField("license_plate", details.license_plate, 10);
    copyField("brand", details.brand, 20);
    copyField("model", details.model, 20);
    copyField("body_type", details.body_type, 20);
    copyField("body_number", details.body_number, 10);
    copyField("engine_type", details.engine_type, 4);
    copyField("color", details.color, 20);
    
    // Обработка числовых полей
    details.year = json.get("year", 0).asInt();
    if(details.year < 1886 || details.year > 2024) {
        throw std::runtime_error("Invalid production year");
    }
    
    // Обработка поля трансмиссии
    std::string transmission = json.get("transmission", "").asString();
    details.transmission = !transmission.empty() ? transmission[0] : ' ';
    
    // Проверка объема двигателя
    details.engine_volume = json.get("engine_volume", 0.0f).asFloat();
    if(details.engine_volume < 0) {
        throw std::runtime_error("Invalid engine volume");
    }
    
    // Проверка мощности двигателя
    details.engine_power = json.get("engine_power", 0).asInt();
    if(details.engine_power < 0) {
        throw std::runtime_error("Invalid engine power");
    }
}

// Валидация VIN номера
void CarController::validateVIN(const char* vin) {
    static const std::regex vinRegex(R"(^[A-HJ-NPR-Z\d]{17}$)");
    if(!std::regex_match(vin, vinRegex)) {
        throw std::runtime_error("Invalid VIN format");
    }
}

// Конвертация структуры в JSON
void CarController::convertToJson(const CarDetails& details, Json::Value& json) {
    json["wi_fi"] = details.wi_fi;
    json["password"] = "[hidden]";  // Маскировка пароля
    json["vin"] = details.vin;
    json["license_plate"] = details.license_plate;
    json["brand"] = details.brand;
    json["model"] = details.model;
    json["year"] = details.year;
    json["transmission"] = std::string(1, details.transmission);
    json["body_type"] = details.body_type;
    json["body_number"] = details.body_number;
    json["engine_volume"] = details.engine_volume;
    json["engine_power"] = details.engine_power;
    json["engine_type"] = details.engine_type;
    json["color"] = details.color;
}

// Шифрование данных автомобиля
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> CarController::encryptData(const CarDetails& data) {
    unsigned char key[32];
    std::vector<unsigned char> iv(16); // Вектор инициализации
    
    // Генерация ключа и вектора инициализации
    deriveKey(key);
    if(RAND_bytes(iv.data(), 16) != 1) {
        throw std::runtime_error("IV generation failed");
    }

    // Создание контекста шифрования
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    auto ctxGuard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        ctx, EVP_CIPHER_CTX_free);
    
    // Инициализация AES-256-CBC
    if(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv.data()) != 1) {
        throw std::runtime_error("Encryption init failed");
    }

    // Шифрование данных
    const int blockSize = EVP_CIPHER_CTX_block_size(ctx);
    std::vector<unsigned char> ciphertext(sizeof(data) + blockSize);
    int len = 0, totalLen = 0;

    // Шифрование основного блока данных
    if(EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                       reinterpret_cast<const unsigned char*>(&data), sizeof(data)) != 1) {
        throw std::runtime_error("Encryption failed");
    }
    totalLen = len;

    // Финализация шифрования
    if(EVP_EncryptFinal_ex(ctx, ciphertext.data() + totalLen, &len) != 1) {
        throw std::runtime_error("Encryption finalization failed");
    }
    totalLen += len;

    ciphertext.resize(totalLen);
    return {ciphertext, iv};
}

// Дешифровка данных
CarDetails CarController::decryptData(const std::vector<unsigned char>& ciphertext, const std::vector<unsigned char>& iv) {
    unsigned char key[32];
    deriveKey(key); // Генерация ключа

    // Создание контекста дешифрования
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    auto ctxGuard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        ctx, EVP_CIPHER_CTX_free);
    
    // Инициализация AES-256-CBC
    if(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv.data()) != 1) {
        throw std::runtime_error("Decryption init failed");
    }

    CarDetails plaintext;
    int len = 0, totalLen = 0;

    // Дешифровка основного блока
    if(EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&plaintext), &len,
                       ciphertext.data(), ciphertext.size()) != 1) {
        throw std::runtime_error("Decryption failed");
    }
    totalLen = len;

    // Финализация дешифрования
    if(EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&plaintext) + totalLen, &len) != 1) {
        throw std::runtime_error("Decryption finalization failed");
    }
    totalLen += len;

    // Проверка размера расшифрованных данных
    if(totalLen != sizeof(CarDetails)) {
        throw std::runtime_error("Invalid decrypted data size");
    }

    return plaintext;
}

// Генерация ключа с использованием PBKDF2
void CarController::deriveKey(unsigned char* key) {
    auto& config = app().getCustomConfig();
    int iterations = config.get("security", "pbkdf2_iterations", 100000).asInt();

    // Использование PBKDF2 для генерации ключа
    if(PKCS5_PBKDF2_HMAC(
        secrets_.key.c_str(), secrets_.key.length(),
        reinterpret_cast<const unsigned char*>(secrets_.salt.c_str()), secrets_.salt.length(),
        iterations, EVP_sha256(), 32, key) != 1) {
        throw std::runtime_error("Key derivation failed");
    }
}

// Получение учетных данных Wi-Fi
std::pair<std::string, std::string> CarController::getSsidAndPassword() {
    static std::pair<std::string, std::string> credentials;
    static std::once_flag flag; // Гарантия однократного выполнения
    
    std::call_once(flag, [this] {
        try {
            // Расшифровка SSID
            credentials.first = executeCommand(
                "openssl enc -d -aes-256-cbc -a -salt -pbkdf2 -iter 10000 -pass file:/etc/wifi_ap/encryption_key -in /etc/wifi_ap/encrypted_ssid"
            );
            // Расшифровка пароля
            credentials.second = executeCommand(
                "openssl enc -d -aes-256-cbc -a -salt -pbkdf2 -iter 10000 -pass file:/etc/wifi_ap/encryption_key -in /etc/wifi_ap/encrypted_password"
            );
            
            // Проверка полученных данных
            if(credentials.first.empty() || credentials.second.empty()) {
                throw std::runtime_error("Invalid WiFi credentials");
            }
        } catch (const std::exception& e) {
            LOG_FATAL << "WiFi error: " << e.what();
            throw;
        }
    });
    
    return credentials;
}

void CarController::updateCarFile(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback) {
    Json::Value response;
    std::lock_guard<std::mutex> lock(fileMutex_);
    
    try {
        const std::string filename = "car_detail.bin";
        if(!fs::exists(filename)) {
            throw std::runtime_error("File not found");
        }

        // 1. Чтение текущих данных
        std::ifstream file(filename, std::ios::binary);
        std::vector<unsigned char> iv(16);
        file.read(reinterpret_cast<char*>(iv.data()), iv.size());
        
        auto fileSize = fs::file_size(filename);
        const size_t encryptedSize = fileSize - iv.size() - SHA256_DIGEST_LENGTH - sizeof(uint32_t);
        std::vector<unsigned char> encryptedData(encryptedSize);
        file.read(reinterpret_cast<char*>(encryptedData.data()), encryptedSize);
        
        CarDetails currentData = decryptData(encryptedData, iv);

        // 2. Парсинг входящего JSON
        auto json = req->getJsonObject();
        if(!json) throw std::runtime_error("Invalid JSON");

        // 3. Обновление разрешенных полей
        for (const auto& key : json->getMemberNames()) {
            const std::string field = key;
            const Json::Value& value = (*json)[key];
            
            if(field == "license_plate") {
                std::string plateValue = value.asString();
                if(plateValue.length() > 9) throw std::runtime_error("License plate too long");
                std::fill(std::begin(currentData.license_plate), 
                        std::end(currentData.license_plate), 0);
                std::copy(plateValue.begin(), plateValue.end(), currentData.license_plate);
            }
            else if(field == "transmission") {
                std::string transmissionValue = value.asString();
                if(transmissionValue.empty()) continue;
                currentData.transmission = transmissionValue[0];
            }
            else if(field == "body_type") {
                std::string bodyTypeValue = value.asString();
                if(bodyTypeValue.length() > 19) throw std::runtime_error("Body type too long");
                std::fill(std::begin(currentData.body_type), 
                        std::end(currentData.body_type), 0);
                std::copy(bodyTypeValue.begin(), bodyTypeValue.end(), currentData.body_type);
            }
            else if(field == "engine_volume") {
                float engineVolumeValue = value.asFloat();
                if(engineVolumeValue <= 0) throw std::runtime_error("Invalid engine volume");
                currentData.engine_volume = engineVolumeValue;
            }
            else if(field == "engine_power") {
                int enginePowerValue = value.asInt();
                if(enginePowerValue < 0) throw std::runtime_error("Invalid engine power");
                currentData.engine_power = enginePowerValue;
            }
            else if(field == "engine_type") {
                std::string engineTypeValue = value.asString();
                if(engineTypeValue.length() > 3) throw std::runtime_error("Engine type too long");
                std::fill(std::begin(currentData.engine_type), 
                        std::end(currentData.engine_type), 0);
                std::copy(engineTypeValue.begin(), engineTypeValue.end(), currentData.engine_type);
            }
            else if(field == "color") {
                std::string colorValue = value.asString();
                if(colorValue.length() > 19) throw std::runtime_error("Color name too long");
                std::fill(std::begin(currentData.color), 
                        std::end(currentData.color), 0);
                std::copy(colorValue.begin(), colorValue.end(), currentData.color);
            }
            else {
                LOG_WARN << "Attempt to modify restricted field: " << field;
                throw std::runtime_error("Modifying field " + field + " is prohibited");
            }
        }

        // 4. Перезапись файла
        auto newEncryptedData = encryptData(currentData);
        auto newSHA = computeSHA256(currentData);
        auto newCRC = computeCRC32(currentData);

        // Открываем файл для записи
        FileDescriptorGuard fd(open(filename.c_str(), O_WRONLY | O_TRUNC));

        // Записываем НОВЫЙ IV из newEncryptedData.second
        writeOrThrow(fd, newEncryptedData.second.data(), newEncryptedData.second.size(), "IV write failed");

        // Записываем зашифрованные данные
        writeOrThrow(fd, newEncryptedData.first.data(), newEncryptedData.first.size(), "Data write failed");

        // Записываем новые хеши и CRC
        writeOrThrow(fd, newSHA.data(), newSHA.size(), "SHA write failed");
        writeOrThrow(fd, &newCRC, sizeof(newCRC), "CRC write failed");

        sendResponse(response["status"] = "Update successful", k200OK, callback);
    }
    catch(const std::exception& e) {
        LOG_ERROR << "Update failed: " << e.what();
        sendResponse(response["error"] = e.what(), k400BadRequest, callback);
    }
}