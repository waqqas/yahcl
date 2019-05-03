#pragma once

#include "HttpRequest.hpp"

#include <asio.hpp>
#include <functional>
#include <future>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

using asio::ip::tcp;

namespace yahcl {
namespace http {

class HttpClient
{
public:
  using on_response_cb  = std::function<void(const HttpRequest::response_type &)>;
  using on_exception_cb = std::function<void(const std::exception &)>;

private:
  using request_type      = std::shared_ptr<HttpRequest>;
  using request_list_type = std::list<std::tuple<request_type, HttpRequest::request_future, on_response_cb, on_exception_cb>>;

  tcp::resolver                   resolver_;
  tcp::socket                     socket_;
  tcp::resolver::results_type     endpoints_;
  asio::streambuf                 response_;
  request_list_type               request_list_;
  const std::chrono::milliseconds check_timeout_;
  asio::steady_timer              check_timer_;

public:
  explicit HttpClient(asio::io_context &ioc, const std::string &host, const std::string &port)
    : resolver_(ioc)
    , socket_(ioc)
    , check_timeout_(200)
    , check_timer_(ioc, check_timeout_)
  {
    endpoints_ = resolver_.resolve(host, port);
    asio::async_connect(socket_, endpoints_, std::bind(&HttpClient::on_connect, this, std::placeholders::_1));
  }

  void createRequest(const std::string &method, const std::string &path, const on_response_cb on_response, const on_exception_cb on_exception)
  {
    request_type request = std::make_shared<HttpRequest>(socket_, response_, method, path);

    request_list_.push_back(std::make_tuple(std::move(request), request->get_future(), std::move(on_response), std::move(on_exception)));
  }

private:
  void check_promises(const asio::error_code &ec)
  {
    if (!ec)
    {
      for (auto it = request_list_.begin(); it != request_list_.end();)
      {
        auto future = std::move(std::get<1>(*it));

        if (future.valid() && is_ready(future))
        {
          try
          {
            auto response        = future.get();
            auto on_response = std::get<2>(*it);
            on_response(response);
          }
          catch (std::exception &e)
          {
            auto on_exception = std::get<3>(*it);
            on_exception(e);
          }

          // Remove current element from list
          it = request_list_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    check_timer_.expires_at(check_timer_.expiry() + check_timeout_);
    check_timer_.async_wait(std::bind(&HttpClient::check_promises, this, std::placeholders::_1));
  }
  void on_connect(const asio::error_code &ec)
  {
    if (!ec)
    {
      check_timer_.async_wait(std::bind(&HttpClient::check_promises, this, std::placeholders::_1));
    }
  }
};
}  // namespace http
}  // namespace yahcl