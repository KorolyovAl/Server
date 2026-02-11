#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <boost/beast.hpp>
#include <boost/json.hpp>

namespace logger {
    namespace json = boost::json;
    namespace beast = boost::beast;
    namespace http = beast::http; 

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;    

    void InitLogging();
 
    void LogServerStart(const int port, std::string_view address);
    void LogServerStop(const int code, std::string_view exception_text = {});
    void LogNetworkError(const int code, std::string_view text, std::string_view where);
    void LogRequest(std::string_view ip, std::string_view URI, std::string_view method);
    void LogResponse(std::string_view ip, const int time, const int code, std::string_view content_type);

} // namespace logger