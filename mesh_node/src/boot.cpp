#include "boot.hpp"
#include <WDT.h>
#include "mqtt_client.hpp"
#include "time_keeper.hpp"

SystemState systemState;

void boot_init(){
    systemState = BOOT;
}

void boot_loop(){

    switch(systemState){
    
        case BOOT:
            if(mqttClient.connected()){
                systemState = INIT_TIME;
            }
            break;

        case INIT_TIME:
        TimeInit();
        systemState = INIT_WDT;
        break;

        case INIT_WDT:
            if(WDT.begin(4096)){
                systemState = RUNNING;
            }
            break;
        case RUNNING:
            break;
    }
}