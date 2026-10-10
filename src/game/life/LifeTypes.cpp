#include "outland/game/life/LifeTypes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace outland::game::life {
WorldClock WorldClock::at(int day,int hour,int minute) {return {static_cast<double>(day)*1440+hour*60+minute};}
int WorldClock::day() const {return static_cast<int>(std::floor(minutes/1440));}
Weekday WorldClock::weekday() const {return static_cast<Weekday>(((day()%7)+7)%7);}
int WorldClock::minute_of_day() const {return static_cast<int>(minutes-static_cast<double>(day())*1440);}
float WorldClock::hour() const {return static_cast<float>((minutes-static_cast<double>(day())*1440)/60);}
bool WorldClock::night() const {const float h=hour();return h<6 || h>=21;}
void WorldClock::advance(float real_seconds,float time_scale) {
    if(std::isfinite(real_seconds) && std::isfinite(time_scale) && real_seconds>0 && time_scale>0)
        minutes+=static_cast<double>(real_seconds)*time_scale/60;
}
std::string WorldClock::label() const {
    static constexpr const char* names[]{"MON","TUE","WED","THU","FRI","SAT","SUN"};
    const int m=minute_of_day();
    char text[16];std::snprintf(text,sizeof text,"%s %02d:%02d",names[static_cast<int>(weekday())],m/60,m%60);
    return text;
}
DayType day_type(Weekday day) {
    return day==Weekday::Sunday ? DayType::Sunday : day==Weekday::Saturday ? DayType::Saturday : DayType::Weekday;
}
const char* place_name(PlaceKind kind) {
    switch(kind) {
        case PlaceKind::Home:return "home";case PlaceKind::Garage:return "garage";
        case PlaceKind::Industrial:return "works";case PlaceKind::Dock:return "docks";
        case PlaceKind::Shop:return "shop";case PlaceKind::Pub:return "pub";
        case PlaceKind::Church:return "church";case PlaceKind::Police:return "police station";
        case PlaceKind::Clinic:return "clinic";case PlaceKind::Office:return "town hall";
        case PlaceKind::Farm:return "fields";case PlaceKind::Square:return "square";
    }
    return "place";
}
const char* occupation_name(Occupation occupation) {
    switch(occupation) {
        case Occupation::Mechanic:return "mechanic";case Occupation::Dockworker:return "dockworker";
        case Occupation::FactoryWorker:return "factory worker";case Occupation::Shopkeeper:return "shopkeeper";
        case Occupation::Publican:return "publican";case Occupation::Pastor:return "pastor";
        case Occupation::PoliceOfficer:return "police officer";case Occupation::Doctor:return "doctor";
        case Occupation::Clerk:return "clerk";case Occupation::Farmer:return "farmer";
        case Occupation::Retired:return "retired";case Occupation::Unemployed:return "out of work";
    }
    return "resident";
}
const char* faction_name(Faction faction) {
    switch(faction) {
        case Faction::Police:return "Police";case Faction::Krimulo:return "Krimulo";
        case Faction::GreenStar:return "Green Star Federation";default:return "none";
    }
}
const char* activity_name(Activity activity) {
    switch(activity) {
        case Activity::Sleep:return "asleep";case Activity::Home:return "at home";
        case Activity::Work:return "working";case Activity::Lunch:return "having lunch";
        case Activity::Socialize:return "at the pub";case Activity::Worship:return "at church";
        case Activity::Shopping:return "shopping";case Activity::Visit:return "visiting family";
        case Activity::Stroll:return "out for a walk";case Activity::Patrol:return "on patrol";
        case Activity::FactionMeet:return "out late";case Activity::Shelter:return "sheltering";
    }
    return "about";
}
}
