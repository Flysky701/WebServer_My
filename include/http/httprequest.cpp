#include <vector>
#include <string>
#include <algorithm> 
#include <sstream>
#include "httprequest.h"

using std::string;
using std::string_view;
using std::vector;

bool HttpRequest::parse(const char* data, size_t len){
    std::string_view input(data, len);
    size_t pos = 0;

    while(pos < input.size() && state_ != PARSE_ERROR){
        size_t line_end = input.find("\r\n", pos);
        if(line_end == string::npos)break;

        string_view line = input.substr(pos, line_end - pos);
        pos = line_end + 2;


        if(state_ == PARSE_LINE){
            if(parse_request_line(line) == false){
                state_ = PARSE_ERROR;
                return false;
            }
            else state_ == PARSE_HEADERS;
        }
        else if (state_ == PARSE_HEADERS)
        {
            if (line.empty()){
                state_ = PARSE_BODY;
                return true; // 暂时不处理body
            }
            parse_headers(line);
            break;
        }
        else
            break;
    }
}
bool HttpRequest::parse_request_line(string_view line){
    size_t method_end = line.find(' ');
    if(method_end == string_view::npos)
        return  false;
    method_ = line.substr(0, method_end);

    size_t path_start = method_end + 1;
    size_t path_end = line.find(' ', path_start);
    if(path_end == string_view::npos)
        return false;

    size_t version_start = path_end + 1;
    version_ = line.substr(version_start);

    return method_.size() && path_.size() && version_.size();
}

void HttpRequest::parse_headers(string_view line){
    size_t colon = line.find(':');
    if(colon == string_view::npos)
        return;

    string key(line.substr(0, colon));
    transform(key.begin(), key.end(), key.begin(), ::tolower);

    string_view value = line.substr(colon + 1);
    value.remove_prefix(std::min(value.find_first_not_of(" "), value.size()));
    value.remove_suffix(value.size() - std::min(value.find_last_not_of(" \r") + 1, value.size()));

    headers_[key] = value;
}

bool HttpRequest::keep_alive() const
{
    auto it = headers_.find("connection");
    if (it != headers_.end()){
        return it->second == "keep-alive" || (version_ == "HTTP/1.1" && it->second != "close");
    }
    return version_ == "HTTP/1.1";
}
