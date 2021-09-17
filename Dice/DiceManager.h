#pragma once
#include <regex>
#include <CivetServer.h>
#include <json.hpp>
#include "GlobalVar.h"
#include "EncodingConvert.h"
#include "DiceConsole.h"
#include "DiceSchedule.h"
#include "MsgMonitor.h"
#include "CQTools.h"
#include "CardDeck.h"

class AuthHandler: public CivetAuthHandler
{
    bool authorize(CivetServer *server, struct mg_connection *conn)
    {
        const char* auth = server->getHeader(conn, "Authorization");
        // Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
        if (auth == nullptr || std::string(auth).substr(0, 5) != "Basic" || base64_decode(std::string(auth).substr(6)) != "admin:password")
        {
            // 401 Unauthorised
            mg_response_header_start(conn, 401);

            // Disable Cache
            mg_response_header_add(conn,
	                       "Cache-Control",
	                       "no-cache, no-store, "
	                       "must-revalidate, private, max-age=0",
	                       -1);
            mg_response_header_add(conn, "Expires", "0", -1);

            // For HTTP 1.0
            mg_response_header_add(conn, "Pragma", "no-cache", -1);

            // Basic Auth Request
            mg_response_header_add(conn, "WWW-Authenticate", "Basic realm=\"Dice WebUI\"", -1);

            mg_response_header_send(conn);

            return false;
        }

        return true;
    }
};

class IndexHandler : public CivetHandler
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string html = 
			#include "webui.html"
		;

        mg_send_http_ok(conn, "text/html", html.length());
        mg_write(conn, html.c_str(), html.length());
        return true;
    }
};

class BasicInfoApiHandler : public CivetHandler
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
			j["data"] = nlohmann::json::object();
            j["data"]["version"] = GBKtoUTF8(Dice_Full_Ver);
            j["data"]["qq"] = console.DiceMaid;
            j["data"]["nick"] = GBKtoUTF8(getMsg("strSelfName"));
            j["data"]["running_time"] = GBKtoUTF8(printDuringTime(time(nullptr) - llStartTime));
            j["data"]["cmd_count"] = std::to_string(FrqMonitor::sumFrqTotal.load());
            j["data"]["cmd_count_today"] = std::to_string(today->get("frq"));
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }

        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;    
    }
};

class CustomMsgApiHandler : public CivetHandler
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            std::shared_lock lock(GlobalMsgMutex);
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
            j["count"] = GlobalMsg.size();
			j["data"] = nlohmann::json::array();
            for (const auto& [key,val] : GlobalMsg)
            {
                j["data"].push_back({{"name", GBKtoUTF8(key)}, {"value", GBKtoUTF8(val)}, {"remark", GBKtoUTF8(getComment(key))}});
            }
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }

        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try 
        {
            auto data = server->getPostData(conn);
            nlohmann::json j = nlohmann::json::parse(data);
            if (j["action"].get<std::string>() == "set")
            {
                std::unique_lock lock(GlobalMsgMutex);
                for(const auto& item: j["data"])
                {
                    GlobalMsg[UTF8toGBK(item["name"].get<std::string>())] = UTF8toGBK(item["value"].get<std::string>());
                    EditedMsg[UTF8toGBK(item["name"].get<std::string>())] = UTF8toGBK(item["value"].get<std::string>());
                }
                saveJMap(DiceDir / "conf" / "CustomMsg.json", EditedMsg);
            } 
            else
            {
                throw std::runtime_error("Invalid Action");
            }
            nlohmann::json j2 = nlohmann::json::object();   
            j2["code"] = 0;
            j2["msg"] = "ok";
            ret = j2.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }
};

class CustomReplyApiHandler : public CivetHandler
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
            j["count"] = CardDeck::mReplyDeck.size();
			j["data"] = nlohmann::json::array();
            for (const auto& [key,val] : CardDeck::mReplyDeck)
            {
                string t;
                for (const auto& item : val)
                {
                    t.append(GBKtoUTF8(item));
                    t.append("|");
                }
                t = t.substr(0, t.size() - 1);
                j["data"].push_back({{"name", GBKtoUTF8(key)}, {"value", t}});
            }
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }

        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try 
        {
            auto data = server->getPostData(conn);
            nlohmann::json j = nlohmann::json::parse(data);
            if (j["action"].get<std::string>() == "set")
            {
                for(const auto& item: j["data"])
                {
                    auto& deck = CardDeck::mReplyDeck[UTF8toGBK(item["name"].get<std::string>())];
                    deck = {};
                    auto v = item["value"].get<std::vector<std::string>>();
                    for(const auto& i : v)
                    {
                        deck.push_back(UTF8toGBK(i));
                    }
                }
                saveJMap(DiceDir / "conf" / "CustomReply.json", CardDeck::mReplyDeck);
            } 
            else if (j["action"].get<std::string>() == "delete")
            {
                for(const auto& item: j["data"])
                {
                    CardDeck::mReplyDeck.erase(UTF8toGBK(item["name"].get<std::string>()));
                }
                saveJMap(DiceDir / "conf" / "CustomReply.json", CardDeck::mReplyDeck);
            }
            else
            {
                throw std::runtime_error("Invalid Action");
            }
            nlohmann::json j2 = nlohmann::json::object();   
            j2["code"] = 0;
            j2["msg"] = "ok";
            ret = j2.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }
};

class CustomRegexReplyApiHandler : public CivetHandler
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
            j["count"] = CardDeck::mRegexReplyDeck.size();
			j["data"] = nlohmann::json::array();
            for (const auto& [key,val] : CardDeck::mRegexReplyDeck)
            {
                string t;
                for (const auto& item : val)
                {
                    t.append(GBKtoUTF8(item));
                    t.append("|");
                }
                t = t.substr(0, t.size() - 1);
                j["data"].push_back({{"name", GBKtoUTF8(key)}, {"value", t}});
            }
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }

        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try 
        {
            auto data = server->getPostData(conn);
            nlohmann::json j = nlohmann::json::parse(data);
            if (j["action"].get<std::string>() == "set")
            {
                for(const auto& item: j["data"])
                {
                    const std::string re = UTF8toGBK(item["name"].get<std::string>());
                    const auto g = std::regex(re, std::regex::ECMAScript);
                    auto& deck = CardDeck::mRegexReplyDeck[re];
                    deck = {};
                    auto v = item["value"].get<std::vector<std::string>>();
                    for(const auto& i : v)
                    {
                        deck.push_back(UTF8toGBK(i));
                    }
                }
                saveJMap(DiceDir / "conf" / "CustomRegexReply.json", CardDeck::mRegexReplyDeck);
            } 
            else if (j["action"].get<std::string>() == "delete")
            {
                for(const auto& item: j["data"])
                {
                    CardDeck::mRegexReplyDeck.erase(UTF8toGBK(item["name"].get<std::string>()));
                }
                saveJMap(DiceDir / "conf" / "CustomRegexReply.json", CardDeck::mRegexReplyDeck);
            }
            else
            {
                throw std::runtime_error("Invalid Action");
            }
            nlohmann::json j2 = nlohmann::json::object();   
            j2["code"] = 0;
            j2["msg"] = "ok";
            ret = j2.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }
};

class AdminConfigHandler : public CivetHandler 
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
            j["count"] = GlobalMsg.size();
			j["data"] = nlohmann::json::array();
            for (const auto& item : console.intDefault)
            {
                const int value = console[item.first.c_str()];
                j["data"].push_back({{"name", GBKtoUTF8(item.first)}, {"value", value}});
            }
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try 
        {
            auto data = server->getPostData(conn);
            nlohmann::json j = nlohmann::json::parse(data);
            if (j["action"].get<std::string>() == "set")
            {
                for(const auto& item: j["data"])
                {
                    console.set(UTF8toGBK(item["name"].get<std::string>()), item["value"].get<int>());
                }
            } 
            else
            {
                throw std::runtime_error("Invalid Action");
            }
            nlohmann::json j2 = nlohmann::json::object();   
            j2["code"] = 0;
            j2["msg"] = "ok";
            ret = j2.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }
};

class MasterHandler : public CivetHandler 
{
public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = 0;
            j["msg"] = "ok";
			j["data"] = nlohmann::json::object();
            j["data"]["masterQQ"] = console.master();
            ret = j.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        std::string ret;
        try 
        {
            auto data = server->getPostData(conn);
            nlohmann::json j = nlohmann::json::parse(data);
            if (j["action"].get<std::string>() == "set")
            {
                const long long masterQQ = j["data"]["masterQQ"].get<long long>();
                if (masterQQ == 0)
                {
                    if (console)
                    {
						console.killMaster();
                        console.isMasterMode = false;
                    }
                } 
                else
                {
                    if (console)
					{
						if (console.masterQQ != masterQQ)
						{
							console.killMaster();
							console.newMaster(masterQQ);
						}
					}
                    else
                    {
                        console.newMaster(masterQQ);
                        console.isMasterMode = true;
                    }
                }
            } 
            else
            {
                throw std::runtime_error("Invalid Action");
            }
            nlohmann::json j2 = nlohmann::json::object();   
            j2["code"] = 0;
            j2["msg"] = "ok";
            ret = j2.dump();
        }
        catch(const std::exception& e)
        {
            nlohmann::json j = nlohmann::json::object();
            j["code"] = -1;
            j["msg"] = GBKtoUTF8(e.what());
            ret = j.dump();
        }
        mg_send_http_ok(conn, "application/json", ret.length());
        mg_write(conn, ret.c_str(), ret.length());
        return true;
    }
};