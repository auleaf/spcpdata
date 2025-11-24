#ifndef MYSQL_WRAPPER_H
#define MYSQL_WRAPPER_H

#include <string>
#include <vector>
#include <map>

#include <mysql.h>


/**
 * MySQL数据库封装类
 * 提供数据库连接、增删改查等基本操作
 */
class MySQLWrapper {
public:
    /**
     * 构造函数
     */
    MySQLWrapper();
    
    /**
     * 析构函数
     */
    ~MySQLWrapper();
    
    /**
     * 连接数据库
     * @param host 数据库主机地址
     * @param user 用户名
     * @param password 密码
     * @param database 数据库名
     * @param port 端口号
     * @return 是否连接成功
     */
    bool connect(const std::string& host, const std::string& user, const std::string& password, 
                 const std::string& database, unsigned int port = 3306);
    
    /**
     * 断开数据库连接
     */
    void disconnect();
    
    /**
     * 执行SQL查询（SELECT语句）
     * @param sql SQL查询语句
     * @return 查询结果，每行数据以map形式存储
     */
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);
    
    /**
     * 执行SQL语句（INSERT、UPDATE、DELETE等）
     * @param sql SQL语句
     * @return 受影响的行数，失败返回-1
     */
    int execute(const std::string& sql);
    
    /**
     * 获取最后一次插入的ID
     * @return 自增ID
     */
    unsigned long long getLastInsertId();
    
    /**
     * 获取当前连接状态
     * @return 是否已连接
     */
    bool isConnected() const;
    
    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    std::string getLastError() const;
    
    /**
     * 转义字符串，防止SQL注入
     * @param str 需要转义的字符串
     * @return 转义后的字符串
     */
    std::string escapeString(const std::string& str);
    
private:
    MYSQL* mysql_;         // MySQL连接句柄
    std::string last_error_; // 最后一次错误信息
    bool connected_;       // 连接状态
};

#endif // MYSQL_WRAPPER_H
