#include <boost/asio.hpp>
#include <deque>
#include <stack>
#include <string>
#include <iostream>

namespace adpm { namespace agent{
		class FtpClient {
	public:
		enum Command {
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

		explicit FtpClient(boost::asio::io_service &io_service);

		void connect(const std::string &address, const std::string &userName, const std::string &password);

		void changeWorkingDirectory(const std::string &dirName);

		void uploadFile(const std::string &uploadFileName);

		~FtpClient();

	private:
		class ErrorHandler {
		public:
			explicit ErrorHandler(boost::asio::io_service &io_service);

			void operator<<(std::pair<int, std::string> CodeMsg);

		private:
			void timerExpired(const boost::system::error_code &error);

			boost::shared_ptr<boost::asio::io_service> service;
		};

		class FileUploader {
		public:
			explicit FileUploader(boost::asio::io_service &io_service);

			void setFileName(const std::string &fileName);

			std::string getFileName() const;

			void loadFileTo(boost::asio::ip::tcp::endpoint ep);
		private:
			void connected(const boost::system::error_code &error);

			void fileUploaded(const boost::system::error_code &error);

			std::string fileName;
			std::string reply;

			boost::asio::ip::tcp::socket socket;
		};
		
		void addressResolved(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator iterator);

		void connected(const boost::system::error_code &error);

		void dataWritten(const boost::system::error_code &error);

		void dataReceived(const boost::system::error_code &error, std::size_t bytes);

		void readAsync();

		void writeAsync();

		void processCommand();

		std::string commandToString(Command cmd);

		void createPassiveConnection(const std::string &passiveAddr, const std::string &passivePort);

		FileUploader uploader;
		ErrorHandler handler;

		boost::asio::ip::tcp::resolver resolver;
		boost::asio::ip::tcp::socket socket;
		boost::asio::io_service &service;

		std::deque<std::pair<Command, const std::string>> pendingCommands;
		Command current;

		std::array<char, 512> wxBuffer;
		std::array<char, 512> rxBuffer;

		friend class ErrorHandler;
	};
}}
