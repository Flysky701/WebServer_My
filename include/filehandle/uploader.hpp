#pragma once
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <vector>
#include <string.h>
#include <string>


#include "httpresponse.hpp"  
#include "httprequest.hpp"
#include "log.h"

namespace fs = std::filesystem;
static const std::string UPLOAD_RELATIVE_PATH = "/uploads";
static const std::string UPLOAD_PERSISTENT_PATH = "/storage";
static const std::string PROJECT_ROOT_PATH = "/root/work_File/Webserver";

class UpLoader{
    public:
        static void Initialize(){
            fs::path upload_dir = fs::path(PROJECT_ROOT_PATH + UPLOAD_RELATIVE_PATH);
            fs::path storage_dir = fs::path(PROJECT_ROOT_PATH + UPLOAD_PERSISTENT_PATH);

            try{
                if (!fs::exists(upload_dir)){
                    fs::create_directories(upload_dir);
                    LOG_INFO("创建上传目录: {}", upload_dir.string());
                }
                fs::permissions(upload_dir,
                    fs::perms::owner_all |
                    fs::perms::group_read | fs::perms::group_write,
                    fs::perm_options::replace);

                if (!fs::exists(storage_dir)){
                    fs::create_directories(storage_dir);
                    LOG_INFO("创建储存目录: {}", storage_dir.string());
                }
                fs::permissions(storage_dir,
                    fs::perms::owner_all |
                    fs::perms::group_read | fs::perms::group_write,
                    fs::perm_options::replace);
            }
            catch (const std::exception &e){
                LOG_ERROR("创建上传/储存目录失败: {}", e.what());
            }
        } 
        static std::string generate_temp_path() {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::string chars =
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "0123456789";
            static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
            std::string filename(16, '\0');
            for (auto &c : filename) c = chars[dis(gen)];
            return  PROJECT_ROOT_PATH + UPLOAD_RELATIVE_PATH +  "/upload_" + filename;
        }
        static std::string generate_user_path(const int user_id, const std::string filename){
            fs::path save_dir = fs::path(PROJECT_ROOT_PATH + UPLOAD_PERSISTENT_PATH + ("user_" + std::to_string(user_id)));
            if(!fs::exists(save_dir)){
                fs::create_directories(save_dir);
            }
            return (save_dir / filename).string();
        }
        // 目前来说，已经将上传文件存到了 -> temp_path
        // 接下来， 将temp_path 下面的文件转存到固定user下的文件夹 storge/{username}/(files)
        // 返回成功
        static bool StreamUpload(const std::string &temp_path, const std::string &save_path){
            try{
                fs::copy(temp_path, save_path, 
                    fs::copy_options::overwrite_existing);
                LOG_DEBUG("文件转存成功: {} => {}", temp_path, save_path);
                return true;
            }catch (std::exception &e){
                LOG_ERROR("上传发生错误{}", e.what());
                return false;
            }
        }
        static bool MmpUpload(const std::string &temp_path, const std::string &save_path){
            constexpr size_t BUFFER_SIZE = 4 * 1024 * 1024;
            std::ifstream src(temp_path, std::ios::binary);
            std::ofstream dst(save_path, std::ios::binary);

            if (!src || !dst)
                return false;

            std::vector<char> buffer(BUFFER_SIZE);
            while (src){
                src.read(buffer.data(), buffer.size());
                dst.write(buffer.data(), src.gcount());
            }
            return true;
        }
        private:
};