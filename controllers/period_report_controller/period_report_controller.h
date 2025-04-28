#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <vector>

using namespace drogon;
using namespace drogon::orm;

class PeriodReportController : public HttpController<PeriodReportController> {
public:
    explicit PeriodReportController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(PeriodReportController::getPeriodReport, 
            "/period-reports", Get);
    METHOD_LIST_END

    void getPeriodReport(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );

private:
    DbClientPtr dbClient_;
};