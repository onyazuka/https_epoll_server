#include "HttpServer.hpp"
#include "ProjLogger.hpp"
#include <filesystem>
#include <fstream>
#include "Utils_Fs.hpp"

using namespace util::web::http;

static constexpr size_t MaxFileSize = 1 * 1024 * 1024;

static std::unordered_map<std::string, std::string> FileExt2ContentTypeMap{
	{".js", "application/javascript"},
	{".css", "text/css"},
	{".html", "text/html; charset=utf-8"}
};

HttpServer::HttpServer() {
	registerRoute("/", Method::GET, [this](const util::web::http::HttpRequest& request) {
		return getEntireFile("/index.html", request);
		});
}

HttpServer& HttpServer::get() {
	static HttpServer srv{};
	return srv;
}

void HttpServer::setRoot(const std::string& _root) {
	root = _root;
}

void HttpServer::setRoot(std::string&& _root) {
	root = std::move(_root);
}

void HttpServer::registerRoute(const std::string& url, util::web::http::Method method, RouteHandlerT handler) {
	if (url.empty()) {
		throw std::runtime_error("route can't be empty");
	}
	if (auto iter = _routes.find(method); iter != _routes.end()) {
		if (auto iter2 = iter->second.find(url); iter2 != iter->second.end()) {
			throw std::logic_error("route already exists");
		}
	}
	_routes[method][url] = handler;
}

void HttpServer::unregisterRoute(const std::string& url, util::web::http::Method method) {
	if (auto iter = _routes.find(method); iter != _routes.end()) {
		iter->second.erase(url);
		if (iter->second.empty()) {
			_routes.erase(method);
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
	default:
		assert(false);
	}
}

util::web::http::HttpResponse HttpServer::_callRoute(const std::string& route, const util::web::http::HttpRequest& request) const {
	if (auto iMethod = _routes.find(request.method); iMethod != _routes.end()) {
		for (const auto& _route : iMethod->second) {
			if (_route.first.back() == '*') {
				// wildcard request - checking prefix to match
				std::string_view _routeSv(_route.first);
				_routeSv = _routeSv.substr(0, _routeSv.size() - 1);
				if ((route.find(_routeSv) == 0) && (_routeSv.size() <= route.size())) {
					return _route.second(request);
				}
			}
			else {
				// exact request - checking for equality
				if (route == _route.first) {
					return _route.second(request);
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

util::web::http::HttpResponse HttpServer::getEntireFile(const std::string& route, const util::web::http::HttpRequest& request) const {
	std::string absRoute = root + route;
	auto pathRoute = std::filesystem::weakly_canonical(absRoute);

	// checking route to be subpath of root to prevent "/../..." access
	if (
		!std::filesystem::exists(absRoute) || 
		!util::fs::isSubpath(std::filesystem::canonical(absRoute), root) || 
		!FileExt2ContentTypeMap.contains(std::filesystem::path(absRoute).extension())) {
		return defaultReponse(404, request);
	}

	std::ifstream ifs(absRoute);
	ifs.seekg(0, ifs.end);
	size_t fsize = ifs.tellg();
	if (fsize > MaxFileSize) {
		return defaultReponse(413, request);
	}
	ifs.seekg(0, ifs.beg);

	std::string responseBody;
	responseBody.resize(fsize);
	ifs.read(responseBody.data(), fsize);

	HttpResponse response(200, request.headers, std::move(responseBody));
	response.headers.add("Content-Type", FileExt2ContentTypeMap.find(pathRoute.extension())->second);
	return response;
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