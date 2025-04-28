#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class ReportController : public HttpController<ReportController> {
public:
    explicit ReportController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ReportController::getNodeReport, 
            "/reports/{node_name}/{date}", Get);
    METHOD_LIST_END

    void getNodeReport(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& node_name,
        const std::string& date_str
    );

private:
    DbClientPtr dbClient_;
};