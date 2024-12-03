#pragma once

#include <boost/json.hpp>

#include <iostream>

namespace json = boost::json;

enum LootTypes {
    KEY = 0,
    WALLET = 1
};

class Loot {
public:
    explicit Loot(json::object loot_object)
    : loot_object_(loot_object)
    {
        if (loot_object.at("name").as_string() == "key") {
            type_ = 0;
        } else if (loot_object.at("name").as_string() == "wallet") {
            type_ = 1;
        }

        rate_ = loot_object.at("value").as_int64();
    }

    int GetLootType() const {
        return type_;
    }

    void SetLootType(int type) {
        type_ = type;
    }

    int GetRate() const {
        return rate_;
    }

    void SetRate(int rate) {
        rate_ = rate;
    }

    json::object GetLootObject() const {
        return loot_object_;
    }

    template <typename Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar& type_;
        ar& rate_;
        std::string obj = boost::json::serialize(loot_object_);
        ar& obj;
    }

private:
    int type_;
    int rate_;
    json::object loot_object_;
};