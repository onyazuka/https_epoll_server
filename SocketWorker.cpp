#include "SocketWorker.hpp"
#include "ProjLogger.hpp"
#include <sys/epoll.h>
#include <string.h>
#include <syncstream>
#include "Http.hpp"

using namespace inet;
using namespace util::web::http;

SocketDataHandler::SocketDataHandler(QueueT&& tasksQueue, ThreadPoolT* ptp, size_t _threadIdx)
	: tasksQueue{ std::move(tasksQueue) }, threadPool{ptp}, threadIdx{_threadIdx}
{
	;
}

SocketDataHandler::SocketDataHandler(SocketDataHandler&& other) noexcept
{
	std::lock_guard<std::mutex> lck(other.mtx);
	tasksQueue = std::move(other.tasksQueue);
	threadPool = other.threadPool;
	threadIdx = other.threadIdx;
	thread = std::move(other.thread);
	sockConnection = std::move(other.sockConnection);
	mapper = other.mapper;
}

SocketDataHandler& SocketDataHandler::operator=(SocketDataHandler&& other) noexcept
{
	std::lock_guard<std::mutex> lck(other.mtx);
	tasksQueue = std::move(other.tasksQueue);
	threadPool = other.threadPool;
	threadIdx = other.threadIdx;
	thread = std::move(other.thread);
	sockConnection = std::move(other.sockConnection);
	mapper = other.mapper;
	return *this;
}

SocketDataHandler::~SocketDataHandler() {
	request_stop();
	join();
}

void SocketDataHandler::request_stop() {
	thread.request_stop();
}

void SocketDataHandler::join() {
	if (thread.joinable()) {
		thread.join();
	}
}

SocketDataHandler::QueueT& SocketDataHandler::queue() {
	return tasksQueue;
}

void SocketDataHandler::setMapper(SocketThreadMapper* _mapper) {
	assert(_mapper);
	mapper = _mapper;
}

void SocketDataHandler::onInputData(int epollFd, std::shared_ptr<ISocket> clientSock) {
	int fd = clientSock->fd();
	if (!checkFd(clientSock)) return;
	//cout << format("Reading from epoll {} and socket {}\n", epollFd, socketFd);
	auto& connection = sockConnection[fd];
	if (!connection.obuf.empty()) {
		Log.warning(std::format("Receiveng request from {}, but response is in process", fd));
		onError(epollFd, clientSock);
		return;
	}
	auto& buf = connection.ibuf;
	Log.debug(std::format("Handling client data {}", fd));
	size_t offset = buf.size();
	ssize_t nbytes = 0;
	if (nbytes = clientSock->read(buf); nbytes <= 0 && nbytes != -EAGAIN) {
		//Log.debug(std::format("Error number {} on {}: {}", nbytes, fd, strerror(errno)));
		Log.error(clientSock->strerr());
		onError(epollFd, clientSock);
		return;
	}
	else if (nbytes == -EAGAIN) {
		Log.debug(std::format("Error number EAGAIN on {}", fd));
		return;
	}
	else {
		Log.debug(std::format("Read {} bytes from {}", nbytes, clientSock->fd()));
	}
	auto bufData = buf.get();
	auto& request = connection.request;
	size_t& bodyStartPos = connection.bodyStartPos;

	if (!request.parsed()) {
		if (bufData.size() < 7) {
			// min data size to check - shortest method "GET" + "\r\n\r\n" == 7, and longest method "OPTIONS" == 7, so it is sufficient to check
			return;
		}
		if (offset == 0) {
			if (!checkInputBufData(std::string_view((char*)bufData.data(), (char*)bufData.data() + 7))) {
				Log.warning(std::format("Invalid non-http data from {}", fd));
				onError(epollFd, clientSock);
				return;
			}
		}
		if (bufData.size() > Connection::MaxIbufSize) {
			Log.warning(std::format("Invalid non-http data from {}: suspicious data of too large size", fd));
			onError(epollFd, clientSock);
			return;
		}
		//Log.debug(std::format("data = {}, offset = {}, size = {}", (void*)bufData.data(), offset, bufData.size()));
		auto bufSv = std::string_view((char*)bufData.data() + offset, (char*)bufData.data() + bufData.size());
		auto pos = bufSv.find("\r\n\r\n");
		if (pos == std::string::npos) {
			Log.warning(std::format("Invalid non-http data from {}", fd));
			onError(epollFd, clientSock);
			return;
		}
		bodyStartPos = offset + pos + 4;
	}
	if (request.parsed() || request.parse(std::string_view((char*)bufData.data(), (char*)bufData.data() + bodyStartPos))) {
		if (std::string sContLen = request.headers().find("CONTENT-LENGTH"); !sContLen.empty()) {
			size_t contentLength = std::stoull(sContLen);
			size_t dataRestLen = bufData.size() - bodyStartPos;
			if (dataRestLen < contentLength) {
				// waiting for the rest data
				return;
			}
			else {
				request.message().body = std::string((char*)bufData.data() + bodyStartPos, (char*)bufData.data() + bodyStartPos + contentLength);
				buf.clear(bodyStartPos + contentLength);
			}
		}
		else {
			// no body - assuming message is received
			buf.clear(bodyStartPos);
		}
	}
	else {
		Log.warning(std::format("Couldn't parse http message from {}", fd));
		onError(epollFd, clientSock);
		return;
	}
	onHttpRequest(epollFd, clientSock, request.message());
	connection.request = util::web::http::HttpParser<util::web::http::HttpRequest>();
}

void SocketDataHandler::onError(int epollFd, std::shared_ptr<ISocket> clientSock) {
	Log.error(std::format("Closing connection from server with client {}", clientSock->fd()));
	onCloseClient(epollFd, clientSock);
}

bool SocketDataHandler::checkInputBufData(std::string_view sv) {
	if (
		sv.size() >= 3 && sv.substr(0, 3) == "GET" ||
		sv.size() >= 4 && sv.substr(0, 4) == "HEAD" ||
		sv.size() >= 4 && sv.substr(0, 4) == "POST" ||
		sv.size() >= 3 && sv.substr(0, 3) == "PUT" ||
		sv.size() >= 6 && sv.substr(0, 6) == "DELETE" ||
		sv.size() >= 7 && sv.substr(0, 7) == "CONNECT" ||
		sv.size() >= 7 && sv.substr(0, 7) == "OPTIONS" ||
		sv.size() >= 5 && sv.substr(0, 5) == "TRACE" ||
		sv.size() >= 5 && sv.substr(0, 5) == "PATCH"
		)
	{
		return true;
	}
	return false;

}

void SocketDataHandler::onCloseClient(int epollFd, std::shared_ptr<ISocket> clientSock) {
	int fd = clientSock->fd();
	if (!checkFd(clientSock)) return;
	epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	mapper->removeFd(clientSock->fd());
	sockConnection.erase(fd);
}

void SocketDataHandler::onHttpRequest(int epollFd, std::shared_ptr<ISocket> clientSock, const util::web::http::HttpRequest& request) {
	int fd = clientSock->fd();
	// no need to check fd, because it is sequential call from onInputData
	auto& connection = sockConnection[fd];
	
	auto response = HttpServer::get().callRoute(request.url, request);

	connection.obuf = OutputSocketBuffer(response.encode());
	onHttpResponse(epollFd, clientSock);
}

void SocketDataHandler::onHttpResponse(int epollFd, std::shared_ptr<inet::ISocket> clientSock) {
	int fd = clientSock->fd();
	if (!checkFd(clientSock)) return;
	auto& connection = sockConnection[fd];
	auto& obuf = connection.obuf;
	if (obuf.empty()) return;
	ssize_t nbytes = clientSock->write(obuf);
	if ((nbytes == 0) || ((nbytes < 0) && (nbytes != -EAGAIN))) {
		// error - closing connection
		Log.error(clientSock->strerr());
		onError(epollFd, clientSock);
		return;
	}
	else if ((nbytes == EAGAIN) || !(obuf.finished())) {
		// recoverable error - will try to send again later
		// NOTE: now outcommented because trying to handle it in EPOLLOUT event
		//threadPool->pushTask(threadIdx, std::function([this](int epollFd, std::shared_ptr<inet::ISocket> clientSock, const util::web::http::HttpRequest& request) { onHttpResponse(epollFd, clientSock, request); return 0; }), std::move(epollFd), std::move(clientSock), request);
		;
	}
	else {
		// ok - written all response
		obuf.clear();
		//onCloseClient(epollFd, clientSock);
	}
	if (nbytes > 0) {
		Log.debug(std::format("Write {} bytes to {}", nbytes, clientSock->fd()));
	}
}

bool SocketDataHandler::checkFd(std::shared_ptr<ISocket> sock) {
	// fd may have been already closed
	if (auto [_sock, threadIdx] = mapper->findThreadIdx(sock->fd()); _sock == nullptr) {
		return false;
	}
	return true;
}

void SocketDataHandler::run() {
	thread = std::move(std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			TaskT task;
			if (tasksQueue.popWaitFor(task, std::chrono::milliseconds(1000))) {
				task.first(task.second);
			}
		}
		}));
}

std::pair<SocketThreadMapper::SockT, size_t> SocketThreadMapper::findThreadIdx(int fd) {
	std::shared_lock<std::shared_mutex> lck(mtx);
	if (auto iter = map.find(fd); iter == map.end()) {
		return { nullptr, 0 };
	}
	else {
		return iter->second;
	}
}
void SocketThreadMapper::addThreadIdx(int fd, SockT sock, size_t threadIdx) {
	std::unique_lock<std::shared_mutex> lck(mtx);
	map[fd] = { sock, threadIdx };
}
void SocketThreadMapper::removeFd(int fd) {
	std::unique_lock<std::shared_mutex> lck(mtx);
	map.erase(fd);
}