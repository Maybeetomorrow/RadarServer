#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class ServiceController : public HttpController<ServiceController> {
public:
    explicit ServiceController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ServiceController::getRemainingServiceKm, 
            "/service/remaining-km", Get);
    METHOD_LIST_END

    void getRemainingServiceKm(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );

private:
    DbClientPtr dbClient_;
};