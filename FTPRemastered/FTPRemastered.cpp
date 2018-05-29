#include "FTPClient.hpp"

#include <boost/asio/ip/address.hpp>

int main()
{
	boost::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("172.18.1.2"), 21);

	FTPClient Client(io_service);
	Client.connect(ep);
	Client.changeWorkingDirectory("data");
	Client.uploadFile("lorem.txt");
	io_service->run();
	
    return 0;
}

