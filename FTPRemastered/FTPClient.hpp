#include <boost/asio.hpp>
#include <deque>
#include <stack>
#include <string>
#include <iostream>

namespace {																					//TODO HANDLER
	
}

class FTPClient
{
	enum WritingCommands {
		DELE,			//Delete file
		RMD,			//Remove directory
		CWD,			//Change directory
		MKD,			//Create directory
		PWD,			//Current directory name
		QUIT,			//Quit
		TYPE,			//Set transaction type
		PORT,			//Go to active state
		PASV,			//Go to passive state
		LIST,			//Get directory content
		RETR,			//Download file
		STOR,			//Upload file
		ABOR,			//Cancel transaction
		RNFR,			//Set file for renaming
		RNTO,			//Rename file
		USER,			//Username
		PASS			//Password
	};

	class ErrorHandler {
	public:
		explicit ErrorHandler(boost::shared_ptr<boost::asio::io_service> service) :service(service){}

		void operator<<(std::pair<int,std::string> CodeMsg);

	private:
		void timerExpired(const boost::system::error_code &error);

		boost::shared_ptr<boost::asio::io_service> service;
	};

	class FileUploader {
	public:
		explicit FileUploader(boost::shared_ptr<boost::asio::io_service> service);

		void setFileName(std::string &fileName);

		std::string getFileName() const;

		void loadFileTo(boost::asio::ip::tcp::endpoint ep);
	private:
		void connected(const boost::system::error_code &error);

		void fileUploaded(const boost::system::error_code &error);

		std::string fileName;
		std::string reply;

		boost::asio::ip::tcp::socket socket;
	};
public:
	explicit FTPClient(boost::shared_ptr<boost::asio::io_service> io_service);
	
	void connect(boost::asio::ip::tcp::endpoint &ep, std::string userName, std::string password);

	void changeWorkingDirectory(std::string dirName);

	void uploadFile(std::string uploadFileName);

	~FTPClient();

private:
	void connected(const boost::system::error_code &error);

	void dataWritten(const boost::system::error_code &error);

	void dataReceived(const boost::system::error_code &error, std::size_t bytes);

	void readAsync();

	void writeAsync();

	void processCommand();

	void createPassiveConnection();

	FileUploader uploader;
	ErrorHandler handler;

	boost::asio::ip::tcp::socket socket;

	boost::shared_ptr<boost::asio::io_service> service;

	std::string currentDirectoryName;
	std::string passiveAddr;
	std::string passivePort;
	std::string userName;
	std::string password;

	std::deque<WritingCommands> pendingCommands;
	std::stack<WritingCommands> writtenCommands;

	std::array<char, 512> wxBuffer;
	std::array<char, 512> rxBuffer;

	friend class ErrorHandler;
};
