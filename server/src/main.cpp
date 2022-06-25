#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <iostream>
#include <future>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "../lib/json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#ifdef __APPLE__
#define MSG_CONFIRM 0
#endif

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

#define ERROR_CONSOLE_TEXT "\033[31m"     // Turn text on console red
#define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
#define NORMAL_CONSOLE_TEXT "\033[0m"     // Restore normal console colour

#define BUFFER_SIZE 256
#define REFRESH_TELEM 90.0f

#define MAX_OFB_SPEED 2.0f   // 2 m/s
#define MAX_OFB_Z_SPEED 1.0f // 1 m/s

struct TelemPack
{
    // position
    std::atomic<double> latitude = 0.0f;
    std::atomic<double> longitude = 0.0f;
    std::atomic<float> abs_alt = 0.0f;
    std::atomic<float> rel_alt = 0.0f;
    // velocity
    std::atomic<float> vel_north = 0.0f;
    std::atomic<float> vel_east = 0.0f;
    std::atomic<float> vel_down = 0.0f;
    // plane data
    std::atomic<float> airspeed = 0.0f;
    std::atomic<float> climb_rate = 0.0f;
    // angle
    std::atomic<float> roll_deg = 0.0f;
    std::atomic<float> pitch_deg = 0.0f;
    std::atomic<float> yaw_deg = 0.0f;
    // misc
    std::atomic<bool> isAllOk = false;
    std::atomic<bool> isArmed = false;
    std::atomic<bool> inAir = false;
    std::atomic<float> batt_percentage = 0.0f;
    std::atomic<float> batt_voltage = 0.0f;
};

static int udp_sockfd;
static struct sockaddr_in udp_servaddr, udp_cliaddr;

std::string pack_to_json(TelemPack &pack)
{
    try
    {
        nlohmann::json j;
        j["position"] = {
            {"lat", (double)pack.latitude},
            {"lon", (double)pack.longitude},
            {"alt_abs", (float)pack.abs_alt},
            {"alt_rel", (float)pack.rel_alt}};
        j["velocity"] = {
            {"north", (float)pack.vel_north},
            {"east", (float)pack.vel_east},
            {"down", (float)pack.vel_down}};
        j["plane"] = {
            {"airspeed", (float)pack.airspeed},
            {"climbrate", (float)pack.climb_rate}};
        j["angles"] = {
            {"pitch", (float)pack.pitch_deg},
            {"roll", (float)pack.roll_deg},
            {"yaw", (float)pack.yaw_deg}};
        j["battery"] = {
            {"percent", (float)pack.batt_percentage},
            {"voltage", (float)pack.batt_voltage}};
        j["misc"] = {
            {"health", (bool)pack.isAllOk},
            {"armed", (bool)pack.isArmed},
            {"inAir", (bool)pack.inAir}};

        return j.dump();
    }
    catch (nlohmann::json::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        return "";
    }
}

int main()
{
    Mavsdk mavsdk;
    std::string connection_url;
    ConnectionResult connection_result;
    TelemPack global_pack;

    bool offb_running = false;
    auto offb_last_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> udp_ips = {};

    connection_result = mavsdk.add_any_connection("udp://:14540");

    if (connection_result != ConnectionResult::Success)
    {
        std::cout << ERROR_CONSOLE_TEXT << "Connection failed: " << connection_result
                  << NORMAL_CONSOLE_TEXT << std::endl;
        return 1;
    }

    std::cout << "Waiting to discover system..." << std::endl;
    auto prom = std::promise<std::shared_ptr<System>>{};
    auto fut = prom.get_future();

    mavsdk.subscribe_on_new_system([&mavsdk, &prom]()
                                   {
                                       auto system = mavsdk.systems().back();

                                       if (system->has_autopilot())
                                       {
                                           std::cout << "Discovered autopilot" << std::endl;

                                           // Unsubscribe again as we only want to find one system.
                                           mavsdk.subscribe_on_new_system(nullptr);
                                           prom.set_value(system);
                                       } });

    if (fut.wait_for(seconds(3)) == std::future_status::timeout)
    {
        std::cout << ERROR_CONSOLE_TEXT << "No autopilot found, exiting." << NORMAL_CONSOLE_TEXT
                  << std::endl;
        return 1;
    }

    auto system = fut.get();
    auto telemetry = Telemetry{system};
    auto action = Action{system};
    auto offboard = Offboard{system};

    // lambdas for telemetry
    telemetry.subscribe_position([&global_pack](Telemetry::Position position)
                                 {
                                     global_pack.latitude = position.latitude_deg;
                                     global_pack.longitude = position.longitude_deg;
                                     global_pack.abs_alt = position.absolute_altitude_m;
                                     global_pack.rel_alt = position.relative_altitude_m; });

    telemetry.subscribe_velocity_ned([&global_pack](Telemetry::VelocityNed vel)
                                     {
                                         global_pack.vel_down = vel.down_m_s;
                                         global_pack.vel_east = vel.east_m_s;
                                         global_pack.vel_north = vel.north_m_s; });

    telemetry.subscribe_fixedwing_metrics([&global_pack](Telemetry::FixedwingMetrics met)
                                          {
                                              global_pack.airspeed = met.airspeed_m_s;
                                              global_pack.climb_rate = met.climb_rate_m_s; });

    telemetry.subscribe_attitude_euler([&global_pack](Telemetry::EulerAngle ang)
                                       {
                                           global_pack.pitch_deg = ang.pitch_deg;
                                           global_pack.roll_deg = ang.roll_deg;
                                           global_pack.yaw_deg = ang.yaw_deg; });

    telemetry.subscribe_battery([&global_pack](Telemetry::Battery batt)
                                {
                                    global_pack.batt_percentage = batt.remaining_percent;
                                    global_pack.batt_voltage = batt.voltage_v; });

    telemetry.subscribe_health_all_ok([&global_pack](bool health)
                                      { global_pack.isAllOk = health; });

    telemetry.subscribe_armed([&global_pack](bool armed)
                              { global_pack.isArmed = armed; });

    telemetry.subscribe_in_air([&global_pack](bool inAir)
                               { global_pack.inAir = inAir; });

    // creating udp thread
    {
        if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "udp socket failed!" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        memset(&udp_servaddr, 0, sizeof(udp_servaddr));
        memset(&udp_cliaddr, 0, sizeof(udp_cliaddr));

        udp_servaddr.sin_family = AF_INET;
        udp_servaddr.sin_addr.s_addr = INADDR_ANY;
        udp_cliaddr.sin_port = htons(6969);
        udp_cliaddr.sin_family = AF_INET;
        udp_cliaddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udp_sockfd, (const struct sockaddr *)&udp_servaddr, sizeof(udp_servaddr)) < 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "udp bind failed!" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        auto send_thread = std::thread([&udp_ips, &global_pack]()
                                       {
                                           int period_ms = (1.0f / REFRESH_TELEM) * 1000.0f;
                                           while (true)
                                           {
                                               auto t1 = std::chrono::high_resolution_clock::now();
                                               auto json_pack = pack_to_json(global_pack);
                                               for (auto ip : udp_ips)
                                               {
                                                   udp_cliaddr.sin_addr.s_addr = inet_addr(ip.c_str());
                                                   sendto(udp_sockfd, (const char *)json_pack.c_str(), json_pack.length(),
                                                          MSG_CONFIRM, (const struct sockaddr *)&udp_cliaddr, sizeof(udp_cliaddr));
                                               }
                                               auto t2 = std::chrono::high_resolution_clock::now();
                                               auto sending_time = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
                                               std::this_thread::sleep_for(
                                                   std::chrono::milliseconds(period_ms - sending_time.count()));
                                           } });
        send_thread.detach();

        auto offb_check_thread = std::thread([&offb_running, &offb_last_time, &offboard](){
            int period_ms = (1.0f / REFRESH_TELEM) * 1000.0f;
            while (true)
            {
                if(offb_running) {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - offb_last_time);

                    if (time_elapsed.count() > 2.0f * 1000.0f)
                    {
                        std::cout << ERROR_CONSOLE_TEXT << "STOPING OFFBOARD!" << NORMAL_CONSOLE_TEXT << std::endl;
                        auto result = offboard.stop();
                        if(result == Offboard::Result::Success)
                            offb_running = false;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
                }
            }
        });

        offb_check_thread.detach();
    }
    // server loop
    {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        char buffer[BUFFER_SIZE] = {0};

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "sock failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {

            std::cout << ERROR_CONSOLE_TEXT << "setsockopt failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(6969);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "sock bind failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        while (listen(server_fd, 10) >= 0)
        {
            memset(buffer, 0, BUFFER_SIZE);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cout << TELEMETRY_CONSOLE_TEXT << "accepting failed" << NORMAL_CONSOLE_TEXT << std::endl;
                continue;
            }

            read(new_socket, buffer, BUFFER_SIZE);
            if (std::string(buffer) == "get")
            {
                // pack to json
                std::string jsonDump = pack_to_json(global_pack);
                send(new_socket, jsonDump.c_str(), jsonDump.length(), 0);
                continue;
            }

            nlohmann::json command;
            std::string command_type;
            try
            {
                command = nlohmann::json::parse(buffer);
                command_type = (std::string)command["command"];
            }
            catch (nlohmann::json::parse_error &ex)
            {
                std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                std::string error(ex.what());
                send(new_socket, error.c_str(), error.size(), 0);
                continue;
            }

            if (command_type == "goto")
            {
                double lat, lon;
                float alt, heading;
                try
                {
                    lat = (double)command["lat"];
                    lon = (double)command["lon"];
                    alt = (float)command["alt"];
                    heading = (float)command["heading"];
                }
                catch (nlohmann::json::exception &ex)
                {
                    std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                    std::string error(ex.what());
                    send(new_socket, error.c_str(), error.size(), 0);
                    continue;
                }
                float alt_abs = global_pack.abs_alt + (alt - global_pack.rel_alt);
                auto result = action.goto_location(lat, lon, alt_abs, heading);
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }

            if (command_type == "takeoff")
            {
                float alt;
                try
                {
                    alt = (float)command["alt"];
                }
                catch (nlohmann::json::exception &ex)
                {
                    std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                    std::string error(ex.what());
                    send(new_socket, error.c_str(), error.size(), 0);
                    continue;
                }
                auto change_alt_result = action.set_takeoff_altitude(alt);
                if (change_alt_result != Action::Result::Success)
                {
                    send(new_socket, "failed change_alt", 18, 0);
                    continue;
                }

                auto result = action.takeoff();
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed takeoff", 15, 0);
                }
                continue;
            }

            if (command_type == "arm_takeoff")
            {
                float alt;
                try
                {
                    alt = (float)command["alt"];
                }
                catch (nlohmann::json::exception &ex)
                {
                    std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                    std::string error(ex.what());
                    send(new_socket, error.c_str(), error.size(), 0);
                    continue;
                }

                auto change_alt_result = action.set_takeoff_altitude(alt);
                if (change_alt_result != Action::Result::Success)
                {
                    send(new_socket, "failed change_alt", 18, 0);
                    continue;
                }

                auto arm_result = action.arm();
                if (arm_result != Action::Result::Success)
                {
                    send(new_socket, "failed arm", 11, 0);
                    continue;
                }

                auto result = action.takeoff();
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed takeoff", 15, 0);
                }
                continue;
            }

            if (command_type == "rtl")
            {
                auto result = action.return_to_launch();
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }

            if (command_type == "add_udp")
            {
                struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&address;
                struct in_addr ipAddr = pV4Addr->sin_addr;
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
                udp_ips.push_back(std::string(str));
                send(new_socket, "success", 8, 0);
            }

            if (command_type == "actuator")
            {
                int index;
                float value;
                try
                {
                    index = (int)command["index"];
                    value = (float)command["value"];
                }
                catch (nlohmann::json::exception &ex)
                {
                    std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                    std::string error(ex.what());
                    send(new_socket, error.c_str(), error.size(), 0);
                    continue;
                }
                auto result = action.set_actuator(index, value);
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }

            if (command_type == "offboard_start")
            {
                action.hold();
                std::cout << TELEMETRY_CONSOLE_TEXT << "offboard start" << NORMAL_CONSOLE_TEXT  << std::endl;
                auto result = offboard.start();
                if (result == Offboard::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                    offb_running = true;
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }

            if (command_type == "offboard_stop")
            {
                std::cout << TELEMETRY_CONSOLE_TEXT << "offboard stop" << NORMAL_CONSOLE_TEXT  << std::endl;
                auto result = offboard.stop();
                if (result == Offboard::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                    offb_running = false;
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }

            if (command_type == "offboard_cmd")
            {
                std::cout << TELEMETRY_CONSOLE_TEXT << "offboard cmd" << NORMAL_CONSOLE_TEXT  << std::endl;
                float x = 0.0f, y = 0.0f, z = 0.0f;

                try
                {
                    x = (float)command["x"];
                    y = (float)command["y"];
                    z = (float)command["z"];
                }
                catch (nlohmann::json::exception &ex)
                {
                    std::cout << ERROR_CONSOLE_TEXT << ex.what() << NORMAL_CONSOLE_TEXT << std::endl;
                    std::string error(ex.what());
                    send(new_socket, error.c_str(), error.size(), 0);
                    continue;
                }

                if (std::fabs(z) > MAX_OFB_Z_SPEED)
                    z = (z / std::fabs(z)) * MAX_OFB_Z_SPEED;

                float speed = sqrt(x * x + y * y);
                if (speed > MAX_OFB_SPEED)
                {
                    x = (x / speed) * MAX_OFB_SPEED;
                    y = (y / speed) * MAX_OFB_SPEED;
                }

                Offboard::VelocityBodyYawspeed cmd{(float)x, (float)y, (float)z, (float)0.0f};
                auto result = offboard.set_velocity_body(cmd);

                if (result == Offboard::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                    offb_last_time = std::chrono::high_resolution_clock::now();
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }

                continue;
            }

            if (command_type == "land")
            {
                auto result = action.land();
                if (result == Action::Result::Success)
                {
                    send(new_socket, "success", 8, 0);
                }
                else
                {
                    send(new_socket, "failed", 7, 0);
                }
                continue;
            }
        }
    }

    return 0;
}
