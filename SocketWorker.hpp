#pragma once
#include "Mt/ThreadPool.hpp"
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include "Socket.hpp"
#include "Http.hpp"
#include "HttpServer.hpp"

class SocketThreadMapper;

class SocketDataHandler {
public:
	using TaskT = std::pair<std::packaged_task<std::any(std::any&)>, std::any>;
	using QueueT = util::mt::SafeQueue<TaskT>;
	using ThreadPoolT = util::mt::RollingThreadPool<SocketDataHandler>;
	SocketDataHandler(QueueT&& tasksQueue, ThreadPoolT* ptp, size_t threadIdx);
	SocketDataHandler(const SocketDataHandler&) = delete;
	SocketDataHandler& operator=(const SocketDataHandler&) = delete;
	SocketDataHandler(SocketDataHandler&& other) noexcept;
	SocketDataHandler& operator=(SocketDataHandler&& other) noexcept;
	~SocketDataHandler();
	void request_stop();
	void join();
	QueueT& queue();
	void setMapper(SocketThreadMapper* _mapper);
	void onInputData(int epollFd, std::shared_ptr<inet::ISocket> sock);
	void onError(int epollFd, std::shared_ptr<inet::ISocket> sock);
	void onHttpResponse(int epollFd, std::shared_ptr<inet::ISocket> clientSock);
	void run();
private:

	struct Connection {
		// limiting max input buffer size to prevent tons of garbage data sent from user
		static constexpr size_t MaxIbufSize = 100 * 1024;

		inet::InputSocketBuffer ibuf;
		size_t lastReadOffset = 0;
		util::web::http::HttpParser<util::web::http::HttpRequest> request;
		size_t bodyStartPos = 0;

		inet::OutputSocketBuffer obuf;
	};

	bool checkInputBufData(std::string_view sv);

	void onCloseClient(int epollFd, std::shared_ptr<inet::ISocket> sock);
	void onHttpRequest(int epollFd, std::shared_ptr<inet::ISocket> clientSock, const util::web::http::HttpRequest& request);
	bool checkFd(std::shared_ptr<inet::ISocket> sock);
	QueueT tasksQueue;
	ThreadPoolT* threadPool = nullptr;
	size_t threadIdx;
	std::jthread thread;
	std::unordered_map<int, Connection> sockConnection;
	//int epollFd;
	std::mutex mtx;
	SocketThreadMapper* mapper = nullptr;

};

class SocketThreadMapper {
public:
	using SockT = std::shared_ptr<inet::ISocket>;
	std::pair<SockT, size_t> findThreadIdx(int fd);
	void addThreadIdx(int fd, SockT sock, size_t threadIdx);
	void removeFd(int fd);
private:
	std::shared_mutex mtx;
	std::unordered_map<int, std::pair<SockT, size_t>> map;
};
