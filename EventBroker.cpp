#include "EventBroker.hpp"

void EventBroker::registerProducerAndHandler(size_t producerId, OnEventCb eventHandler) {
	std::unique_lock<std::shared_mutex> lck(mtx);
	producerIdToEventHandlerMap[producerId] = eventHandler;
}

void EventBroker::unregister(size_t producerId) {
	std::unique_lock<std::shared_mutex> lck(mtx);
	producerIdToEventHandlerMap.erase(producerId);
}

void EventBroker::emitEvent(size_t producerId, std::variant<util::web::http::HttpResponse, std::string> msg) {
	std::shared_lock<std::shared_mutex> lck(mtx);
	if (auto iter = producerIdToEventHandlerMap.find(producerId); iter != producerIdToEventHandlerMap.end()) {
		iter->second(producerId, msg);
	}
}

EventBroker& EventBroker::get() {
	static EventBroker ebr{};
	return ebr;
}

EventBroker::EventBroker() {
	;
}