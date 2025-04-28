#include "daily_report_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <ctime>

using namespace drogon;
using namespace drogon::orm;

void DailyReportController::getDailyReport(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& date_str
) {
    Json::StreamWriterBuilder writer;
    writer.settings_["emitUTF8"] = true;
    writer.settings_["indentation"] = "";

    try {
        // Валидация формата даты
        std::tm tm = {};
        if (!strptime(date_str.c_str(), "%Y-%m-%d", &tm)) {
            throw std::invalid_argument("Неверный формат даты. Используйте YYYY-MM-DD");
        }

        dbClient_->execSqlAsync(
            "SELECT get_daily_report($1::DATE) as report;",
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
                    error["error"] = "Данные за указанную дату отсутствуют";
                    callback(HttpResponse::newHttpJsonResponse(error));
                }
            },
            [callback](const DrogonDbException& e) {
                LOG_ERROR << "Ошибка ежедневного отчета: " << e.base().what();
                Json::Value errorResp;
                errorResp["error"] = "Ошибка генерации отчета";
                auto resp = HttpResponse::newHttpJsonResponse(errorResp);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            date_str
        );
    } catch (const std::exception& e) {
        Json::Value error;
        error["error"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}