#pragma once
#include <string>
#include <sstream>
#include <unordered_map>



class HttpResponse{
    public:
        HttpResponse() : status_code_(200), keep_alive_(false) {};
        // ~HttpResponse();
        HttpResponse &set_status(int code, const std::string &reason);
        HttpResponse &set_header(const std::string &key, const std::string &value);
        HttpResponse &set_content(const std::string &content, const std::string &mime_type);
        HttpResponse &set_json_content(const std::string &content);
        HttpResponse &set_keep_alive(bool keep_alive);
        std::string serialize() const;

        void SetFile(int fd, size_t size){file_fd_ = fd, file_size_ = size;};
        bool HasFile() const { return file_fd_ != -1; };
        int GetFileFd() const { return file_fd_; };
        size_t GetFileSize() const { return file_size_; };

    private:
        int status_code_;
        std::string status_reason_;
        std::unordered_map<std::string, std::string> headers_;
        std::string content_;
        bool keep_alive_;

        int file_fd_ = -1;
        size_t file_size_ = 0;

        static std::string default_reason(const int code){
            static const std::unordered_map<int, std::string> reasons = {
                {200, "OK"}, {201, "Created"}, {204, "No Content"}, 
                {301, "Moved Permanently"}, {302, "Found"}, 
                {400, "Bad Request"}, {401, "Unauthorized"}, 
                {403, "Forbidden"}, 
                {404, "Not Found"}, {500, "Internal Server Error"}
            };
            return reasons.count(code)? reasons.at(code): "Unknown";
        }
};


HttpResponse &HttpResponse::set_status(int code, const std::string &reason = ""){
    status_code_ = code;
    status_reason_ = reason.empty() ? default_reason(code) : reason;
    return *this;
}
HttpResponse &HttpResponse::set_header(const std::string &key, const std::string &value){
    headers_[key] = value;
    return *this;
}
HttpResponse &HttpResponse::set_content(const std::string &content, const std::string &mime_type = "text/plain"){
    content_ = content;
    return set_header("Content-Type", mime_type).set_header("Content-Length", std::to_string(content.size()));
}
HttpResponse &HttpResponse::set_json_content(const std::string &content){
    return set_content(content, "application/json");
}

HttpResponse &HttpResponse::set_keep_alive(bool keep_alive)
{
    keep_alive_ = keep_alive;
    return set_header("Connection", keep_alive ? "keep-alive" : "close");
}
std::string HttpResponse::serialize() const{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code_ << " " << status_reason_ << "\r\n";
    for (const auto &[key, value] : headers_)
    {
        oss << key << ": " << value << "\r\n";
    }
    oss << "\r\n" << content_;
    return oss.str();
}
