#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <iostream>
#include <future>
#include <memory>
#include <thread>
#include <atomic>
#include "../lib/json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

#define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
#define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
#define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

struct TelemPack {
    // position
    std::atomic<double> latitude;
    std::atomic<double> longitude;
    std::atomic<float> abs_alt;
    std::atomic<float> rel_alt;
    // velocity
    std::atomic<float> vel_north;
    std::atomic<float> vel_east;
    std::atomic<float> vel_down;
    // plane data
    std::atomic<float> airspeed;
    std::atomic<float> climb_rate;
    // angle
    std::atomic<float> roll_deg;
    std::atomic<float> pitch_deg;
    std::atomic<float> yaw_deg;
    // misc
    std::atomic<bool> isAllOk;
    std::atomic<bool> isArmed;
    std::atomic<bool> inAir;
    std::atomic<float> batt_percentage;
    std::atomic<float> batt_voltage;
};


int main()
{
    Mavsdk mavsdk;
    std::string connection_url;
    ConnectionResult connection_result;

    connection_result = mavsdk.add_any_connection("udp://:14540");

    if (connection_result != ConnectionResult::Success) {
        std::cout << ERROR_CONSOLE_TEXT << "Connection failed: " << connection_result
                  << NORMAL_CONSOLE_TEXT << std::endl;
        return 1;
    }

    std::cout << "Waiting to discover system..." << std::endl;
    auto prom = std::promise<std::shared_ptr<System>>{};
    auto fut = prom.get_future();

    mavsdk.subscribe_on_new_system([&mavsdk, &prom]() {
        auto system = mavsdk.systems().back();

        if (system->has_autopilot()) {
            std::cout << "Discovered autopilot" << std::endl;

            // Unsubscribe again as we only want to find one system.
            mavsdk.subscribe_on_new_system(nullptr);
            prom.set_value(system);
        }
    });

    if (fut.wait_for(seconds(3)) == std::future_status::timeout) {
        std::cout << ERROR_CONSOLE_TEXT << "No autopilot found, exiting." << NORMAL_CONSOLE_TEXT
                  << std::endl;
        return 1;
    }

    auto system = fut.get();
    auto telemetry = Telemetry{system};
    TelemPack pack;

    // lambdas for telemetry
    telemetry.subscribe_position([&pack](Telemetry::Position position) {
       pack.latitude = position.latitude_deg;
       pack.longitude = position.longitude_deg;
       pack.abs_alt = position.absolute_altitude_m;
       pack.rel_alt = position.relative_altitude_m; 
    });

    telemetry.subscribe_velocity_ned([&pack](Telemetry::VelocityNed vel) {
        pack.vel_down = vel.down_m_s;
        pack.vel_east = vel.east_m_s;
        pack.vel_north = vel.north_m_s;
    });

    telemetry.subscribe_fixedwing_metrics([&pack](Telemetry::FixedwingMetrics met) {
        pack.airspeed = met.airspeed_m_s;
        pack.climb_rate = met.climb_rate_m_s;
    });

    telemetry.subscribe_attitude_euler([&pack](Telemetry::EulerAngle ang) {
        pack.pitch_deg = ang.pitch_deg;
        pack.roll_deg = ang.roll_deg;
        pack.yaw_deg = ang.yaw_deg;
    });

    telemetry.subscribe_battery([&pack](Telemetry::Battery batt) {
        pack.batt_percentage = batt.remaining_percent;
        pack.batt_voltage = batt.voltage_v;
    });

    telemetry.subscribe_health_all_ok([&pack](bool health) {
        pack.isAllOk = health;
    });

    telemetry.subscribe_armed([&pack](bool armed) {
        pack.isArmed = armed;
    });

    telemetry.subscribe_in_air([&pack](bool inAir) {
        pack.inAir = inAir;
    });

    // server loop
    {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        char buffer[128] = {0};

        if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "sock failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
      
            std::cout << ERROR_CONSOLE_TEXT << "setsockopt failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }


        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(6969);

        if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            std::cout << ERROR_CONSOLE_TEXT << "sock bind failed" << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
            
        }

        while(listen(server_fd, 10) >= 0)
        {
            if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
            {
               std::cout << TELEMETRY_CONSOLE_TEXT << "accepting failed" << NORMAL_CONSOLE_TEXT << std::endl;
               continue;
            }

            read(new_socket, buffer, 128);
            if(std::string(buffer) == "get")
            {
                // pack to json
                nlohmann::json j;
                j["position"] = {
                    {"lat",     (double)pack.latitude},
                    {"lon",     (double)pack.longitude},
                    {"alt_abs", (float)pack.abs_alt},
                    {"alt_rel", (float)pack.rel_alt}
                };
                j["velocity"] = {
                    {"north",   (float)pack.vel_north},
                    {"east",    (float)pack.vel_east},
                    {"down",    (float)pack.vel_down}
                };
                j["plane"] = {
                    {"airspeed",    (float)pack.airspeed},
                    {"climbrate",   (float)pack.climb_rate}
                };
                j["angles"] = {
                    {"pitch",   (float)pack.pitch_deg},
                    {"roll",    (float)pack.roll_deg},
                    {"yaw",     (float)pack.yaw_deg}
                };
                j["battery"] = {
                    {"percent", (float)pack.batt_percentage},
                    {"voltage", (float)pack.batt_voltage}
                };
                j["misc"] = {
                    {"health",  (bool)pack.isAllOk},
                    {"armed",   (bool)pack.isArmed},
                    {"inAir",     (bool)pack.inAir}
                };

                std::string jsonDump = j.dump();

                send(new_socket, jsonDump.c_str(), jsonDump.length(), 0);
                
            }

            memset(buffer, 0, 128);
        }
    }

    return 0;
}