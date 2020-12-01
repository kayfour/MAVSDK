#include <iostream>
#include <chrono>
#include <thread>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/mission/mission.h>
#include <future>
#include <math.h>
using namespace mavsdk;
#define PI 3.1415926535897

int main(int argc, char** argv)
{   //==============================================================================================
    // connect_result
    //==============================================================================================
    Mavsdk mavsdk;
    ConnectionResult connection_result;
    bool discovered_system = false;

    connection_result = mavsdk.add_any_connection(argv[1]);

    if (connection_result != ConnectionResult::Success) {return 1;}

    mavsdk.subscribe_on_new_system([&mavsdk, &discovered_system]() {    // 콜백함수 with 익명함수
        const auto system = mavsdk.systems().at(0);
        if (system->is_connected()) {discovered_system = true;} // 시스템 연결 확인
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));   // 콜백함수에 의한 시스템 연결 대기

    if (!discovered_system) {return 1;} // 시스템 디텍트 실패시 리턴 1
    const auto system = mavsdk.systems().at(0);

    system->register_component_discovered_callback(
        [](ComponentType component_type) -> void {std::cout << unsigned(component_type);}
    );
    //==============================================================================================
    // regist_telemetry
    //==============================================================================================
    auto telemetry = std::make_shared<Telemetry>(system);   // 원격 측정

    const Telemetry::Result set_rate_result = telemetry -> set_rate_position(1.0);  // 1초마다 수신
    if(set_rate_result != Telemetry::Result::Success){return 1;}

    double lat, lon;

    telemetry -> subscribe_position([&lat, &lon](Telemetry::Position position){   // 고도를 모니터링하기 위한 콜백함수, 익명함수
            // std::cout << "Altitude: " << position.relative_altitude_m << "m" << std::endl;  
            lat = position.latitude_deg;
            lon = position.longitude_deg;
            // std::cout << "lat: " << lat << " deg";
            // std::cout << " lon: " << lon << " deg" << std::endl; 
    });
    while (telemetry -> health_all_ok() != true){
        std::cout << "Vehicle is getting ready to arm" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    //==============================================================================================
    // 1.add mission and check mission size
    //==============================================================================================
    std::vector<Mission::MissionItem> mission_items;
    Mission::MissionItem mission_item;
    const double center_lat=lat, center_lon=lon;
    int max = 45;
    double limit = 3*max, w;
    for(int j = 0; j < 3;j++){
        for(int i = 0; i<max+1; i++){
            w = (limit--)/(3*max);
            mission_item.latitude_deg = center_lat + w*0.0004*sin(i*2*PI/max );    // 범위: -90 to +90
            mission_item.longitude_deg = center_lon + w*0.0004*cos(i*2*PI/max );    // 범위: -180 to +180
            mission_item.relative_altitude_m = 10.0f;    // takeoff altitude
            mission_item.speed_m_s = 27.77777777777777777777777777778f;
            mission_item.is_fly_through = true;    // 비행 안하고 정지

            mission_items.push_back(mission_item);  // 미션 웨이포인트를 items에 저장
        }
    }
    std::cout << "Mission size: " << mission_items.size() << std::endl;

    //==============================================================================================
    // 2.upload mission
    //==============================================================================================
    auto mission = std::make_shared<Mission>(system);   
    {
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom -> get_future();  // 결과를 가져온다
        Mission::MissionPlan mission_plan;
        mission_plan.mission_items = mission_items;
        mission->upload_mission_async(mission_plan,
                                    [prom](Mission::Result result){
                                        prom -> set_value(result);
                                    });
        const Mission::Result result = future_result.get();
        if(result != Mission::Result::Success){return 1;}
    }
    //==============================================================================================
    // arm
    //==============================================================================================
    auto action = std::make_shared<Action>(system);
    std:: cout << "Arming..." << std::endl;
    const Action::Result arm_result = action -> arm();  // 암

    if(arm_result != Action::Result::Success){
        std::cout << "Arming failed: " << arm_result <<std::endl;
        return 1;
    }
    //==============================================================================================
    // 3.mission progress
    //==============================================================================================
    {
        std:: cout << "Starting mission." << std::endl;
        auto start_prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = start_prom -> get_future();
        mission->start_mission_async([start_prom](Mission::Result result){
            start_prom -> set_value(result);
            std::cout << "Started mission." << std::endl;
        });
        const Mission::Result result = future_result.get();
        if(result != Mission::Result::Success){return 1;}
        while (!mission -> is_mission_finished().second){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    //==============================================================================================
    // 4.return_to_launch
    //==============================================================================================
    {
        // We are done, and can do RTL to go home.
        std::cout << "return_to_launch..." << std::endl;
        const Action::Result result = action->return_to_launch();
        if (result != Action::Result::Success) {
            std::cout << "Failed return to launch" << std::endl;
        }
    }
    //==============================================================================================
    // 5.disarmed
    //==============================================================================================
    while (telemetry->armed()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Disarmed..." << std::endl;
    std::cout << "Finishied" << std::endl;

    return 0;
}
