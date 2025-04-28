#include "utilities.h"
#include <cctype>        // Для работы с isspace
#include <memory>        // Для unique_ptr
#include <stdexcept>     // Для исключений
#include <stdio.h>       // Для popen/pclose

// Функция обрезки пробелов в начале и конце строки
std::string trim(const std::string& s) {
    auto start = s.begin();
    // Пропускаем начальные пробелы
    while (start != s.end() && std::isspace(*start)) start++;
    
    auto end = s.end();
    // Пропускаем конечные пробелы
    do { 
        end--; 
    } while (end > start && std::isspace(*end));
    
    return std::string(start, end+1);
}

// Функция выполнения системной команды с захватом вывода
std::string executeCommand(const char* cmd) {
    char buffer[128];  // Буфер для чтения вывода
    std::string result;
    
    // Лямбда для автоматического закрытия pipe
    auto pipe_deleter = [](FILE* f) { 
        if(f) pclose(f); 
    };
    
    // Умный указатель с кастомным делитером
    std::unique_ptr<FILE, decltype(pipe_deleter)> pipe(popen(cmd, "r"), pipe_deleter);
    
    if (!pipe) throw std::runtime_error("Ошибка открытия pipe");
    
    // Чтение вывода команды построчно
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    
    return trim(result);  // Возвращаем обрезанный результат
}