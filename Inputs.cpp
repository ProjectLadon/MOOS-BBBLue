/************************************************************/
/*    NAME:                                               */
/*    ORGN: Project Ladon                                     */
/*    FILE: Inputs.cpp                                   */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <streambuf>
#include <memory>
#include <cstdlib>
#include <cmath>
#include "BBBlue.h"
#include "BBBlue_FunctionBlock.hpp"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/schema.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"

extern "C" {
    #include "roboticscape.h"
}

using namespace std;
using namespace rapidjson;
using namespace BBBL;

ButtonBlock* ButtonBlock::instance() {
    if (!s_instance) s_instance = new ButtonBlock();
    return s_instance;
}

bool ButtonBlock::configure(rapidjson::Value &v) {
    if (v.HasMember("PAUSE") && v["PAUSE"].IsString()) pause = v["PAUSE"].GetString();
    if (v.HasMember("MODE") && v["MODE"].IsString()) mode = v["MODE"].GetString();
    return isConfigured();
}

bool ButtonBlock::tick(BBBlue *b) {
    if (pause != "") {
        if (rc_get_pause_button() == PRESSED) {
            b->notify(pause, 1);
        } else b->notify(pause, 0);
    }
    if (mode != "") {
        if (rc_get_mode_button() == PRESSED) {
            b->notify(mode, 1);
        } else b->notify(mode, 0);
    }
    return true;
}

ACTable ButtonBlock::buildReport() {
    ACTable actab(2);
    actab << "Mode | Pause";
    actab.addHeaderLines();
    actab << rc_get_mode_button();
    actab << rc_get_pause_button();
    return actab;
}

ADCBlock* ADCBlock::instance() {
    if (!s_instance) s_instance = new ADCBlock();
    return s_instance;
}

bool ADCBlock::configure(rapidjson::Value &v) {
    if (v.HasMember("rawChannels") && v["rawChannels"].IsArray()) {
        for (auto &c: v["rawChannels"].GetArray()) {
            if (c.IsString()) {
                rawChannels.push_back(c.GetString());
            } else {rawChannels.push_back("");}
        }
    }
    if (v.HasMember("voltChannels") && v["voltChannels"].IsArray()) {
        for (auto &c: v["rawChannels"].GetArray()) {
            if (c.IsString()) {
                voltChannels.push_back(c.GetString());
            } else {rawChannels.push_back("");}
        }
    }
    return isConfigured();
}

bool ADCBlock::tick(BBBlue *b) {
    int ch = 0;
    for (auto &c: rawChannels) {
        if (c != "") b->notify(c, rc_adc_raw(ch));
        ch++;
    }
    for (auto &c: voltChannels) {
        if (c != "") b->notify(c, rc_adc_volt(ch));
        ch++;
    }
    b->notify("BBBL_BATTERY_VOLTAGE", rc_battery_voltage());
    b->notify("BBBL_JACK_VOLTAGE", rc_dc_jack_voltage());
}

bool ADCBlock::isConfigured() {
    for (auto &c: rawChannels) if (c != "") return true;
    for (auto &c: voltChannels) if (c != "") return true;
    return false;
}

ACTable ADCBlock::buildReport() {
    ACTable actab(6);
    actab << "CH0|CH1|CH2|CH3|Jack|Battery";
    actab.addHeaderLines();
    actab << to_string(rc_adc_volt(0));
    actab << to_string(rc_adc_volt(1));
    actab << to_string(rc_adc_volt(2));
    actab << to_string(rc_adc_volt(3));
    actab << to_string(rc_dc_jack_voltage());
    actab << to_string(rc_battery_voltage());
    actab << to_string(rc_adc_raw(0));
    actab << to_string(rc_adc_raw(1));
    actab << to_string(rc_adc_raw(2));
    actab << to_string(rc_adc_raw(3));
    return actab;
}

BaroBlock* BaroBlock::instance() {
    if (!s_instance) s_instance = new BaroBlock();
    return s_instance;
}

bool BaroBlock::configure(rapidjson::Value &v) {
    rc_bmp_oversample_t oversample = BMP_OVERSAMPLE_1;
    rc_bmp_filter_t filter = BMP_FILTER_OFF;
    if (v.HasMember("oversample") && v["oversample"].IsInt()) {
        switch (v["oversample"].GetInt()) {
            case 1:
                oversample = BMP_OVERSAMPLE_1;
                break;
            case 2:
                oversample = BMP_OVERSAMPLE_2;
                break;
            case 4:
                oversample = BMP_OVERSAMPLE_4;
                break;
            case 8:
                oversample = BMP_OVERSAMPLE_8;
                break;
            case 16:
                oversample = BMP_OVERSAMPLE_16;
                break;
            default:
                return false;
        }
        configured = true;
    }
    if (v.HasMember("filter") && v["filter"].IsInt()) {
        switch (v["filter"].GetInt()) {
            case 0:
                filter = BMP_FILTER_OFF;
                break;
            case 2:
                filter = BMP_FILTER_2;
                break;
            case 4:
                filter = BMP_FILTER_4;
                break;
            case 8:
                filter = BMP_FILTER_8;
                break;
            case 16:
                filter = BMP_FILTER_16;
                break;
            default:
                return false;
        }
        configured = true;
    }
    if (configured) {
        if (rc_initialize_barometer(oversample, filter)) {
            configured = false;
            return false;
        }
        return true;
    }
    return false;
}

bool BaroBlock::procMail(CMOOSMsg &msg) {
    if ((msg.GetKey() == "BBBL_BARO_SEA_LEVEL") && msg.IsDouble()) {
        seaLevelPressure = msg.GetDouble();
        return true;
    }
    return false;
}

bool BaroBlock::tick(BBBlue *b) {
    b->notify("BBBL_BARO_TEMP", rc_bmp_get_temperature());
    b->notify("BBBL_BARO_PRES", rc_bmp_get_pressure_pa());
    b->notify("BBBL_BARO_ALT", rc_bmp_get_altitude_m());
    return true;
}

bool BaroBlock::subscribe(BBBlue *b) {
    b->registerVar("BBBL_BARO_SEA_LEVEL");
    return true;
}

ACTable BaroBlock::buildReport() {
    ACTable actab(4);
    actab << "SeaLevel (Pa)|Ambient (Pa)|Altitude (m)|Temp (C)";
    actab.addHeaderLines();
    actab << to_string(seaLevelPressure);
    actab << to_string(rc_bmp_get_pressure_pa());
    actab << to_string(rc_bmp_get_altitude_m());
    actab << to_string(rc_bmp_get_temperature());

}

EncodersBlock* EncodersBlock::instance() {
    if (!s_instance) s_instance = new EncodersBlock();
    return s_instance;
}

bool EncodersBlock::configure(rapidjson::Value &v) {
    if (v.IsArray()) {
        int cnt = 1;
        for (auto &e : v.GetArray()) {
            if (e.IsString()) {
                encoders.push_back(e.GetString());
            } else if (e.IsObject()) {
                encoders.push_back(e["name"].GetString());
                if (e.HasMember("init")) rc_set_encoder_pos(cnt, e["init"].GetInt());
            } else encoders.push_back("");
            cnt++;
        }
        return isConfigured();
    }
    return false;
}

bool EncodersBlock::tick(BBBlue *b) {
    int cnt = 1;
    for (auto &e: encoders) {
        if (e != "") {
            b->notify(e, rc_get_encoder_pos(cnt));
        }
        cnt++;
    }
    return true;
}

bool EncodersBlock::isConfigured() {
    for (auto &s: encoders) if (s != "") return true;
    return false;
}

ACTable EncodersBlock::buildReport() {
    ACTable actab(4);
    actab << "E1|E2|E3|E4";
    for (int i = 1; i < 5; i++) actab << to_string(rc_get_encoder_pos(i));
    return actab;
}

EncodersBlock* EncodersBlock::s_instance;
ButtonBlock* ButtonBlock::s_instance;
ADCBlock* ADCBlock::s_instance;
BaroBlock* BaroBlock::s_instance;
