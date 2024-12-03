#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(custom_message, "CustomMessage", std::string)

namespace http_server {

void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    namespace keywords = boost::log::keywords;
    namespace logging = boost::log;
    namespace json = boost::json;

    json::value entry{
        {"code"s, ec.value()},
        {"text"s, ec.message()},
        {"where"s, std::move(what)}
    };

    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "error"); 
}

void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    using namespace std::literals;
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    stream_.expires_after(30s);
    // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
    http::async_read(stream_, buffer_, request_,
                        // По окончании операции будет вызван метод OnRead
                        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        // Нормальная ситуация - клиент закрыл соединение
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        // Семантика ответа требует закрыть соединение
        return Close();
    }

    // Считываем следующий запрос
    Read();
}

}  // namespace http_server