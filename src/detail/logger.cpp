#include "logger.h"

#include <iostream>
#include <string_view>

namespace logger {

    namespace {
        namespace logging = boost::log;
        namespace keywords = boost::log::keywords;
        namespace expr = boost::log::expressions;

        BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime);
        BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::object);

        void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
            json::object log_obj;

            // timestamp
            auto ts = rec[timestamp];
            if (ts) {
                std::ostringstream ss;
                ss << to_iso_extended_string(ts.get());
                log_obj["timestamp"] = ss.str();
            }

            // data
            auto data = rec[additional_data];
            if (data) {
                log_obj["data"] = data.get();
            }

            if (auto msg = rec[expr::smessage]) {
                log_obj["message"] = msg.get();
            }

            strm << logger::json::serialize(log_obj) << '\n';
        }
    } // namespace

    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;

    void InitLogging() {
        logging::add_common_attributes();

        logging::add_console_log( 
            std::cout,
            keywords::format = &MyFormatter,
            keywords::auto_flush = true
        );
    }
 
    void LogServerStart(const int port, std::string_view address) {
        json::object data;
        data["port"] = port;
        data["address"] = std::string(address);
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data.get_name(), data)
                                << "server started";
    }

    void LogServerStop(const int code, std::string_view exception_text) {
        json::object data;
        data["code"] = code;
        if (!exception_text.empty()) {
            data["exception"] = std::string(exception_text);
        }

        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data.get_name(), data)
                                << "server exited";
    }

    void LogNetworkError(const int code, std::string_view text, std::string_view where) {
        json::object data;
        data["code"] = code;
        data["text"] = std::string(text);
        data["where"] = std::string(where);
        BOOST_LOG_TRIVIAL(error) << boost::log::add_value(additional_data.get_name(), data)
                                 << "error";
    }

    void LogRequest(std::string_view ip, std::string_view URI, std::string_view method) {
        json::object data;
        data["ip"] = std::string(ip);
        data["URI"] = std::string(URI);
        data["method"] = std::string(method);
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data.get_name(), data)
                                << "request received";
    }

    void LogResponse(std::string_view ip, const int time, const int code, std::string_view content_type) {
        json::object data;
        data["ip"] = std::string(ip);
        data["response_time"] = time;
        data["code"] = code;

        if (!content_type.empty()) {
            data["content_type"] = std::string(content_type);
        }
        else {
            data["content_type"] = nullptr;
        }

        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data.get_name(), data)
                                << "response sent";
    }

} // namespace logger