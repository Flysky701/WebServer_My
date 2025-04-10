#pragma once
#include <vector>
#include <string>
#include <memory>
#include "sqlconnpool.hpp"

struct FileMeta{
    int id;
    int user_id;
    size_t file_size;
    std::string file_name;
    std::string file_path;
    std::string creatat;
    bool is_del = false;
};

class FileDao
{
    public:
        explicit FileDao(SqlConnPool& pool): pool_(pool){};
        bool CreatFile(const FileMeta &meta);
        bool DeleteFile(int file_id, int user_id);
        std::vector<FileMeta> ListFilesByUser(int user_id, int limit = 100);
        std::shared_ptr<FileMeta> GetFileById(int file_id, int user_id);
        bool CheckOwnership(int file_id, int user_id);

    private:
        SqlConnPool &pool_;
};
 
bool FileDao::CreatFile(const FileMeta &meta){
    SqlGuard conn(pool_);
    conn->setAutoCommit(false);

    try{

        auto update_user = conn->prepareStatement(
            "UPDATE users SET used_quota = used_quota + ? "
            "WHERE id = ? AND (total_quota - used_quota) >= ?");
        update_user->setUInt64(1, meta.file_size);
        update_user->setInt(2, meta.user_id);
        update_user->setUInt64(3, meta.file_size);
        if(update_user -> executeQuery() == 0){
            throw std::runtime_error("存储空间不足或用户不存在");
            return false;
        }

        auto stmt = conn->prepareStatement(
            "INSERT INTO files (filename, user_id, file_size, file_path) "
            "VALUES (?, ?, ?, ?)");

        stmt->setString(1, meta.file_name);
        stmt->setInt(2, meta.user_id);
        stmt->setUInt64(3, meta.file_size);
        stmt->setString(4, meta.file_path);
        stmt->executeUpdate();

        conn->commit();
        return true;
    }
    catch (sql::SQLException &e)
    {
        LOG_ERROR("创建文件失败：" +  std::string(e.what()));
        conn->rollback();
        return false;
    }
}
bool FileDao::DeleteFile(int file_id, int user_id){
    SqlGuard conn(pool_);
    conn->setAutoCommit(false);
    try
    {
        auto get_size = conn->prepareStatement(
            "SELECT file_size FROM files WHERE id = ? AND users_id = ?");

        get_size->setInt(1, file_id);
        get_size->setInt(2, user_id);
        auto res = get_size->executeQuery();
        if(!res -> next()) return false;
        uint64_t size = res->getUInt64("file_size");

        auto update_user = conn->prepareStatement(
            "UPDATE users SET used_quota = used_quota - ? WHERE id = ?"
        );
        update_user->setUInt64(1, size);
        update_user->setInt(2, user_id);
        update_user->executeUpdate();

        auto stmt = conn->prepareStatement("UPDATE files SET is_deleted = TRUE "
                                           "WHERE id = ? AND user_id = ?");
        stmt->setInt(1, file_id);
        stmt->setInt(2, user_id);
        stmt->executeUpdate();

        conn->commit();
        return true;
    }
    catch (sql::SQLException &e){
        LOG_ERROR("删除文件失败：" + std::string(e.what()));
        conn->rollback();
        return false;
    }
}

std::vector<FileMeta> FileDao::ListFilesByUser(int user_id, int limit = 100){
    SqlGuard conn(pool_);
    std::vector<FileMeta> files_;

    try{
        auto stmt = conn->prepareStatement(
            "SELECT id, filename, file_size, file_path, created_at "
            "FROM files WHERE user_id = ? AND is_deleted = FALSE "
            "ORDER BY created_at DESC LIMIT ?"
        );
        stmt->setInt(1, user_id);
        stmt->setInt(2, limit);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while(res -> next()){
            FileMeta tmp;
            tmp.id = res->getInt("id");
            tmp.user_id = res->getInt("user_id");
            tmp.file_size = res->getUInt64("file_size");
            tmp.file_path = res->getString("file_path");
            tmp.creatat = res->getString("created_at");
            tmp.file_name = res->getString("filename");
            files_.push_back(tmp);
        }
    }
    catch (sql::SQLException &e)
    {
        LOG_ERROR("查询用户文件列表失败：" + std::string(e.what()));
        throw;
    }
    return files_;
}
std::shared_ptr<FileMeta> FileDao::GetFileById(int file_id, int user_id){
    SqlGuard conn(pool_);

    try{
        auto stmt = conn->prepareStatement(
            "SELECT id, filename, user_id, file_size, file_path, created_at "
            "FROM files WHERE id = ? AND user_id = ? AND is_deleted = FALSE");

        stmt->setInt(1, file_id);
        stmt->setInt(2, user_id);

        std::unique_ptr<sql::ResultSet> res;
        if(res -> next()){
            auto tmp = std::make_shared<FileMeta>();
            tmp -> id = res->getInt("id");
            tmp -> user_id = res->getInt("user_id");
            tmp -> file_size = res->getUInt64("file_size");
            tmp -> file_path = res->getString("file_path");
            tmp -> creatat = res->getString("created_at");
            tmp -> file_name = res->getString("filename");
            return tmp;
        }
    }
    catch (sql::SQLException &e){
        LOG_ERROR("获取文件失败：" + std::string(e.what()));
        return nullptr;
    }
}
bool FileDao::CheckOwnership(int file_id, int user_id){
    SqlGuard conn(pool_);

    try{
        auto stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM files"
            "WHERE id = ? AND user_id = ? AND is_deleted = FALSE");
        stmt->setInt(1, file_id);
        stmt->setInt(2, user_id);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        return res->next() && res->getInt(1) > 0;
    }
    catch (sql::SQLException &e){
        LOG_ERROR("检查文件失败：" + std::string(e.what()));
        return false;
    }
}
