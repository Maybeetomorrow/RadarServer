#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class MaintenanceReportController : public HttpController<MaintenanceReportController> {
public:
    explicit MaintenanceReportController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MaintenanceReportController::getMaintenanceReport, 
            "/maintenance-reports", Get);
    METHOD_LIST_END

    void getMaintenanceReport(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );

private:
    DbClientPtr dbClient_;
};