#include "maintenance_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void MaintenanceController::addMaintenance(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    Json::Value response;
    auto jsonBody = req->getJsonObject();

    if (!jsonBody || !jsonBody->isArray()) {
        response["error"] = "Неверный формат данных. Ожидается массив узлов";
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    dbClient_->execSqlAsync(
        "CALL add_maintenance($1::JSONB);",
        [callback](const Result& result) {
            Json::Value successResp;
            successResp["status"] = "Данные ТО успешно добавлены";
            callback(HttpResponse::newHttpJsonResponse(successResp));
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Ошибка добавления ТО: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(errorResp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        Json::writeString(Json::StreamWriterBuilder(), *jsonBody)
    );
}