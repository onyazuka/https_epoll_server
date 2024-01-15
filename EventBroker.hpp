#pragma once
#include <unordered_map>
#include <thread>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include "Http.hpp"

class EventBroker {
public:
	using OnEventCb = std::function<void(size_t, std::variant<util::web::http::HttpResponse, std::string>)>;
	void registerProducerAndHandler(size_t producerId, OnEventCb eventHandler);
	void unregister(size_t producerId);

	void emitEvent(size_t producerId, std::variant<util::web::http::HttpResponse, std::string> msg);
	static EventBroker& get();
protected:
	EventBroker();
	std::shared_mutex mtx;
	std::unordered_map<size_t, OnEventCb> producerIdToEventHandlerMap;
};