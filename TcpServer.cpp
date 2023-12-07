#include "TcpServer.hpp"
#include <arpa/inet.h>
#include "ProjLogger.hpp"
#include <string.h>
#include <fcntl.h>

using namespace inet::tcp;

AddrInfo::AddrInfo(std::string_view ipv4, uint16_t port) 
	: _sAddr{ipv4.data(), ipv4.size()}, _port{port}
{
	if (inet_pton(AF_INET, ipv4.data(), &(_addr.sin_addr)) < 0) {
		throw InvalidAddrException();
	}
	_addr.sin_family = AF_INET;
	_addr.sin_port = htons(port);
}

TcpServer::Options::Options(bool _nonBlock) 
	: nonBlock{_nonBlock}
{
	;
}

TcpServer::TcpServer(std::string_view ipv4, uint16_t port, Options&& _opts)
	: serverFd{ socket(AF_INET, SOCK_STREAM | (_opts.nonBlock ? SOCK_NONBLOCK : 0), 0) }, serverSock{std::shared_ptr<ISocket>(new SocketT(serverFd)), 0}, addrInfo(ipv4, port), opts{std::move(_opts)}, socketMapper{}, threadPool{}
{
	if (init() < 0) {
		throw std::runtime_error("Server init error");
	}
	if (run() < 0) {
		throw std::runtime_error("Server run error");
	}
}

int TcpServer::init() {
	Log.debug("Server creating socket");
	if (serverFd < 0) {
		Log.error(std::format("Error while creating socket: {}", strerror(errno)));
		return -1;
	}
	for (size_t i = 0; i < threadPool.size(); ++i) {
		threadPool.getThreadObj(i).setMapper(&socketMapper);
	}
	return 0;
}

int TcpServer::run() {
	int opt = 1;
	serverSock.init();
	if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		Log.error(std::format("Error while setting SO_REUSEADDR to socket {}: {}", serverFd, strerror(errno)));
		serverClose();
		return -1;
	}

	if (!opts.nonBlock) {
		Log.warning("WARNING: only non-blocking operations are now permitted, opts.nonBlocking option has no effect for now");
	}

	//if (opts.nonBlock) {
		if (fcntl(serverFd, F_SETFL, fcntl(serverFd, F_GETFL, 0) | O_NONBLOCK) < 0) {
			Log.error(std::format("Error while setting O_NONBLOCK to socket {}: {}", serverFd, strerror(errno)));
			serverClose();
			return -1;
		}
	//}

	Log.debug(std::format("Server binding socket {} on ip {} and port {}", serverFd, addrInfo.sAddr(), addrInfo.port()));
	const auto& addr = addrInfo.sockAddr();
	if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		Log.error(std::format("Error while binding socket {}: {}", serverFd, strerror(errno)));
		serverClose();
		return -1;
	}

	Log.debug(std::format("Server listening socket {}", serverFd));
	if (listen(serverFd, MAX_LISTENING_CLIENTS) < 0) {
		Log.error(std::format("Error while listening socket {}: {}", serverFd, strerror(errno)));
		serverClose();
		return -1;
	}

	Log.debug("Server creating epoll");
	epollFd = epoll_create1(0);
	if (epollFd < 0) {
		Log.error(std::format("Error while creating epoll: {}", strerror(errno)));
		serverClose();
		return -1;
	}

	Log.debug("Server epoll_ctl");
	struct epoll_event event, events[MAX_EPOLL_EVENTS];
	event.events = EPOLLIN | EPOLLET | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR ;
	event.data.fd = serverFd;
	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) < 0) {
		Log.error(std::format("Error on epoll ctl", strerror(errno)));
		serverClose();
		return -1;
	}

	
	
	//uint32_t ss = 0;
	//uint32_t len = sizeof(ss);
	//int err = getsockopt(serverFd, SOL_SOCKET, SO_RCVBUF, (char*)&ss, &len);

	while (true) {
		int numEvents = epoll_wait(epollFd, events, MAX_EPOLL_EVENTS, -1);
		if (numEvents < 0) {
			Log.error(std::format("Error on epoll {} waiting: {}", epollFd, strerror(errno)));
			serverClose();
			return -1;
		}
		//Log.debug(std::format("Recv {} events", numEvents));
		for (int i = 0; i < numEvents; ++i) {
			if (events[i].data.fd == serverFd) {
				// handle new connections
				auto [errOccured, clientFds] = serverSock.acceptAll();
				if (clientFds.empty() || errOccured) {
					Log.error(serverSock.strerr());
				}
				for (auto errCliendFdPair : clientFds) {
					auto& clientFd = errCliendFdPair.second;
					int fd = clientFd->fd();
					event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
					event.data.fd = fd;
					if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
						Log.error(std::format("Failed to add client socket to epoll instance", strerror(errno)));
						close(fd);
						break;
					}
					Log.debug(std::format("Handling client {}", fd));

					size_t threadIdx = threadPool.getIdx();
					socketMapper.addThreadIdx(fd, clientFd, threadIdx);
				}
			}
			else {
				std::shared_ptr<ISocket> clientSock = nullptr;
				size_t threadIdx = 0;
				if (auto [sock, idx] = socketMapper.findThreadIdx(events[i].data.fd); sock != nullptr) {
					threadIdx = idx;
					clientSock = sock;
					//Log.debug(std::format("Got existing idx {} for fd {}", threadIdx, clientFd));
				}
				else {
					int fd = events[i].data.fd;
					Log.error(std::format("Unknown fd {}, closing connection", fd));
					epoll_ctl(epollFd, EPOLL_CTL_DEL,fd, NULL);
					close(fd);
					continue;
				}
				auto& threadCtx = threadPool.getThreadObj(threadIdx);
				if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
					threadPool.pushTask(threadIdx, std::function([&threadCtx](int epollFd, std::shared_ptr<inet::ISocket> clientSock) { threadCtx.onError(epollFd, clientSock); return 0; }), std::move(epollFd), std::move(clientSock));
					//threadCtx.onError(epollFd, clientSock);

					//Log.error(std::format("Client connection closed {}", clientSock.fd()));
					//epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, NULL);
					//close(clientFd);
					continue;
				}
				else if (events[i].events & EPOLLIN) {
					threadPool.pushTask(threadIdx, std::function([&threadCtx](int epollFd, std::shared_ptr<inet::ISocket> clientSock) { threadCtx.onInputData(epollFd, clientSock); return 0; }), std::move(epollFd), std::move(clientSock));
					//Log.debug(std::format("Queue size is {} for idx {}", threadCtx.queue().size(), threadIdx));
					//threadCtx.onInputData(epollFd, clientSock);
				}
				else if (events[i].events & EPOLLOUT) {
					// continuing to write response, previously stopped on EAGAIN 
					threadPool.pushTask(threadIdx, std::function([&threadCtx](int epollFd, std::shared_ptr<inet::ISocket> clientSock) { threadCtx.onHttpResponse(epollFd, clientSock); return 0; }), std::move(epollFd), std::move(clientSock));
				}
				continue;
			}
		}
		// cooldown sleep to reduce number of small events
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}



	return 0;
}

void TcpServer::serverClose() {
	Log.debug(std::format("Closing server socket {}", serverFd));
	if (serverFd >= 0) close(serverFd);
	if (epollFd >= 0) close(epollFd);
}