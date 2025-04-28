#include "sd_bus.h"
#include <systemd/sd-bus.h>  // SystemD D-Bus API
#include <drogon/drogon.h>   // Для логирования

bool startService(const std::string& serviceName) {
    // Инициализация структур D-Bus
    sd_bus_error error = SD_BUS_ERROR_NULL;  // Контейнер для ошибок
    sd_bus_message* reply = nullptr;         // Ответ от D-Bus
    sd_bus* bus = nullptr;                   // Соединение с шиной

    // Подключаемся к системной шине D-Bus
    int ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        LOG_ERROR << "Ошибка подключения к системной шине: " << strerror(-ret);
        return false;
    }

    // Вызываем метод StartUnit у systemd
    ret = sd_bus_call_method(
        bus,                             // Соединение с шиной
        "org.freedesktop.systemd1",       // Имя сервиса systemd
        "/org/freedesktop/systemd1",      // Объект systemd
        "org.freedesktop.systemd1.Manager", // Интерфейс
        "StartUnit",                      // Название метода
        &error,                           // Контейнер для ошибок
        &reply,                           // Ответ
        "ss",                             // Сигнатура параметров (две строки)
        serviceName.c_str(),              // Имя целевого сервиса
        "replace"                         // Режим запуска (заменить существующий)
    );

    // Обработка ошибок вызова
    if (ret < 0) {
        LOG_ERROR << "Ошибка запуска сервиса " << serviceName 
                 << ": " << error.message;
        // Освобождаем ресурсы
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return false;
    }

    // Успешное выполнение
    LOG_INFO << "Сервис " << serviceName << " успешно запущен";
    
    // Освобождаем ресурсы
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    
    return true;
}