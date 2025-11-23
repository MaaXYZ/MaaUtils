#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <variant>

#include <meojson/json.hpp>

#include "MaaUtils/NoWarningCVMat.hpp"

#include "MaaUtils/Encoding.h"
#include "MaaUtils/Platform.h"

namespace json::ext
{
template <>
class jsonization<std::wstring>
{
public:
    json::value to_json(const std::wstring& wstr) const { return MAA_NS::from_u16(wstr); }

    bool check_json(const json::value& json) const { return json.is_string(); }

    bool from_json(const json::value& json, std::wstring& wstr) const
    {
        wstr = MAA_NS::to_u16(json.as_string());
        return true;
    }
};

template <>
class jsonization<std::filesystem::path>
{
public:
    json::value to_json(const std::filesystem::path& path) const { return MAA_NS::path_to_utf8_string(path); }

    bool check_json(const json::value& json) const { return json.is_string(); }

    bool from_json(const json::value& json, std::filesystem::path& path) const
    {
        path = MAA_NS::path(json.as_string());
        return true;
    }
};

template <>
class jsonization<std::chrono::milliseconds>
{
public:
    json::value to_json(const std::chrono::milliseconds& ms) const { return std::format("{}ms", ms.count()); }

    bool check_json(const json::value&) const { return false; }

    bool from_json(const json::value&, const std::chrono::milliseconds&) const { return false; }
};

template <>
class jsonization<cv::Point>
{
public:
    json::value to_json(const cv::Point& point) const { return json::array { point.x, point.y }; }

    bool check_json(const json::value& json) const { return json.is<std::vector<int>>() && json.as_array().size() == 2; }

    bool from_json(const json::value& json, cv::Point& point) const
    {
        auto arr = json.as<std::vector<int>>();
        point = cv::Point(arr[0], arr[1]);
        return true;
    }
};

template <>
class jsonization<cv::Rect>
{
public:
    json::value to_json(const cv::Rect& rect) const { return json::array { rect.x, rect.y, rect.width, rect.height }; }

    bool check_json(const json::value& json) const { return json.is<std::vector<int>>() && json.as_array().size() == 4; }

    bool from_json(const json::value& json, cv::Rect& rect) const
    {
        auto arr = json.as<std::vector<int>>();
        rect = cv::Rect(arr[0], arr[1], arr[2], arr[3]);
        return true;
    }
};

template <>
class jsonization<cv::Size>
{
public:
    json::value to_json(const cv::Size& size) const { return json::array { size.width, size.height }; }

    bool check_json(const json::value& json) const { return json.is<std::vector<int>>() && json.as_array().size() == 2; }

    bool from_json(const json::value& json, cv::Size& size) const
    {
        auto arr = json.as<std::vector<int>>();
        size = cv::Size(arr[0], arr[1]);
        return true;
    }
};

template <>
class jsonization<cv::Mat>
{
public:
    json::value to_json(const cv::Mat& mat) const { return json::array { mat.rows, mat.cols, mat.type() }; }

    // bool check_json(const json::value&) const { return false; }

    // bool from_json(const json::value&, cv::Mat&) const { return false; }
};

template <typename T>
concept has_output_operator = requires { std::declval<std::ostream&>() << std::declval<T>(); };

template <has_output_operator T>
requires(!std::is_constructible_v<T, json::value> && !std::is_constructible_v<T, json::array> && !std::is_constructible_v<T, json::object>)
class jsonization<T>
{
public:
    json::value to_json(const T& value) const
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    // bool check_json(const json::value&) const { return false; }

    // bool from_json(const json::value&, T&) const { return false; }
};
} // namespace json::ext
