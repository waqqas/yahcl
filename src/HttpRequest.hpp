#pragma once

#include "StringUtils.hpp"
#include <algorithm>
#include <asio.hpp>
#include <exception>
#include <sstream>
#include <string>
#include <utility>

using asio::ip::tcp;

namespace yahcl {
namespace http {

template <typename R>
bool is_ready(std::future<R> const &f)
{
  return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

class HttpRequest
{
public:
  using response_type        = std::string;
  using request_promise      = std::promise<response_type>;
  using request_future       = std::future<response_type>;
  using response_header_type = std::unordered_map<std::string, std::string>;

private:
  tcp::socket &        socket_;
  asio::streambuf &    response_;
  asio::streambuf      request_;
  request_promise      promise_;
  response_header_type response_header_;

public:
  HttpRequest(tcp::socket &socket, asio::streambuf &response, const std::string &method, const std::string &path)
    : socket_(socket)
    , response_(response)
  {
    std::ostream request_stream(&request_);
    request_stream << method << " " << path << " HTTP/1.0\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: keep-alive\r\n";
    request_stream << "\r\n";

    asio::async_write(socket_, request_, std::bind(&HttpRequest::on_write, this, std::placeholders::_1));
  }

  request_future get_future()
  {
    return promise_.get_future();
  }

private:
  void on_write(const asio::error_code &ec)
  {
    if (ec)
    {
      promise_.set_exception(std::make_exception_ptr(std::system_error(ec)));
      return;
    }

    // Receive the HTTP response
    asio::async_read_until(socket_, response_, "\r\n", std::bind(&HttpRequest::on_read_status_line, this, std::placeholders::_1));
  }

  void on_read_status_line(const asio::error_code &ec)
  {
    if (!ec)
    {
      // Check that response is OK.
      std::istream response_stream(&response_);
      std::string  http_version;
      response_stream >> http_version;
      unsigned int status_code;
      response_stream >> status_code;
      std::string status_message;
      std::getline(response_stream, status_message);
      if (!response_stream || http_version.substr(0, 5) != "HTTP/" || status_code != 200)
      {
        promise_.set_exception(std::make_exception_ptr(std::runtime_error("Invalid response line")));
        return;
      }

      // Read the response headers, which are terminated by a blank line.
      asio::async_read_until(socket_, response_, "\r\n\r\n", std::bind(&HttpRequest::on_read_headers, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
      promise_.set_exception(std::make_exception_ptr(std::system_error(ec)));
    }
  }

  void on_read_headers(const asio::error_code &ec, std::size_t bytes_transferred)
  {
    if (!ec)
    {
      std::istream response_stream(&response_);
      // Get content length
      std::string            header;
      std::string::size_type index;
      while (std::getline(response_stream, header) && header != "\r")
      {
        index = header.find(':', 0);
        if (index != std::string::npos)
        {
          std::string header_name = header.substr(0, index);
          yahcl::utils::trim(header_name);
          std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
          response_header_.insert(std::make_pair(header_name, yahcl::utils::trim_copy(header.substr(index + 1))));
        }
      }

      auto content_length_header = response_header_.find("content-length");
      if (content_length_header != response_header_.end())
      {
        std::size_t content_length = std::stoul(content_length_header->second);
        asio::async_read(socket_, response_, asio::transfer_exactly(content_length), std::bind(&HttpRequest::on_read_content, this, std::placeholders::_1, std::placeholders::_2));
      }
      else
      {
        promise_.set_value("");
      }

      // response_.consume(bytes_transferred);
      // asio::async_read(socket_, response_, asio::transfer_exactly(16), std::bind(&HttpRequest::on_read_content, this, std::placeholders::_1));
    }
    else
    {
      promise_.set_exception(std::make_exception_ptr(std::system_error(ec)));
    }
  }

  void on_read_content(const asio::error_code &ec, std::size_t bytes_transferred)
  {
    if (!ec)
    {
      try
      {
        std::ostringstream response_stream(&response_);
        promise_.set_value(response_stream.str());
      }
      catch (std::exception &e)
      {
        promise_.set_exception(std::current_exception());
      }
    }
    else
    {
      promise_.set_exception(std::make_exception_ptr(std::system_error(ec)));
    }
  }
};
}  // namespace http
}  // namespace yahcl
