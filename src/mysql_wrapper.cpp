#include "mysql_wrapper.h"
#include <iostream>
#include <cstring>

MySQLWrapper::MySQLWrapper() {
    mysql_ = nullptr;
    connected_ = false;
    
    // 初始化MySQL连接
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        last_error_ = "Failed to initialize MySQL connection";
    }
}

MySQLWrapper::~MySQLWrapper() {
    disconnect();
    
    // 释放MySQL句柄
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool MySQLWrapper::connect(const std::string& host, const std::string& user, 
                          const std::string& password, const std::string& database, 
                          unsigned int port) {
    // 如果已经连接，先断开
    if (connected_) {
        disconnect();
    }
    
    // 确保mysql_已经初始化
    if (!mysql_) {
        mysql_ = mysql_init(nullptr);
        if (!mysql_) {
            last_error_ = "Failed to initialize MySQL connection";
            return false;
        }
    }
    
    // 设置连接选项
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    // 连接数据库
    if (!mysql_real_connect(mysql_, host.c_str(), user.c_str(), password.c_str(), 
                           database.c_str(), port, nullptr, 0)) {
        last_error_ = mysql_error(mysql_);
        return false;
    }
    
    connected_ = true;
    last_error_.clear();
    return true;
}

void MySQLWrapper::disconnect() {
    if (connected_ && mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        connected_ = false;
    }
}

std::vector<std::map<std::string, std::string>> MySQLWrapper::query(const std::string& sql) {
    std::vector<std::map<std::string, std::string>> result;
    
    if (!connected_ || !mysql_) {
        last_error_ = "Not connected to database";
        return result;
    }
    
    // 执行查询
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        last_error_ = mysql_error(mysql_);
        return result;
    }
    
    // 获取查询结果
    MYSQL_RES* mysql_result = mysql_store_result(mysql_);
    if (!mysql_result) {
        // 如果查询失败或结果为空
        if (mysql_field_count(mysql_) == 0) {
            // 非SELECT语句，没有结果集
            last_error_.clear();
        } else {
            last_error_ = mysql_error(mysql_);
        }
        return result;
    }
    
    // 获取字段信息
    int num_fields = mysql_num_fields(mysql_result);
    MYSQL_FIELD* fields = mysql_fetch_fields(mysql_result);
    
    // 获取数据行
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(mysql_result))) {
        std::map<std::string, std::string> row_data;
        
        // 填充每行数据
        for (int i = 0; i < num_fields; i++) {
            std::string field_name = fields[i].name;
            std::string field_value = row[i] ? row[i] : "";
            row_data[field_name] = field_value;
        }
        
        result.push_back(row_data);
    }
    
    // 释放结果集
    mysql_free_result(mysql_result);
    last_error_.clear();
    return result;
}

int MySQLWrapper::execute(const std::string& sql) {
    if (!connected_ || !mysql_) {
        last_error_ = "Not connected to database";
        return -1;
    }
    
    // 执行SQL语句
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        last_error_ = mysql_error(mysql_);
        return -1;
    }
    
    // 返回受影响的行数
    last_error_.clear();
    return static_cast<int>(mysql_affected_rows(mysql_));
}

unsigned long long MySQLWrapper::getLastInsertId() {
    if (!connected_ || !mysql_) {
        last_error_ = "Not connected to database";
        return 0;
    }
    
    return mysql_insert_id(mysql_);
}

bool MySQLWrapper::isConnected() const {
    return connected_;
}

std::string MySQLWrapper::getLastError() const {
    return last_error_;
}

std::string MySQLWrapper::escapeString(const std::string& str) {
    if (!connected_ || !mysql_) {
        last_error_ = "Not connected to database";
        return str;
    }
    
    // 计算需要的缓冲区大小
    size_t buf_size = str.length() * 2 + 1;
    char* buffer = new char[buf_size];
    
    // 转义字符串
    unsigned long escaped_length = mysql_real_escape_string(mysql_, buffer, str.c_str(), str.length());
    
    std::string result(buffer, escaped_length);
    delete[] buffer;
    
    return result;
}
