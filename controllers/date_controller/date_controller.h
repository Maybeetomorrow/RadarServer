#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class DateController : public HttpController<DateController> {
public:
    explicit DateController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(DateController::getUniqueDates, "/dates", Get);
        ADD_METHOD_TO(DateController::getMaintenanceDates, "/maintenance-dates", Get);
    METHOD_LIST_END

    void getUniqueDates(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );      

    void getMaintenanceDates( 
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );

private:
    DbClientPtr dbClient_;
};