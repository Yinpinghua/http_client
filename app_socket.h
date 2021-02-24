#ifndef http_socket_h__
#define http_socket_h__

#include <string>
#include <boost/asio.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <unordered_map>

namespace http = boost::beast::http;

enum class request_mod
{
	get  = 0,
	post = 1,
};

enum class content_type
{
	json = 0,
	form_data = 1,
};

struct Http  {};
struct Https {};

template<typename T>
class app_socket
{
public:
	app_socket();
	~app_socket();
	bool connect(const std::string& host, const std::string& port);
	bool request(const std::string& target, request_mod mod, content_type content,std::string body="");
	int  request_result();
	std::string request_data();
	std::string reason();
	void set_form_data(const std::string& key, const std::string value);
private:
	void close();
	auto& socket();
	bool http_connect(const std::string& host, const std::string& port);
	bool https_connect(const std::string& host, const std::string& port);
	void close_http_socket();
	void close_https_socket();
	std::string create_form_data_body();
private:
	static constexpr bool is_http = 
		std::is_same<T,Http>::value;
	std::string host_;
	boost::asio::io_context ioc_;
	boost::asio::ip::tcp::resolver resolver_;
	boost::asio::ssl::context ctx_;
	boost::beast::ssl_stream<boost::beast::tcp_stream> https_stream_;
	boost::beast::tcp_stream http_stream_;
	boost::optional<http::response<http::string_body>>res_;
	std::unordered_map<std::string, std::string>form_datas_;
};

using http_client  = app_socket<Http>;
using https_client = app_socket<Https>;
#endif // http_socket_h__
