#include "node_controller.h"
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void NodeController::getAllNodes(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
) {
    dbClient_->execSqlAsync(
        "SELECT get_all_nodes_json() as nodes;",
        [callback](const Result& result) mutable {
            if (!result.empty()) {
                Json::StreamWriterBuilder writer;
                writer.settings_["emitUTF8"] = true; // Включаем UTF-8
                writer.settings_["indentation"] = ""; // Убираем отступы

                auto nodesJson = result[0]["nodes"].as<Json::Value>();
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(Json::writeString(writer, nodesJson));
                resp->setContentTypeCodeAndCustomString(
                    CT_APPLICATION_JSON,
                    "application/json; charset=utf-8" // Явно указываем кодировку
                );
                callback(resp);
            } else {
                Json::Value error;
                error["error"] = "Nodes not found";
                callback(HttpResponse::newHttpJsonResponse(error));
            }
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Nodes error: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = "Failed to get nodes";
            auto resp = HttpResponse::newHttpJsonResponse(errorResp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }
    );
}

void NodeController::getSubnodesByNodeName(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& node_name
) {
    // Логирование полученного параметра для отладки
    LOG_DEBUG << "Запрос подузлов для узла: " << node_name;

    Json::StreamWriterBuilder writer;
    writer.settings_["emitUTF8"] = true;
    writer.settings_["indentation"] = "";

    dbClient_->execSqlAsync(
        "SELECT get_subnodes_by_node_name_json($1) as subnodes;",
        [callback, writer](const Result& result) mutable {
            if (!result.empty()) {
                auto subnodesJson = result[0]["subnodes"].as<Json::Value>();
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(Json::writeString(writer, subnodesJson));
                resp->setContentTypeCodeAndCustomString(
                    CT_APPLICATION_JSON,
                    "application/json; charset=utf-8" // Явное указание кодировки
                );
                callback(resp);
            } else {
                Json::Value error;
                error["error"] = "Узел не найден";
                callback(HttpResponse::newHttpJsonResponse(error));
            }
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Ошибка БД: " << e.base().what();
            Json::Value errorResp;
            errorResp["error"] = "Ошибка сервера";
            auto resp = HttpResponse::newHttpJsonResponse(errorResp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        node_name // UTF-8 строка
    );
}