#include "FTPClient.hpp"

#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <utility>

using boost::asio::placeholders::error;

FTPClient::FTPClient(boost::shared_ptr<boost::asio::io_service> io_service)
	: socket(*io_service), service(io_service), uploader(io_service), handler(io_service) {
}

void FTPClient::connect(boost::asio::ip::tcp::endpoint &ep, std::string userName, std::string password) {
	socket.async_connect(ep, boost::bind(&FTPClient::connected, this, error));

	this->userName = userName;
	this->password = password;
	pendingCommands = { USER, PASS, TYPE };
}

void FTPClient::connected(const boost::system::error_code &error) {
	if (error) {
		return;
	}

	readAsync();
}

void FTPClient::changeWorkingDirectory(std::string dirName) {
	this->currentDirectoryName = dirName;
	pendingCommands.push_back(CWD);
}

void FTPClient::uploadFile(std::string uploadFileName) {
	pendingCommands.push_back(PASV);

	uploader.setFileName(uploadFileName);
}

void FTPClient::dataReceived(const boost::system::error_code &error, std::size_t bytes) {
	if (error) {
		return;
	}
	readAsync();

	std::string strToParse(rxBuffer.begin(), rxBuffer.begin() + bytes);

	boost::regex pattern("([0-9]+) (.+)");
	boost::smatch results;
	bool matched = boost::regex_match(strToParse, results, pattern);
	if (matched && !writtenCommands.empty()) {
		int code = boost::lexical_cast<int>(results[1]);
		std::pair<int, std::string> CodeMsg = std::make_pair(code, results[2]);

		switch (writtenCommands.top()) {
		case CWD: {
			if (CodeMsg.first != 250) {
				handler << CodeMsg;
			}
			break;
		}
		case PWD: {
			if (CodeMsg.first != 257) {
				handler << CodeMsg;
			}
			break;
		}
		case TYPE: {
			if (CodeMsg.first != 200) {
				handler << CodeMsg;
			}
			break;
		}
		case STOR: {
			if (CodeMsg.first != 150 && CodeMsg.first != 226) {
				handler << CodeMsg;
			}
			break;
		}
		case PASV: {
			if (CodeMsg.first != 227) {
				handler << CodeMsg;
			}
			else if (CodeMsg.first == 227) {
				boost::regex addrPattern("[a-zA-Z ]*\\(([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)\\).*");
				boost::smatch fullAdress;
				boost::regex_match(CodeMsg.second, fullAdress, addrPattern);
				passiveAddr = fullAdress[1] + '.' + fullAdress[2] + '.' + fullAdress[3] + '.' + fullAdress[4];
				passivePort = std::to_string(boost::lexical_cast<int>(fullAdress[5]) * 256 + boost::lexical_cast<int>(fullAdress[6]));
				
				createPassiveConnection();
			}
			break;
		}
		case USER: {
			if (CodeMsg.first != 331) {
				handler << CodeMsg;
			}
			break;
		}
		case PASS: {
			if (CodeMsg.first != 230) {
				handler << CodeMsg;
			}
			break;
		}
		default:
			break;
		}

	}

	std::string data(rxBuffer.begin(), rxBuffer.begin() + bytes);												//RM
	std::cout << " -> " << data << std::endl;

	processCommand();
}

void FTPClient::dataWritten(const boost::system::error_code &error) {
	if (error) {
		return;
	}
}

void FTPClient::readAsync() {
	socket.async_read_some(
		boost::asio::buffer(rxBuffer, sizeof(rxBuffer)),
		boost::bind(&FTPClient::dataReceived, this, error, boost::asio::placeholders::bytes_transferred)
	);

}

void FTPClient::writeAsync() {
	socket.async_send(boost::asio::buffer(wxBuffer.data(), wxBuffer.size()),
		boost::bind(&FTPClient::dataWritten, this, error));

}

void FTPClient::processCommand() {
	if (pendingCommands.empty()) {
		return;
	}
	std::string resultCommand = " ";
	memset(&wxBuffer, 0, 512);

	switch (pendingCommands.front()) {
		/*	case DELE:
				break;
			case MKD:
				break;
			case QUIT:
				break;
			case LIST:
				break;
			case RETR:
				break;
			case ABOR:
				break;
			case RNFR:
				break;
			case RNTO:
				break;
			case PORT:
				break;
			case RMD:
				break;*/
	case CWD: {
		resultCommand = "CWD " + currentDirectoryName;
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(CWD);
		break;
	}
	case PWD: {
		resultCommand = "PWD";
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(PWD);
		break;
	}
	case TYPE: {
		resultCommand = "TYPE I";
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(TYPE);
		break;
	}
	case STOR: {
		resultCommand = "STOR " + uploader.getFileName();
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(STOR);
		break;
	}

	case PASV: {
		resultCommand = "PASV";
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(PASV);
		break;
	}
	case USER: {
		resultCommand = "USER " + userName;
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(USER);
		break;
	}
	case PASS: {
		resultCommand = "PASS " + password;
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(PASS);
		break;
	}
	default:
		break;
	}

	std::string data(wxBuffer.begin(), wxBuffer.begin() + 20);												//RM
	std::cout << " <- " << data << std::endl;

	pendingCommands.pop_front();
	writeAsync();
}

void FTPClient::createPassiveConnection() {
	pendingCommands.push_back(STOR);

	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(passiveAddr), std::stoi(passivePort));
	uploader.loadFileTo(ep);
}

FTPClient::~FTPClient() {
	if (socket.is_open())
		socket.close();
}

FTPClient::FileUploader::FileUploader(boost::shared_ptr<boost::asio::io_service> service)
	:socket(*service) {
}

void FTPClient::FileUploader::setFileName(std::string & fileName) {
	this->fileName = fileName;
}

std::string FTPClient::FileUploader::getFileName() const {
	return fileName;
}

void FTPClient::FileUploader::loadFileTo(boost::asio::ip::tcp::endpoint ep) {
	socket.async_connect(ep, boost::bind(&FTPClient::FileUploader::connected, this, error));
}

void FTPClient::FileUploader::connected(const boost::system::error_code & error) {
	if (error) {
		return;
	}

	std::ifstream is(fileName.c_str(), std::ios::in | std::ios::binary);
	char buf[512];

	while (is.read(buf, sizeof(buf)).gcount() > 0)
		reply.append(buf, is.gcount());

	socket.async_send(boost::asio::buffer(reply, reply.size()),
		boost::bind(&FTPClient::FileUploader::fileUploaded, this, error));
}

void FTPClient::FileUploader::fileUploaded(const boost::system::error_code & error) {
	if (error) {
		return;
	}												

	socket.close();
}


void FTPClient::ErrorHandler::operator<<(std::pair<int,std::string> CodeMsg) {
	switch (CodeMsg.first) {
	case 100: {

		boost::asio::deadline_timer timer(*service, boost::posix_time::seconds(5));
		timer.async_wait(boost::bind(&FTPClient::ErrorHandler::timerExpired, this, error));
		break;
	}
	case 110: {
		//TODO
		break;
	}
	case 120: {
		boost::regex pattern("[a-zA-Z ]*([0-9]+)[a-zA-Z ]*");
		boost::smatch minutes;
		boost::regex_match(CodeMsg.second, minutes, pattern);

		boost::asio::deadline_timer timer(*service, boost::posix_time::minutes(boost::lexical_cast<int>(minutes[1])));
		timer.async_wait(boost::bind(&FTPClient::ErrorHandler::timerExpired, this, error));
		break;
	}
	case 332: {
		std::cout << "Need account for login." << std::endl;
	}
	case 350: {
		std::cout << "Requested file action pending further information" << std::endl;
	}
	case 421: {
		std::cout << "Service not available, closing control connection." << std::endl;
	}
	case 425: {
		std::cout << "Can't open data connection." << std::endl;
	}
	case 426: {
		std::cout << "Connection closed; transfer aborted." << std::endl;
	}
	case 430: {
		std::cout << "Invalid username or password" << std::endl;
	}
	case 434: {
		std::cout << "Requested host unavailable." << std::endl;
	}
	case 450: {
		std::cout << "Requested file action not taken." << std::endl;
	}
	case 451: {
		std::cout << "Requested action aborted. Local error in processing." << std::endl;
	}
	case 452: {
		std::cout << "	Requested action not taken. Insufficient storage space in system.File unavailable." << std::endl;
	}
	case 501: {
		std::cout << "Syntax error in parameters or arguments." << std::endl;
	}
	case 502: {
		std::cout << "Command not implemented." << std::endl;
	}
	case 503: {
		std::cout << "Bad sequence of commands." <<std::endl;
	}
	case 504: {
		std::cout << "Command not implemented for that parameter." << std::endl;
	}
	case 530: {
		std::cout << "Not logged in." << std::endl;
	}
	case 532: {
		std::cout << "Need account for storing files." << std::endl;
	}
	case 534: {
		std::cout << "Could Not Connect to Server - Policy Requires SSL" << std::endl;
	}
	case 550: {
		std::cout << "Requested action not taken. File unavailable (e.g., file not found, no access)." <<std::endl;
	}
	case 551: {
		std::cout << "Requested action aborted. Page type unknown." << std::endl;
	}
	case 552: {
		std::cout << "Requested file action aborted. Exceeded storage allocation (for current directory or dataset)." << std::endl;
	}
	case 553: {
		std::cout << "Requested action not taken. File name not allowed." << std::endl;
	}
	case 10054: {
		std::cout << "Connection reset by peer. The connection was forcibly closed by the remote host." << std::endl;
	}
	case 10060: {
		std::cout << "Cannot connect to remote server." << std::endl;
	}
	case 10061: {
		std::cout << "Cannot connect to remote server. The connection is actively refused by the server." << std::endl;
	}
	case 10066: {
		std::cout << "Directory not empty." << std::endl;
	}
	case 10068: {
		std::cout << "Too many users, server is full." << std::endl;
	}
	default:
		break;
	}
}

void FTPClient::ErrorHandler::timerExpired(const boost::system::error_code & error){
	if (error) {
		return;
	}
}
