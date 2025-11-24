#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <future>
#include <chrono>
#include <memory>

// 第三方库头文件
#include "json.hpp"
using json = nlohmann::json;

#include "httplib.h"
using namespace httplib;

// MySQL封装类
#include "mysql_wrapper.h"

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
    
    // MySQL连接
    std::unique_ptr<MySQLWrapper> mysql;
    if (cfg.contains("mysql")) {
        auto& mysql_cfg = cfg["mysql"];
        std::string mysql_host = mysql_cfg.value("host", "127.0.0.1");
        std::string mysql_user = mysql_cfg.value("user", "root");
        std::string mysql_pass = mysql_cfg.value("password", "");
        std::string mysql_db = mysql_cfg.value("database", "test");
        int mysql_port = mysql_cfg.value("port", 3306);
        
        mysql = std::make_unique<MySQLWrapper>();
        if (mysql->connect(mysql_host, mysql_user, mysql_pass, mysql_db, mysql_port)) {
            std::cout << "MySQL connected successfully" << std::endl;
            
            // 尝试创建测试表
            std::string create_table_sql = R"(
                CREATE TABLE IF NOT EXISTS users (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100) NOT NULL,
                    email VARCHAR(100) NOT NULL,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
            )";
            if (mysql->execute(create_table_sql) < 0) {
                std::cerr << "Failed to create table: " << mysql->getLastError() << std::endl;
            }
        } else {
            std::cerr << "Failed to connect to MySQL: " << mysql->getLastError() << std::endl;
            mysql.reset();
        }
    }

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
    
    // MySQL API endpoints
    
    // 获取用户列表
    svr.Get("/api/users", [&](const Request &req, Response &res){
        if (!mysql) {
            res.status = 503;
            json err = { {"error", "Database not available"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        auto result = mysql->query("SELECT * FROM users");
        if (!mysql->getLastError().empty()) {
            res.status = 500;
            json err = { {"error", mysql->getLastError()} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        json response = json::array();
        for (const auto& row : result) {
            response.push_back(row);
        }
        res.set_content(response.dump(), "application/json");
    });
    
    // 添加用户
    svr.Post("/api/users", [&](const Request &req, Response &res){
        if (!mysql) {
            res.status = 503;
            json err = { {"error", "Database not available"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        try {
            auto j = json::parse(req.body);
            if (!j.contains("name") || !j.contains("email")) {
                res.status = 400;
                json err = { {"error", "Missing required fields: name and email"} };
                res.set_content(err.dump(), "application/json");
                return;
            }
            
            std::string name = j["name"];
            std::string email = j["email"];
            
            // 转义字符串防止SQL注入
            name = mysql->escapeString(name);
            email = mysql->escapeString(email);
            
            std::string sql = "INSERT INTO users (name, email) VALUES ('" + name + "', '" + email + "')";
            int affected_rows = mysql->execute(sql);
            
            if (affected_rows < 0) {
                res.status = 500;
                json err = { {"error", mysql->getLastError()} };
                res.set_content(err.dump(), "application/json");
                return;
            }
            
            unsigned long long insert_id = mysql->getLastInsertId();
            json response = {
                {"success", true},
                {"message", "User created successfully"},
                {"user_id", insert_id},
                {"affected_rows", affected_rows}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 400;
            json err = { {"error", "Invalid JSON"}, {"message", e.what()} };
            res.set_content(err.dump(), "application/json");
        }
    });
    
    // 更新用户
    svr.Put("/api/users/:id", [&](const Request &req, Response &res){
        if (!mysql) {
            res.status = 503;
            json err = { {"error", "Database not available"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        try {
            std::string id = req.path_params.at("id");
            auto j = json::parse(req.body);
            
            std::string updates;
            if (j.contains("name")) {
                updates += "name = '" + mysql->escapeString(j["name"]) + "'";
            }
            if (j.contains("email")) {
                if (!updates.empty()) updates += ", ";
                updates += "email = '" + mysql->escapeString(j["email"]) + "'";
            }
            
            if (updates.empty()) {
                res.status = 400;
                json err = { {"error", "No fields to update"} };
                res.set_content(err.dump(), "application/json");
                return;
            }
            
            std::string sql = "UPDATE users SET " + updates + " WHERE id = " + mysql->escapeString(id);
            int affected_rows = mysql->execute(sql);
            
            if (affected_rows < 0) {
                res.status = 500;
                json err = { {"error", mysql->getLastError()} };
                res.set_content(err.dump(), "application/json");
                return;
            }
            
            json response = {
                {"success", true},
                {"message", "User updated successfully"},
                {"affected_rows", affected_rows}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 400;
            json err = { {"error", "Invalid JSON"}, {"message", e.what()} };
            res.set_content(err.dump(), "application/json");
        }
    });
    
    // 删除用户
    svr.Delete("/api/users/:id", [&](const Request &req, Response &res){
        if (!mysql) {
            res.status = 503;
            json err = { {"error", "Database not available"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        std::string id = req.path_params.at("id");
        std::string sql = "DELETE FROM users WHERE id = " + mysql->escapeString(id);
        int affected_rows = mysql->execute(sql);
        
        if (affected_rows < 0) {
            res.status = 500;
            json err = { {"error", mysql->getLastError()} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        json response = {
            {"success", true},
            {"message", "User deleted successfully"},
            {"affected_rows", affected_rows}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    // 获取单个用户
    svr.Get("/api/users/:id", [&](const Request &req, Response &res){
        if (!mysql) {
            res.status = 503;
            json err = { {"error", "Database not available"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        std::string id = req.path_params.at("id");
        std::string sql = "SELECT * FROM users WHERE id = " + mysql->escapeString(id);
        auto result = mysql->query(sql);
        
        if (!mysql->getLastError().empty()) {
            res.status = 500;
            json err = { {"error", mysql->getLastError()} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        if (result.empty()) {
            res.status = 404;
            json err = { {"error", "User not found"} };
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        res.set_content(json(result[0]).dump(), "application/json");
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

    // 关闭MySQL连接
    // MySQL连接会在unique_ptr析构时自动断开
    
    // 关闭Redis连接
#ifdef HAVE_HIREDIS
    if (redis) redisFree(redis);
#endif

    return 0;
}