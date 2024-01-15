#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "EventBroker.hpp"
#include "Http.hpp"

class HttpServer {
public:
	using CallbackMsgFn = EventBroker::OnEventCb;
	using RouteHandlerT = std::function<util::web::http::HttpResponse(const util::web::http::HttpRequest&, CallbackMsgFn)>;
	void registerRoute(const std::string& url, util::web::http::Method method, RouteHandlerT handler);
	void unregisterRoute(const std::string& url, util::web::http::Method method);
	util::web::http::HttpResponse defaultReponse(size_t statusCode, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse callRoute(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	inline auto& routes() { return _routes; }
	static HttpServer& get();
	void setRoot(const std::string& root);
	void setRoot(std::string&& root);
	util::web::http::HttpResponse GET(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse HEAD(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse POST(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse PUT(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse DELETE(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse CONNECT(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse OPTIONS(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse TRACE(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse PATCH(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	util::web::http::HttpResponse getEntireFile(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
private:
	util::web::http::HttpResponse _callRoute(const std::string& route, const util::web::http::HttpRequest& request, CallbackMsgFn cbMsgFn = nullptr) const;
	HttpServer();
	std::unordered_map<util::web::http::Method, std::unordered_map<std::string, RouteHandlerT>> _routes;
	std::string root;
};