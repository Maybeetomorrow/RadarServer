#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class DailyReportController : public HttpController<DailyReportController> {
public:
    explicit DailyReportController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(DailyReportController::getDailyReport, 
            "/daily-reports/{date}", Get);
    METHOD_LIST_END

    void getDailyReport(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& date_str
    );

private:
    DbClientPtr dbClient_;
};