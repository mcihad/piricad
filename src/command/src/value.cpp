// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/value.hpp"

namespace piricad::command {
namespace {

const std::string& empty_text()
{
    static const std::string s;
    return s;
}

const Value::Points& empty_points()
{
    static const Value::Points p;
    return p;
}

const Value::Ints& empty_ids()
{
    static const Value::Ints v;
    return v;
}

core::Json point_json(Point2 p)
{
    core::JsonArray a;
    a.push_back(core::Json::integer(p.x));
    a.push_back(core::Json::integer(p.y));
    return core::Json::array(std::move(a));
}

core::Result<Point2> point_from_json(const core::Json& j)
{
    if (!j.is_array() || j.as_array().size() != 2)
        return core::err(core::ErrorCode::ParseError,
                         "Expected a point as [x_mm, y_mm]; got " + j.dump());
    return Point2{j.as_array()[0].as_int(), j.as_array()[1].as_int()};
}

} // namespace

Value Value::boolean(bool v)
{
    Value x;
    x.kind_ = Kind::Bool;
    x.b_    = v;
    return x;
}

Value Value::integer(std::int64_t v)
{
    Value x;
    x.kind_ = Kind::Int;
    x.i_    = v;
    return x;
}

Value Value::number(double v)
{
    Value x;
    x.kind_ = Kind::Number;
    x.d_    = v;
    return x;
}

Value Value::text(std::string v)
{
    Value x;
    x.kind_ = Kind::Text;
    x.s_    = std::move(v);
    return x;
}

Value Value::point(Point2 v)
{
    Value x;
    x.kind_ = Kind::Point;
    x.pts_  = {v};
    return x;
}

Value Value::points(Points v)
{
    Value x;
    x.kind_ = Kind::PointList;
    x.pts_  = std::move(v);
    return x;
}

Value Value::ids(Ints v)
{
    Value x;
    x.kind_ = Kind::IdList;
    x.ids_  = std::move(v);
    return x;
}

bool Value::as_bool(bool d) const
{
    switch (kind_) {
    case Kind::Bool: return b_;
    case Kind::Int: return i_ != 0;
    case Kind::Number: return d_ != 0.0;
    default: return d;
    }
}

std::int64_t Value::as_int(std::int64_t d) const
{
    switch (kind_) {
    case Kind::Int: return i_;
    case Kind::Bool: return b_ ? 1 : 0;
    case Kind::Number: return static_cast<std::int64_t>(d_ >= 0 ? d_ + 0.5 : d_ - 0.5);
    default: return d;
    }
}

double Value::as_number(double d) const
{
    switch (kind_) {
    case Kind::Number: return d_;
    case Kind::Int: return static_cast<double>(i_);
    case Kind::Bool: return b_ ? 1.0 : 0.0;
    default: return d;
    }
}

const std::string& Value::as_text() const
{
    return kind_ == Kind::Text ? s_ : empty_text();
}

Point2 Value::as_point() const
{
    if ((kind_ == Kind::Point || kind_ == Kind::PointList) && !pts_.empty()) return pts_.front();
    return Point2{};
}

const Value::Points& Value::as_points() const
{
    return (kind_ == Kind::Point || kind_ == Kind::PointList) ? pts_ : empty_points();
}

const Value::Ints& Value::as_ids() const
{
    return kind_ == Kind::IdList ? ids_ : empty_ids();
}

core::Json Value::to_json() const
{
    using core::Json;
    switch (kind_) {
    case Kind::Empty: return Json::null();
    case Kind::Bool: return Json::boolean(b_);
    case Kind::Int: return Json::integer(i_);
    case Kind::Number: return Json::number(d_);
    case Kind::Text: return Json::string(s_);
    case Kind::Point: return point_json(pts_.empty() ? Point2{} : pts_.front());
    case Kind::PointList: {
        core::JsonArray a;
        a.reserve(pts_.size());
        for (auto p : pts_)
            a.push_back(point_json(p));
        return Json::array(std::move(a));
    }
    case Kind::IdList: {
        core::JsonArray a;
        a.reserve(ids_.size());
        for (auto v : ids_)
            a.push_back(Json::integer(v));
        return Json::array(std::move(a));
    }
    }
    return Json::null();
}

core::Result<Value> Value::from_json(const core::Json& j)
{
    using core::ErrorCode;
    using core::Json;

    switch (j.type()) {
    case Json::Type::Null: return Value{};
    case Json::Type::Bool: return Value::boolean(j.as_bool());
    case Json::Type::Int: return Value::integer(j.as_int());
    case Json::Type::Double: return Value::number(j.as_double());
    case Json::Type::String: return Value::text(j.as_string());
    case Json::Type::Object:
        return core::err(ErrorCode::ParseError,
                         "A command argument may not be an object: " + j.dump());
    case Json::Type::Array: break;
    }

    const auto& a = j.as_array();
    if (a.empty()) return Value::points({});

    // [x, y]  -> a single point.  [[x,y], ...] -> a point list.  [n, ...] -> ids.
    if (a.size() == 2 && a[0].is_int() && a[1].is_int())
        return Value::point(Point2{a[0].as_int(), a[1].as_int()});

    if (a.front().is_array()) {
        Points pts;
        pts.reserve(a.size());
        for (const auto& item : a) {
            auto p = point_from_json(item);
            if (!p) return p.error();
            pts.push_back(p.value());
        }
        return Value::points(std::move(pts));
    }

    Ints out;
    out.reserve(a.size());
    for (const auto& item : a) {
        if (!item.is_number())
            return core::err(ErrorCode::ParseError,
                             "Expected a number in id list, got " + item.dump());
        out.push_back(item.as_int());
    }
    return Value::ids(std::move(out));
}

bool operator==(const Value& a, const Value& b)
{
    if (a.kind_ != b.kind_) return false;
    switch (a.kind_) {
    case Value::Kind::Empty: return true;
    case Value::Kind::Bool: return a.b_ == b.b_;
    case Value::Kind::Int: return a.i_ == b.i_;
    case Value::Kind::Number: return a.d_ == b.d_;
    case Value::Kind::Text: return a.s_ == b.s_;
    case Value::Kind::Point:
    case Value::Kind::PointList: return a.pts_ == b.pts_;
    case Value::Kind::IdList: return a.ids_ == b.ids_;
    }
    return false;
}

void Args::set(std::string name, Value v)
{
    for (auto& [k, val] : items_) {
        if (k == name) {
            val = std::move(v);
            return;
        }
    }
    items_.emplace_back(std::move(name), std::move(v));
}

const Value* Args::find(std::string_view name) const
{
    for (const auto& [k, v] : items_)
        if (k == name) return &v;
    return nullptr;
}

Value Args::get(std::string_view name) const
{
    const Value* v = find(name);
    return v ? *v : Value{};
}

core::Json Args::to_json() const
{
    core::JsonObject obj;
    obj.reserve(items_.size());
    for (const auto& [k, v] : items_)
        obj.emplace_back(k, v.to_json());
    return core::Json::object(std::move(obj));
}

core::Result<Args> Args::from_json(const core::Json& j)
{
    if (!j.is_object())
        return core::err(core::ErrorCode::ParseError, "Command arguments must be a JSON object");

    Args a;
    for (const auto& [k, v] : j.as_object()) {
        auto val = Value::from_json(v);
        if (!val) return val.error();
        a.set(k, std::move(val.value()));
    }
    return a;
}

} // namespace piricad::command
