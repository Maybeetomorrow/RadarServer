#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class MaintenanceController : public HttpController<MaintenanceController> {
public:
    explicit MaintenanceController(const DbClientPtr& dbClient) 
        : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MaintenanceController::addMaintenance, 
            "/add-maintenance", Post);
    METHOD_LIST_END

    void addMaintenance(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );

private:
    DbClientPtr dbClient_;
};