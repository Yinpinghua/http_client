#include "app_socket.h"

#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

template class app_socket<Http>;
template class app_socket<Https>;

template<typename T>
app_socket<T>::app_socket()
	:resolver_(ioc_)
	, ctx_(boost::asio::ssl::context::tlsv12_client)
	, http_stream_(ioc_)
	, https_stream_(ioc_, ctx_)
{

}

template<typename T>
app_socket<T>::~app_socket()
{
	close();
}

template<typename T>
bool app_socket<T>::connect(const std::string& host, const std::string& port)
{
	if constexpr (is_http) {
		return http_connect(host, port);
	}

	return https_connect(host, port);
}

template<typename T>
bool app_socket<T>::request(const std::string& target, request_mod mod, content_type content, std::string body)
{
	http::request<http::string_body> req;
	if (mod == request_mod::get) {
		req.method(http::verb::get);
	}
	else if (mod == request_mod::post) {
		req.method(http::verb::post);
	}
	else {
		return false;
	}

	req.target(target);
	req.set(http::field::host, host_);
	if (content == content_type::json) {
		req.set(http::field::content_type, "application/json;charset=utf-8");
	}else if (content == content_type::form_data) {
		std::string uuid = create_uuid();
		std::string context_str = "multipart/form-data;boundary=";
		context_str += uuid;
		req.set(http::field::content_type, context_str);
		body = create_form_data_body(uuid);
	}else if (content == content_type::form_urlencoded){
		req.set(http::field::content_type,"application/x-www-form-urlencoded");
		body = create_urlencoded_data_body();
	}

	if (!body.empty()) {
		req.body() = body;
	}

	req.prepare_payload();

	std::string body_test = req.body();

	boost::system::error_code ec;
	http::write(socket(), req, ec);
	if (ec) {
		return false;
	}

	res_.emplace();
	boost::beast::flat_buffer buffer;
	http::read(socket(), buffer, *res_, ec);
	if (ec) {
		return false;
	}

	return true;
}

template<typename T>
int app_socket<T>::request_result()
{
	return res_->base().result_int();
}

template<typename T>
std::string app_socket<T>::request_data()
{
	std::string body = res_->body();
	return std::move(body);
}

template<typename T>
void app_socket<T>::close()
{
	if constexpr (is_http) {
		close_http_socket();
	}
	else {
		close_https_socket();
	}
}

template<typename T>
auto& app_socket<T>::socket()
{
	if constexpr (is_http) {
		return http_stream_;
	}
	else {
		return https_stream_;
	}
}

template<typename T>
bool app_socket<T>::http_connect(const std::string& host, const std::string& port)
{
	boost::system::error_code ec;
	const auto  results = resolver_.resolve(host, port, ec);
	if (ec) {
		return false;
	}

	boost::beast::get_lowest_layer(http_stream_).expires_after(std::chrono::seconds(3));
	boost::beast::get_lowest_layer(http_stream_).connect(results, ec);
	if (ec) {
		return false;
	}

	boost::beast::get_lowest_layer(http_stream_).expires_never();

	host_ = host;
	return true;
}

template<typename T>
bool app_socket<T>::https_connect(const std::string& host, const std::string& port)
{
	boost::system::error_code ec;
	const auto  results = resolver_.resolve(host, port, ec);
	if (ec) {
		return false;
	}

	boost::beast::get_lowest_layer(https_stream_).expires_after(std::chrono::seconds(3));
	boost::beast::get_lowest_layer(https_stream_).connect(results, ec);
	if (ec) {
		return false;
	}

	boost::beast::get_lowest_layer(https_stream_).expires_never();
	boost::beast::get_lowest_layer(https_stream_).expires_after(std::chrono::seconds(10));
	https_stream_.handshake(boost::asio::ssl::stream_base::client, ec);
	if (ec) {
		return false;
	}

	boost::beast::get_lowest_layer(https_stream_).expires_never();
	host_ = host;
	return true;
}

template<typename T>
void app_socket<T>::close_http_socket()
{
	using tcp = boost::asio::ip::tcp;

	boost::system::error_code ec;
	http_stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
}

template<typename T>
void app_socket<T>::close_https_socket()
{
	boost::system::error_code ec;
	https_stream_.shutdown(ec);
}

template<typename T>
std::string app_socket<T>::reason()
{
	return res_->base().reason().to_string();
}

template<typename T>
void app_socket<T>::set_form_data(const std::string& key, const std::string value)
{
	form_datas_.emplace(key, value);
}

template<typename T>
void app_socket<T>::set_form_urlencoded_data(const std::string& key, const std::string value)
{
	form_urlencoded_datas_.emplace(key, value);
}

template<typename T>
std::string app_socket<T>::create_form_data_body(const std::string& uuid)
{
	std::string body;
	std::string CRLF = "\r\n";
	const std::string spilt_str = "--";

	auto iter_begin = form_datas_.begin();
	int count = 1;
	for (;iter_begin != form_datas_.end();++iter_begin) {
		if (count == 1) {
			body.append(spilt_str + uuid + CRLF);
		}

		body.append("Content-Disposition: form-data; name=\"" + iter_begin->first + "\"" + CRLF);
		body.append(CRLF);
		body.append(iter_begin->second + CRLF);
		if (count == form_datas_.size()) {
			body.append(spilt_str + uuid + spilt_str + CRLF);
		}
		else {
			body.append(spilt_str + uuid + CRLF);
		}

		++count;
	}

	return std::move(body);
}

template<typename T>
std::string app_socket<T>::create_urlencoded_data_body()
{
	std::string body;
	int count = 1;
	auto iter_begin = form_urlencoded_datas_.begin();
	for (;iter_begin != form_urlencoded_datas_.end();++iter_begin) {
		body.append(iter_begin->first);
		body.append("=");
		body.append(iter_begin->second);
		if (count < form_urlencoded_datas_.size()){
			body.append("&");
		}
	}

	std::string temp_body = url_encode(body);
	return std::move(temp_body);
}

template<typename T>
std::string app_socket<T>::create_uuid()
{
	std::string str;
	boost::uuids::uuid uuid = boost::uuids::random_generator_mt19937()();

	str = boost::uuids::to_string(uuid);

	return std::move(str);
}

template<typename T>
std::string app_socket<T>::url_encode(const std::string& value)
{
	std::string hex_chars = "0123456789ABCDEF";

	std::string result;
	result.reserve(value.size()); // Minimum size of result

	for (auto& chr : value) {
		if (!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' || chr == '_' || chr == '~'))
			result += std::string("%") + hex_chars[static_cast<unsigned char>(chr) >> 4] + hex_chars[static_cast<unsigned char>(chr) & 15];
		else
			result += chr;
	}

	return std::move(result);
}



