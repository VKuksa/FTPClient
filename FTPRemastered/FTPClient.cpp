#include "FtpClient.hpp"

#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <utility>

using boost::asio::placeholders::error;

namespace adpm { namespace agent {
		FtpClient::FtpClient(boost::asio::io_service &io_service)
			: socket(io_service), service(io_service), uploader(io_service), handler(io_service), resolver(io_service) {
		}

		void FtpClient::connect(const std::string &address, const std::string &userName, const std::string &password) {
			boost::asio::ip::tcp::resolver::query query(address, "21");

			resolver.async_resolve(query, boost::bind(&FtpClient::addressResolved, this, error, boost::asio::placeholders::iterator));

			pendingCommands.push_back(std::make_pair(USER, userName));
			pendingCommands.push_back(std::make_pair(PASS, password));
			pendingCommands.push_back(std::make_pair(TYPE, "I"));
			current = USER;
		}

		void FtpClient::addressResolved(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator iterator) {
			if (!error) {
				socket.async_connect(*iterator, boost::bind(&FtpClient::connected, this, error));
			}
		}

		void FtpClient::connected(const boost::system::error_code &error) {
			if (!error) {
				readAsync();
			}
		}

		void FtpClient::changeWorkingDirectory(const std::string &dirName) {
			pendingCommands.push_back(std::make_pair(CWD, dirName));
		}

		void FtpClient::uploadFile(const std::string &uploadFileName) {
			pendingCommands.push_back(std::make_pair(PASV, ""));

			uploader.setFileName(uploadFileName);
		}

		void FtpClient::dataReceived(const boost::system::error_code &error, std::size_t bytes) {
			if (!error) {
				readAsync();

				std::string strToParse(rxBuffer.begin(), rxBuffer.begin() + bytes);

				boost::regex pattern("([0-9]+) (.+)");
				boost::smatch results;
				bool matched = boost::regex_match(strToParse, results, pattern);
				if (matched) {
					int code = boost::lexical_cast<int>(results[1]);
					std::pair<int, std::string> CodeMsg = std::make_pair(code, results[2]);

					switch (current) {
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
						else {
							boost::regex addrPattern("[a-zA-Z ]*\\(([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)\\).*");
							boost::smatch fullAdress;
							boost::regex_match(CodeMsg.second, fullAdress, addrPattern);
							std::string passiveAddr = fullAdress[1] + '.' + fullAdress[2] + '.' + fullAdress[3] + '.' + fullAdress[4];
							std::string passivePort = std::to_string(boost::lexical_cast<int>(fullAdress[5]) * 256 + boost::lexical_cast<int>(fullAdress[6]));

							createPassiveConnection(passiveAddr, passivePort);
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
						pendingCommands.clear();
						socket.shutdown(socket.shutdown_both);
						socket.close();
					}

				}

				std::string data(rxBuffer.begin(), rxBuffer.begin() + bytes);												//RM
				std::cout << " -> " << data << std::endl;

				processCommand();
			}
		}

		void FtpClient::dataWritten(const boost::system::error_code &error) {
			if (error) {
				return;
			}
		}

		void FtpClient::readAsync() {
			socket.async_read_some(
				boost::asio::buffer(rxBuffer, sizeof(rxBuffer)),
				boost::bind(&FtpClient::dataReceived, this, error, boost::asio::placeholders::bytes_transferred)
			);
		}

		void FtpClient::writeAsync() {
			socket.async_send(boost::asio::buffer(wxBuffer.data(), wxBuffer.size()),
				boost::bind(&FtpClient::dataWritten, this, error));
		}

		void FtpClient::processCommand() {
			if (!pendingCommands.empty()) {
				std::string resultCommand = commandToString(pendingCommands.front().first) + " " + pendingCommands.front().second;
				wxBuffer.fill(0);

				switch (pendingCommands.front().first) {
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
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = CWD;
					break;
				}
				case PWD: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = PWD;
					break;
				}
				case TYPE: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = TYPE;
					break;
				}
				case STOR: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = STOR;
					break;
				}

				case PASV: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = PASV;
					break;
				}
				case USER: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = USER;
					break;
				}
				case PASS: {
					std::copy(resultCommand.begin(), resultCommand.end(), wxBuffer.begin());
					current = PASS;
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
		}

		std::string FtpClient::commandToString(Command cmd)
		{
			switch (cmd) {
			case DELE: return "DELE";
			case RMD: return "RMD";
			case CWD: return "CWD";
			case MKD: return "MKD";
			case PWD: return "PWD";
			case QUIT: return "QUIT";
			case TYPE: return "TYPE";
			case PORT: return "PORT";
			case PASV: return "PASV";
			case LIST: return "LIST";			
			case STOR: return "STOR";
			case USER: return "USER";
			case PASS: return "PASS";
			default:
				std::cerr << "Unknown command" << std::endl;
				pendingCommands.clear();
				socket.shutdown(socket.shutdown_both);
				socket.close();
			}
		}

		void FtpClient::createPassiveConnection(const std::string &passiveAddr, const std::string &passivePort) {
			pendingCommands.push_back(std::make_pair<Command, const std::string>(STOR, uploader.getFileName()));

			boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(passiveAddr), std::stoi(passivePort));
			uploader.loadFileTo(ep);
		}

		FtpClient::~FtpClient() {
			if (socket.is_open())
				socket.close();
		}

		FtpClient::FileUploader::FileUploader(boost::asio::io_service &io_service)
			:socket(io_service) {
		}

		void FtpClient::FileUploader::setFileName(const std::string & fileName) {
			this->fileName = fileName;
		}

		std::string FtpClient::FileUploader::getFileName() const {
			return fileName;
		}

		void FtpClient::FileUploader::loadFileTo(boost::asio::ip::tcp::endpoint ep) {
			socket.async_connect(ep, boost::bind(&FtpClient::FileUploader::connected, this, error));
		}

		void FtpClient::FileUploader::connected(const boost::system::error_code & error) {
			if (!error) {
				std::ifstream is(fileName.c_str(), std::ios::in | std::ios::binary);
				char buf[512];

				while (is.read(buf, sizeof(buf)).gcount() > 0)
					reply.append(buf, is.gcount());

				socket.async_send(boost::asio::buffer(reply, reply.size()),
					boost::bind(&FtpClient::FileUploader::fileUploaded, this, error));
			}
		}

		void FtpClient::FileUploader::fileUploaded(const boost::system::error_code & error) {
			if (!error) {
				socket.close();
			}
		}


		FtpClient::ErrorHandler::ErrorHandler(boost::asio::io_service & io_service) :service(service) {
		}

		void FtpClient::ErrorHandler::operator<<(std::pair<int, std::string> CodeMsg) {
			switch (CodeMsg.first) {
			case 100: {

				boost::asio::deadline_timer timer(*service, boost::posix_time::seconds(5));
				timer.async_wait(boost::bind(&FtpClient::ErrorHandler::timerExpired, this, error));
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
				timer.async_wait(boost::bind(&FtpClient::ErrorHandler::timerExpired, this, error));
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
				std::cout << "Bad sequence of commands." << std::endl;
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
				std::cout << "Requested action not taken. File unavailable (e.g., file not found, no access)." << std::endl;
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

		void FtpClient::ErrorHandler::timerExpired(const boost::system::error_code & error) {
			if (error) {
				std::cout << "Timer haven't worked properly" << std::endl;
				return;
			}
		}
}}
