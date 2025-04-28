#include "date_controller.h"
#include "../../utilities/utilities.h"
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void DateController::getUniqueDates(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    Json::Value response;
    dbClient_->execSqlAsync(
        "SELECT get_unique_dates() as dates;",
        [callback, response](const Result& result) mutable {
            if (!result.empty()) {
                auto datesJson = result[0]["dates"].as<Json::Value>();
                callback(HttpResponse::newHttpJsonResponse(datesJson));
            } else {
                response["error"] = "No dates found";
                callback(HttpResponse::newHttpJsonResponse(response));
            }
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = "Internal server error";
            callback(HttpResponse::newHttpJsonResponse(errorResp));
        }
    );
}
void DateController::getMaintenanceDates(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    Json::Value response;
    dbClient_->execSqlAsync(
        "SELECT get_maintenance_dates() as maintenance_dates;",
        [callback, response](const Result& result) mutable {
            if (!result.empty()) {
                auto datesJson = result[0]["maintenance_dates"].as<Json::Value>();
                callback(HttpResponse::newHttpJsonResponse(datesJson));
            } else {
                response["error"] = "No maintenance dates found";
                callback(HttpResponse::newHttpJsonResponse(response));
            }
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Maintenance dates error: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = "Failed to get maintenance dates";
            auto resp = HttpResponse::newHttpJsonResponse(errorResp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }
    );
}