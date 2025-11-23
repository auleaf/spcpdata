#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <future>
#include <chrono>

// 第三方库头文件
#include "json.hpp"
using json = nlohmann::json;

#include "httplib.h"
using namespace httplib;

// Redis相关代码
#ifdef HAVE_HIREDIS
#include <hiredis.h>

static redisContext* redis_connect_cfg(const json &cfg) {
    if (!cfg.contains("redis")) return nullptr;
    auto r = cfg["redis"];
    const char *host = r.value("host", "127.0.0.1").c_str();
    int port = r.value("port", 6379);
    struct timeval timeout = {1, 500000};
    redisContext *c = redisConnectWithTimeout(host, port, timeout);
    if (c == nullptr || c->err) {
        if (c) redisFree(c);
        return nullptr;
    }
    return c;
}
#endif

// 配置文件加载函数
static json load_config(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) return {};
    json j;
    try { ifs >> j; } catch (...) { }
    return j;
}

int main(int argc, char **argv) {
    // 配置文件路径 - 支持多种路径，确保能找到配置文件
    std::string cfg_path = "config/config.json";
    if (argc > 1) {
        cfg_path = argv[1];
    } else if (!std::ifstream(cfg_path).good()) {
        // 如果当前目录找不到，尝试上级目录
        cfg_path = "../config/config.json";
    }

    // 加载配置
    auto cfg = load_config(cfg_path);
    std::string host = cfg.value("host", "0.0.0.0");
    int port = cfg.value("port", 8080);

    // 用于存储异步任务的future对象
    std::vector<std::future<void>> async_tasks;

    // Redis连接
#ifdef HAVE_HIREDIS
    redisContext *redis = redis_connect_cfg(cfg);
    if (redis) std::cout << "Redis connected\n";
#endif

    // HTTP服务器
    Server svr;

    // 路由处理
    svr.Get("/ping", [](const Request &req, Response &res){
        res.set_content("pong", "text/plain");
    });

    svr.Get("/config", [&](const Request &req, Response &res){
        res.set_content(cfg.dump(2), "application/json");
    });

    // POST JSON endpoint
    svr.Post("/json", [&](const Request &req, Response &res){
        try {
            auto j = json::parse(req.body);

            // 异步处理
            async_tasks.push_back(std::async(std::launch::async, [j]() {
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } catch (const std::exception &e) {
                    std::cerr << "Async task exception: " << e.what() << std::endl;
                }
            }));

            res.set_content(j.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 400;
            json err = { {"error", "invalid json"}, {"msg", e.what()} };
            res.set_content(err.dump(), "application/json");
        }
    });

    // Echo endpoint
    svr.Post("/echo", [&](const Request &req, Response &res){
        res.set_content(req.body, req.get_header_value("Content-Type").c_str());
    });

    // 启动服务器
    std::cout << "Server listening on " << host << ":" << port << "\n";
    svr.listen(host.c_str(), port);

    // 等待异步任务完成
    for (auto &task : async_tasks) {
        try {
            task.wait();
        } catch (const std::exception &e) {
            std::cerr << "Exception when waiting for async task: " << e.what() << std::endl;
        }
    }
    async_tasks.clear();

    // 关闭Redis连接
#ifdef HAVE_HIREDIS
    if (redis) redisFree(redis);
#endif

    return 0;
}