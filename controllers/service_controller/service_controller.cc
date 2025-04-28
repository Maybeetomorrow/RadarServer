#include "service_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void ServiceController::getRemainingServiceKm(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    Json::StreamWriterBuilder writer;
    writer.settings_["emitUTF8"] = true;
    writer.settings_["indentation"] = "";

    dbClient_->execSqlAsync(
        "SELECT calculate_remaining_service_km() as result;",
        [callback, writer](const Result& result) mutable {
            if (!result.empty()) {
                auto reportJson = result[0]["result"].as<Json::Value>();
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(Json::writeString(writer, reportJson));
                resp->setContentTypeCodeAndCustomString(
                    CT_APPLICATION_JSON,
                    "application/json; charset=utf-8"
                );
                callback(resp);
            } else {
                Json::Value error;
                error["error"] = "Данные о пробеге недоступны";
                callback(HttpResponse::newHttpJsonResponse(error));
            }
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Ошибка запроса пробега: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = "Ошибка сервера при расчете пробега";
            auto resp = HttpResponse::newHttpJsonResponse(errorResp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }
    );
}