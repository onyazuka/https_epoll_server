#include "HttpServer.hpp"
#include "ProjLogger.hpp"

using namespace util::web::http;

HttpServer::HttpServer() {

}

HttpServer& HttpServer::get() {
	static HttpServer srv{};
	return srv;
}

void HttpServer::registerRoute(const std::string& url, util::web::http::Method method, RouteHandlerT handler) {
	if (url.empty()) {
		throw std::runtime_error("route can't be empty");
	}
	if (auto iter = _routes.find(url); iter != _routes.end()) {
		if (auto iter2 = iter->second.find(method); iter2 != iter->second.end()) {
			throw std::logic_error("route already exists");
		}
	}
	_routes[url][method] = handler;
}

void HttpServer::unregisterRoute(const std::string& url, util::web::http::Method method) {
	if (auto iter = _routes.find(url); iter != _routes.end()) {
		iter->second.erase(method);
		if (iter->second.empty()) {
			_routes.erase(url);
		}
	}
}

util::web::http::HttpResponse HttpServer::callRoute(const std::string& route, const util::web::http::HttpRequest& request) const {
	switch (request.method) {
	case Method::GET:
		return GET(route, request);
	case Method::HEAD:
		return HEAD(route, request);
	case Method::POST:
		return POST(route, request);
	case Method::PUT:
		return PUT(route, request);
	case Method::DELETE:
		return DELETE(route, request);
	case Method::CONNECT:
		return CONNECT(route, request);
	case Method::OPTIONS:
		return OPTIONS(route, request);
	case Method::TRACE:
		return TRACE(route, request);
	case Method::PATCH:
		return PATCH(route, request);
	}
}

util::web::http::HttpResponse HttpServer::_callRoute(const std::string& route, const util::web::http::HttpRequest& request) const {
	for (const auto& _route : _routes) {
		if (_route.first.back() == '*') {
			// wildcard request - checking prefix to match
			std::string_view _routeSv(_route.first);
			_routeSv = _routeSv.substr(0, _routeSv.size() - 1);
			if ((route.find(_routeSv) == 0) && (_routeSv.size() <= route.size())) {
				if (auto iter2 = _route.second.find(request.method); iter2 != _route.second.end()) {
					return iter2->second(request);
				}
			}
		}
		else {
			// exact request - checking for equality
			if (route == _route.first) {
				if (auto iter2 = _route.second.find(request.method); iter2 != _route.second.end()) {
					return iter2->second(request);
				}
			}
		}
	}
	return defaultReponse(404, request);
}

util::web::http::HttpResponse HttpServer::GET(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("GET {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::HEAD(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("HEAD {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::POST(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("POST {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::PUT(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("PUT {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::DELETE(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("DELETE {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::CONNECT(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("CONNECT {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::OPTIONS(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("OPTIONS {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::TRACE(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("TRACE {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::PATCH(const std::string& route, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("PATCH {}", route));
	return _callRoute(route, request);
}

util::web::http::HttpResponse HttpServer::defaultReponse(size_t statusCode, const util::web::http::HttpRequest& request) const {
	Log.info(std::format("Sending default response {}", statusCode));
	HttpHeaders headers;
	util::web::http::HttpResponse response{
		statusCode,
		HttpHeaders()
	};
	return response;
}