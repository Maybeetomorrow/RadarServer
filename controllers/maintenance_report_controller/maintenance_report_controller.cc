#include "maintenance_report_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <ctime>

using namespace drogon;
using namespace drogon::orm;

void MaintenanceReportController::getMaintenanceReport(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    Json::StreamWriterBuilder writer;
    writer.settings_["emitUTF8"] = true;
    writer.settings_["indentation"] = "";

    try {
        const auto& params = req->getParameters();
        std::optional<std::string> start_date, end_date;

        // Получение параметров дат
        if (params.find("start_date") != params.end()) {
            start_date = params.at("start_date");
        }
        if (params.find("end_date") != params.end()) {
            end_date = params.at("end_date");
        }

        // Валидация формата дат
        std::tm tm = {};
        if (start_date && !strptime(start_date->c_str(), "%Y-%m-%d", &tm)) {
            throw std::invalid_argument("Неверный формат start_date");
        }
        if (end_date && !strptime(end_date->c_str(), "%Y-%m-%d", &tm)) {
            throw std::invalid_argument("Неверный формат end_date");
        }

        // Подготовка параметров для SQL
        auto start_param = start_date.has_value() ? *start_date : "NULL";
        auto end_param = end_date.has_value() ? *end_date : "NULL";

        dbClient_->execSqlAsync(
            "SELECT generate_maintenance_report("
            "CASE WHEN $1::TEXT = 'NULL' THEN NULL ELSE $1::DATE END, "
            "CASE WHEN $2::TEXT = 'NULL' THEN NULL ELSE $2::DATE END"
            ") as report;",
            [callback, writer](const Result& result) mutable {
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
                    error["error"] = "Данные не найдены";
                    callback(HttpResponse::newHttpJsonResponse(error));
                }
            },
            [callback](const DrogonDbException& e) {
                LOG_ERROR << "Ошибка отчета: " << e.base().what();
                Json::Value errorResp;
                errorResp["error"] = "Ошибка генерации отчета";
                auto resp = HttpResponse::newHttpJsonResponse(errorResp);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            start_param,
            end_param
        );
    } catch (const std::exception& e) {
        Json::Value error;
        error["error"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}