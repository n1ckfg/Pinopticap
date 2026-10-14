// Test shim: ofxJSONElement::open without openFrameworks, same jsoncpp as the addon.
#pragma once
#include <fstream>
#include <sstream>
#include "json/json.h"
class ofxJSONElement : public Json::Value {
public:
    bool open(const std::string& filename) {
        std::ifstream in(filename);
        std::stringstream text;
        text << in.rdbuf();
        Json::Reader reader;
        return reader.parse(text.str(), *this);
    }
};
