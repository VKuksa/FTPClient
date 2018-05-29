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

void FTPClient::connect(boost::asio::ip::tcp::endpoint &ep) {
	socket.async_connect(ep, boost::bind(&FTPClient::connected, this, error));

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
		resultCommand = "USER";
		std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
		writtenCommands.push(USER);
		break;
	}
	case PASS: {
		resultCommand = "PASS";
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
		//timer.async_wait(boost::bind(&FtpConnection::timerHandler, _1));
		break;
	}
	case 110: {
		//TODO

	}
	case 120: {
		boost::regex pattern("([0-9]+ )");
		boost::smatch minutes;
		boost::regex_match(CodeMsg.second, minutes, pattern);

		boost::asio::deadline_timer timer(*service, boost::posix_time::minutes(boost::lexical_cast<int>(minutes[1])));
		//timer.async_wait(boost::bind(&FtpConnection::timerHandler, _1));
		break;
	}
	case 332:
	case 350:
	case 421:
	case 425:
	case 426:
	case 430:
	case 434:
	case 450:
	case 451:
	case 452:
	case 501:
	case 502:
	case 503:
	case 504:
	case 530: {
		//pendingCommands.push_back(::USER);
	}
	case 532:
	case 534:
	case 550:
	case 551:
	case 552:
	case 553:
	case 631:
	case 632:
	case 633:
	case 10054:
	case 10060:
	case 10061:
	case 10066:
	case 10068:
	default:
		break;
	}
}
