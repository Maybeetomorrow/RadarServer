#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

class NodeController : public HttpController<NodeController> {
public:
    explicit NodeController(const DbClientPtr& dbClient) : dbClient_(dbClient) {}

    static const bool isAutoCreation = false;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(NodeController::getAllNodes, "/nodes", Get);
    ADD_METHOD_TO(NodeController::getSubnodesByNodeName, "/nodes/{node_name}/subnodes", Get);
    METHOD_LIST_END

    void getAllNodes(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    );
    void getSubnodesByNodeName(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& node_name
    );

private:
    DbClientPtr dbClient_;
};