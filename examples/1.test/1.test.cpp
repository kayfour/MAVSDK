#include <iostream>
#include <chrono>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <thread>
using namespace mavsdk;

int main(int argc, char** argv)
{   //==============================================================================================
    // 1. connect_result
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
    // 2.regist_telemetry
    //==============================================================================================
    auto telemetry = std::make_shared<Telemetry>(system);   // 원격 측정

    const Telemetry::Result set_rate_result = telemetry -> set_rate_position(1.0);  // 1초마다 수신
    if(set_rate_result != Telemetry::Result::Success){return 1;}
    telemetry -> subscribe_position([](Telemetry::Position position){   // 고도를 모니터링하기 위한 콜백함수, 익명함수
            std::cout << "Altitude: " << position.relative_altitude_m << "m" << std::endl;  
    });
    while (telemetry -> health_all_ok() != true){
        std::cout << "Vehicle is getting ready to arm" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    //==============================================================================================
    // 3.arm
    //==============================================================================================
    auto action = std::make_shared<Action>(system);
    std:: cout << "Arming..." << std::endl;
    const Action::Result arm_result = action -> arm();  // 암

    if(arm_result != Action::Result::Success){
        std::cout << "Arming failed: " << arm_result <<std::endl;
        return 1;
    }
    //==============================================================================================
    // 4.takeoff
    //==============================================================================================
    std::cout << "Taking off..." << std::endl;
    const Action::Result takeoff_result = action -> takeoff();  // 이륙
    if(takeoff_result != Action::Result::Success){
        std::cout << "Take off failed: " << takeoff_result << std::endl;
        return 1;
    }
    //==============================================================================================
    // 5.land
    //==============================================================================================
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Landing..." << std::endl;
    const Action::Result land_result = action -> land();    // 랜딩

    if(land_result != Action::Result::Success){
        std::cout << "land failed: " << takeoff_result << std::endl;
        return 1;
    }
    while(telemetry -> in_air()){   // 공중에 있는지 체크
        std::this_thread::sleep_for(std::chrono::seconds(1));   // 주기 1초
    }
    std::cout << "Landed" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "Finished..." << std::endl;

    return 0;
}
