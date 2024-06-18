#include <cstdlib>
#include <stdint.h>

#include <iostream>
#include <string>
#include <optional>
#include <future>
#include <chrono>
#include <functional>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/log/trivial.hpp>

#include <rapidjson/document.h>

#include <commonUtils.h>

class mini_ws_client {
    using tcp = boost::asio::ip::tcp;

    const std::string host = "testnet.binance.vision";
    const std::string port = "443";
    const std::string target = "/ws-api/v3";

    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23_client};
    boost::asio::io_context ioc;
    std::shared_ptr<boost::beast::websocket::stream<boost::asio::ssl::stream<tcp::socket> > > ws_sp;

public:
    explicit mini_ws_client(
        const std::string host = "testnet.binance.vision",
        const std::string port = "443",
        const std::string target = "/ws-api/v3"
    ): host(host), port(port), target(target) {
        tcp::resolver resolver(ioc);
        ws_sp = std::make_shared<boost::beast::websocket::stream<boost::asio::ssl::stream<tcp::socket> > >(ioc, ctx);
        if (!SSL_set_tlsext_host_name(ws_sp->next_layer().native_handle(), host.c_str())) {
            boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }
        auto const results = resolver.resolve(host, port);
        auto ep = boost::asio::connect(ws_sp->next_layer().next_layer(), results);
        ws_sp->next_layer().handshake(boost::asio::ssl::stream_base::client);
        ws_sp->set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::request_type &req) {
                req.set(boost::beast::http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
            }));
        ws_sp->handshake(host, target);
    }

    ~mini_ws_client() {
        ws_sp->close(boost::beast::websocket::close_code::normal);
        ws_sp.reset();
    }

    std::pair<double, double> get_diff_delay(
        int test_times = 100,
        int one_group_num = 10,
        const std::string msg = "{\"id\":1,\"method\":\"time\"}"
    ) const {
        auto future_example = std::async(
            std::launch::async,
            &mini_ws_client::get_one_timestamps,
            this,
            msg
        );
        std::cout << "type of future_example is " << typeid(future_example).name() << std::endl;
        std::vector<int64_t> timeStamps{};
        int cnt{test_times};
        while (cnt) {
            int cur_group_num{one_group_num};
            if (cnt > one_group_num) {
                cnt -= one_group_num;
            } else {
                cur_group_num = test_times;
                cnt = 0;
            }
            std::cout << "test group num " << cur_group_num << "; " << "remain num " << cnt << std::endl;
            std::vector<std::future<std::vector<int64_t> > > time_stamps_vec{};
            // std::vector<decltype(future_example) > time_stamps_vec{};
            for (int i{0}; i < cur_group_num; ++i) {
                time_stamps_vec.emplace_back(
                    std::async(
                        std::launch::async,
                        &mini_ws_client::get_one_timestamps,
                        this,
                        msg
                    )
                );
            }
            for (auto &timestamps_future: time_stamps_vec) {
                auto one_timestamp = timestamps_future.get();
                printVector(one_timestamp);
                timeStamps.insert(timeStamps.end(), one_timestamp.begin(), one_timestamp.end());
            }
        }
        auto diff_delay = calculateTimeDiffDelaySub(timeStamps);
        return diff_delay;
    }

    std::vector<int64_t> get_one_timestamps(const std::string &msg) const {
        std::vector<int64_t> timeStamps_one_case{};
        auto presend_time = getTimeStamp();
        auto response_str = request_to_response(msg);
        auto postsend_time = getTimeStamp();
        auto server_time = get_server_time(response_str);
        if (server_time) {
            timeStamps_one_case.push_back(presend_time);
            timeStamps_one_case.push_back(postsend_time);
            timeStamps_one_case.push_back(server_time.value());
        }
        printVector(timeStamps_one_case);

        return timeStamps_one_case;
    }

    std::pair<double, double> calculateTimeDiffDelaySub(const std::vector<int64_t> &timeStamps) const {
        double diff{0.};
        double delay{0.};
        for (int i{0}; i < timeStamps.size(); i += 3) {
            diff += (
                timeStamps[i]
                + timeStamps[i + 1]
                - (timeStamps[i + 2] << 1)
            ) * 0.5;
            delay += (timeStamps[i + 1] - timeStamps[i]) >> 1;
        }
        diff /= timeStamps.size() / 3;
        delay /= timeStamps.size() / 3;
        auto ret = std::make_pair(diff, delay);
        return ret;
    }


    int64_t getTimeStamp(int64_t diff = 0LL) const {
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();

        // 将时间点转换为毫秒
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        // 输出 13 位时间戳
        return millis - diff;
    }


    std::string request_to_response(const std::string &msg) const {
        ws_sp->write(boost::asio::buffer(msg));
        boost::beast::flat_buffer buffer;
        ws_sp->read(buffer);
        const char *bufferData = reinterpret_cast<const char *>(buffer.data().data());
        std::size_t bufferSize = buffer.data().size();
        std::string response(bufferData, bufferSize);
        return response;
    }

    std::optional<int64_t> get_server_time(const std::string &msg) const {
        rapidjson::Document msg_doc;
        msg_doc.Parse(msg.c_str());
        if (msg_doc.HasParseError() || !msg_doc.IsObject()) {
            BOOST_LOG_TRIVIAL(fatal) << __FILE__ << ": " << __LINE__ << "# " << "JSON parse error or not an object" <<
     std::endl;
            return std::nullopt;
        }
        if (!msg_doc.HasMember("result") || !msg_doc["result"].IsObject()) {
            BOOST_LOG_TRIVIAL(fatal) << __FILE__ << ": " << __LINE__ << "# " <<
     "No 'result' field or 'result' is not an object" << std::endl;
            return std::nullopt;
        }
        const rapidjson::Value &result = msg_doc["result"];
        if (!result.HasMember("serverTime") || !result["serverTime"].IsInt64()) {
            BOOST_LOG_TRIVIAL(fatal) << __FILE__ << ": " << __LINE__ << "# " <<
 "'serverTime' field missing or not an int64"
     << std::endl;
            return std::nullopt;
        }
        int64_t serverTime = result["serverTime"].GetInt64();
        return serverTime;
    }
};


int main() {
    auto ws_client = std::make_shared<mini_ws_client>("ws-api.binance.com", "443", "/ws-api/v3");
    std::cout << ws_client->request_to_response("{\"id\":1,\"method\":\"time\"}") << std::endl;;
    auto &[diff,delay] = ws_client->get_diff_delay();
    std::cout << "diff = " << diff << std::endl;
    std::cout << "delay = " << delay << std::endl;
}
