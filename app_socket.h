#ifndef udp_server_h__
#define udp_server_h__

#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include "msg.h"
#include "server_data_proc.h"

class udp_server
{
	using Func = std::function<void(std::vector<char>&&)>;
public:
	udp_server(boost::asio::io_context& io,const hardware_info &info);
	~udp_server();
	bool listen(const uint16_t port);
	void run();
	void send(const char*buff,size_t len);
private:
	void close();
	void do_read();
	void handle_msg(const Cmd_size cmd,std::vector<char>&&bodys);
	void regedit_msg_func(Cmd_size cmd,const Func& func);
	void user_regedit(std::vector<char>&& bodys);
	void user_login(std::vector<char>&& bodys);
private:
	hardware_info info_;
	std::string login_cookie_;
	std::vector<char>recv_buffers_;
	std::vector<char>send_buffers_;
	std::unordered_map<Cmd_size, Func>funcs_;
	boost::asio::io_context& io_server_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint sender_endpoint_;
};
#endif // udp_server_h__
