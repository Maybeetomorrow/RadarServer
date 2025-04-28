#include "period_report_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <ctime>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>

using namespace drogon;
using namespace drogon::orm;

/// Преобразует вектор строк в строку, представляющую PostgreSQL-массив.
std::string toPgArray(const std::vector<std::string>& vec)
{
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << "\"" << vec[i] << "\"";
        if (i != vec.size() - 1)
            oss << ",";
    }
    oss << "}";
    return oss.str();
}

void PeriodReportController::getPeriodReport(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    Json::StreamWriterBuilder writer;
    writer.settings_["emitUTF8"] = true;
    writer.settings_["indentation"] = "";

    try {
        // Парсинг параметров
        auto params = req->getParameters();

        // Даты периода (обязательные параметры)
        const auto &start_date = params["start_date"];
        const auto &end_date = params["end_date"];

        // Валидация дат
        std::tm tm_start = {}, tm_end = {};
        if (!strptime(start_date.c_str(), "%Y-%m-%d", &tm_start) ||
            !strptime(end_date.c_str(), "%Y-%m-%d", &tm_end))
        {
            throw std::invalid_argument("Неверный формат даты. Используйте YYYY-MM-DD");
        }

        // Обработка параметра node_names (обязательный параметр)
        if (params.find("node_names") == params.end()) {
            throw std::invalid_argument("Параметр node_names отсутствует");
        }
        std::vector<std::string> node_names;
        std::stringstream ss(params["node_names"]);
        std::string item;
        while (std::getline(ss, item, ',')) {
            // Можно добавить удаление лишних пробелов здесь, если необходимо
            node_names.push_back(item);
        }

        if (node_names.empty()) {
            throw std::invalid_argument("Список узлов пуст. Укажите хотя бы один узел.");
        }

        // Преобразуем вектор node_names в строку в формате PostgreSQL-массива
        std::string pgArray = toPgArray(node_names);

        // Выполняем SQL запрос
        dbClient_->execSqlAsync(
            "SELECT get_period_report($1::TEXT[], $2::DATE, $3::DATE) as report;",
            [callback, writer](const Result &result) mutable {
                if (!result.empty()) {
                    auto reportJson = result[0]["report"].as<Json::Value>();
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setBody(Json::writeString(writer, reportJson));
                    resp->setContentTypeCodeAndCustomString(
                        CT_APPLICATION_JSON,
                        "application/json; charset=utf-8"
                    );
                    callback(resp);
                } else {
                    Json::Value error;
                    error["error"] = "Данные за период не найдены";
                    callback(HttpResponse::newHttpJsonResponse(error));
                }
            },
            [callback](const DrogonDbException &e) {
                LOG_ERROR << "Ошибка отчета: " << e.base().what();
                Json::Value errorResp;
                errorResp["error"] = "Ошибка генерации отчета";
                auto resp = HttpResponse::newHttpJsonResponse(errorResp);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            // Передаём параметры: строку pgArray, start_date и end_date
            pgArray,
            start_date,
            end_date
        );
    }
    catch (const std::exception &e) {
        Json::Value error;
        error["error"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}