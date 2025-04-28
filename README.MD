# RadarServer

Серверное приложение для управления данными автомобилей, генерации отчетов и интеграции с системными сервисами.

## Описание
- Создание, обновление и чтение зашифрованных данных об автомобилях.
- Генерация ежедневных, периодических и технических отчетов.
- Интеграция с PostgreSQL для хранения данных.
- Управление сервисами через SystemD D-Bus.
- Поддержка CORS и security headers.

## Требования
- **Компилятор C++** с поддержкой C++17.
- **Drogon Framework** (версия >= 1.8.0).
- **PostgreSQL** (версия >= 12).
- **OpenSSL** (для шифрования данных).
- **Systemd** и **libsystemd-dev** (для работы с D-Bus).
- **JsonCpp** (для работы с JSON).

## Установка
1. Клонируйте репозиторий:
   ```bash
   git clone https://github.com/Maybeetomorrow/RadarServer.git
   ```
2. Установите зависимости:
   ```bash
   sudo apt install libdrogon-dev postgresql libjsoncpp-dev libsystemd-dev openssl
   ```
3. Соберите проект:
   ```bash
   mkdir build && cd build
   cmake .. && make
   ```

## Конфигурация
1. Создайте `config.json` в корневой директории:
   ```json
   {
     "server": {
       "listen_address": "0.0.0.0",
       "port": 8080,
       "threads": 4
     },
     "security": {
       "allowed_origins": ["*"],
       "pbkdf2_iterations": 100000
     }
   }
   ```
2. Установите переменные окружения:
   ```bash
   export CAR_ENCRYPTION_KEY="your_encryption_key"
   export CAR_ENCRYPTION_SALT="your_encryption_salt"
   ```
3. Настройте подключение к PostgreSQL в `main.cc` (хост, порт, логин, пароль).

## Endpoints
### Управление данными автомобиля
- `POST /car/create`  
  Создание файла с данными (требует JSON с полями: `vin`, `license_plate`, `brand` и др.).
- `GET /car/info`  
  Получение информации об автомобиле.
- `PATCH /car/update`  
  Обновление разрешенных полей (например, `license_plate`, `engine_power`).

### Отчеты
- `GET /daily-reports/{date}`  
  Ежедневный отчет за указанную дату (формат: `YYYY-MM-DD`).
- `GET /period-reports?start_date=...&end_date=...&node_names=...`  
  Отчет за период с фильтрацией по узлам.
- `GET /maintenance-reports`  
  Отчеты по техническому обслуживанию.

### Узлы и сервисы
- `GET /nodes`  
  Список всех узлов.
- `GET /nodes/{node_name}/subnodes`  
  Подузлы для указанного узла.
- `GET /service/remaining-km`  
  Расчет оставшегося пробега до ТО.

## Запуск
```bash
./build/radarserver
```

Сервер запустится на `http://0.0.0.0:8080`.

## Безопасность
- Данные автомобиля шифруются с использованием AES-256-CBC.
- Для генерации ключей используется PBKDF2 с солью.
- Настройки CORS ограничивают источники запросов (см. `config.json`).

## Примечания
- При запуске от root привилегии автоматически понижаются до UID/GID 1000.
- Для работы с D-Bus необходим сервис `setup_ap.service`.

---