#pragma once

#include <string>
#include <memory>
#include <unistd.h>      // Для работы с POSIX API (close)
#include <stdexcept>     // Для исключений
#include <cstring>       // Для работы со строками C
#include <filesystem>    // Для работы с файловой системой
#include <locale>        // Для работы с локализацией

// Класс-страж для автоматической установки локали
struct LocaleGuard {
    LocaleGuard() { 
        std::setlocale(LC_ALL, "en_US.UTF-8"); // Установка локали для корректной работы с Unicode
    }
};

// Класс для безопасного управления файловыми дескрипторами (RAII)
class FileDescriptorGuard {
    int fd_;  // Хранимый файловый дескриптор
public:
    explicit FileDescriptorGuard(int fd) : fd_(fd) {}  // Явное получение дескриптора
    
    ~FileDescriptorGuard() { 
        if(fd_ != -1) close(fd_);  // Автоматическое закрытие при разрушении объекта
    }
    
    operator int() const { return fd_; }  // Неявное преобразование к int для удобства использования
};

// Прототипы функций:
std::string trim(const std::string& s);           // Обрезка пробелов в строке
std::string executeCommand(const char* cmd);      // Выполнение системной команды