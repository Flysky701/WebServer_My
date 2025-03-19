#pragma once

#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>

#include "log.h"

class SqlConnPool;

class SqlGuard
{
public:
    explicit SqlGuard(SqlConnPool &pool) : pool_(&pool), conn_(pool.GetConn()) {};

    SqlGuard(const SqlGuard &) = delete;
    SqlGuard operator=(const SqlGuard &) = delete;

    ~SqlGuard(){
        if (pool_ && conn_)
            pool_->FreeConn(conn_);
    }

    sql::Connection *operator->() const{ return conn_.get();}
    sql::Connection &operator*() const{ return *conn_;}

private:
    SqlConnPool *pool_;
    std::shared_ptr<sql::Connection> conn_;
};

// explicit SqlGua

class SqlConnPool
{
public:
    static SqlConnPool &instance()
    {
        static SqlConnPool instance;
        return instance;
    }
    void init(const std::string &host,
              const std::string &user,
              const std::string &password,
              const std::string &db,
              int port,
              int poolSize);
    std::shared_ptr<sql::Connection> MakeConn();
    std::shared_ptr<sql::Connection> GetConn();
    void FreeConn(std::shared_ptr<sql::Connection> conn);

private:
    SqlConnPool() = default;
    ~SqlConnPool();

    sql::mysql::MySQL_Driver *driver_ = nullptr;

    // 连接队列
    std::queue<std::shared_ptr<sql::Connection>> Conns_;

    // 同步机制
    std::mutex pool_mtx_;
    std::condition_variable cv_;

    // 连接配置参数
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    int port_;
};

SqlConnPool::~SqlConnPool()
{
    std::lock_guard<std::mutex> lock(pool_mtx_);
    while (!Conns_.empty())
    {
        try
        {
            auto conn = Conns_.front();
            if (conn)
                conn->close();
        }
        catch (const sql::SQLException &e)
        {
            LOG_ERROR("数据库连接关闭失败:" + std::string(e.what()));
            throw std::runtime_error("数据库连接关闭失败: " + std::string(e.what()));
        }
    }
}
std::shared_ptr<sql::Connection> SqlConnPool::MakeConn()
{
    auto new_conn = std::shared_ptr<sql::Connection>(driver_->connect(host_ + ":" + std::to_string(port_), user_, password_));
    new_conn->setSchema(database_);
    return new_conn;
}

void SqlConnPool::init(const std::string &host,
                       const std::string &user,
                       const std::string &password,
                       const std::string &db,
                       int port = 8080,
                       int poolSize = 10) : host_(host),
                                            user_(user),
                                            password_(password),
                                            database_(db),
                                            port_(port)
{

    std::lock_guard<std::mutex> lock(pool_mtx_);

    try
    {
        driver_ = sql::mysql::get_mysql_driver_instance();

        for (int i = 0; i < poolSize; ++i)
            Conns_.push(MakeConn());
    }
    catch (const sql::SQLException &e)
    {
        // 重新抛出异常
        LOG_ERROR("数据库连接初始化失败:" + std::string(e.what()));
        throw std::runtime_error("数据库连接初始化失败: " + std::string(e.what()));
    }
}

std::shared_ptr<sql::Connection> SqlConnPool::GetConn()
{

    std::unique_lock<std::mutex> lock(pool_mtx_);
    // 超时
    while (Conns_.empty())
    {
        if (cv_.wait_for(lock, std::chrono::seconds(5)) == std::cv_status::timeout)
        {
            LOG_ERROR("数据库连接超时");
            throw std::runtime_error("数据库连接超时");
        }
    }

    auto conn = Conns_.front();
    Conns_.pop();
    return conn;
}

void SqlConnPool::FreeConn(std::shared_ptr<sql::Connection> conn)
{
    std::lock_guard<std::mutex> lock(pool_mtx_);
    try
    {
        if (conn && conn->isValid() && !conn->isClosed())
        {
            Conns_.push(conn);
        }
        else
        {
            Conns_.push(MakeConn());
        }
    }
    catch (const sql::SQLException &e)
    {
        LOG_ERROR("数据库FreeConn失败");
        throw std::runtime_error("数据库FreeConn失败");
    }
    cv_.notify_one();
}
