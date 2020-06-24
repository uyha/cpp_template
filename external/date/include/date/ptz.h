#ifndef PTZ_H
#define PTZ_H

// The MIT License (MIT)
//
// Copyright (c) 2017 Howard Hinnant
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// This header allows Posix-style time zones as specified for TZ here:
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03
//
// Posix::time_zone can be constructed with a posix-style string and then used in
// a zoned_time like so:
//
// zoned_time<system_clock::duration, Posix::time_zone> zt{"EST5EDT,M3.2.0,M11.1.0",
//                                                         system_clock::now()};
// or:
//
// Posix::time_zone tz{"EST5EDT,M3.2.0,M11.1.0"};
// zoned_time<system_clock::duration, Posix::time_zone> zt{tz, system_clock::now()};
//
// Note, Posix-style time zones are not recommended for all of the reasons described here:
// https://stackoverflow.com/tags/timezone/info
//
// They are provided here as a non-trivial custom time zone example, and if you really
// have to have Posix time zones, you're welcome to use this one.

#include "date/tz.h"

#include <cctype>
#include <ostream>
#include <string>

namespace Posix {

namespace detail {

#if HAS_STRING_VIEW

using string_t = std::string_view;

#else // !HAS_STRING_VIEW

using string_t = std::string;

#endif // !HAS_STRING_VIEW

class rule;

void throw_invalid(const string_t &s, unsigned i, const string_t &message);
unsigned read_date(const string_t &s, unsigned i, rule &r);
unsigned read_name(const string_t &s, unsigned i, std::string &name);
unsigned read_signed_time(const string_t &s, unsigned i, std::chrono::seconds &t);
unsigned read_unsigned_time(const string_t &s, unsigned i, std::chrono::seconds &t);
unsigned read_unsigned(const string_t &s, unsigned i, unsigned limit, unsigned &u);

class rule {
  enum { off, J, M, N };

  date::month m_;
  date::weekday wd_;
  unsigned short n_ : 14;
  unsigned short mode_ : 2;
  std::chrono::duration<std::int32_t> time_ = std::chrono::hours{2};

public:
  rule()
      : mode_(off) {}

  bool ok() const {
    return mode_ != off;
  }
  date::local_seconds operator()(date::year y) const;
  std::string to_string() const;

  friend std::ostream &operator<<(std::ostream &os, const rule &r);
  friend unsigned read_date(const string_t &s, unsigned i, rule &r);
  friend bool operator==(const rule &x, const rule &y);
};

inline bool operator==(const rule &x, const rule &y) {
  if (x.mode_ != y.mode_)
    return false;
  switch (x.mode_) {
  case rule::J:
  case rule::N:
    return x.n_ == y.n_;
  case rule::M:
    return x.m_ == y.m_ && x.n_ == y.n_ && x.wd_ == y.wd_;
  default:
    return true;
  }
}

inline bool operator!=(const rule &x, const rule &y) {
  return !(x == y);
}

inline date::local_seconds rule::operator()(date::year y) const {
  using date::days;
  using date::January;
  using date::last;
  using date::local_days;
  using sec = std::chrono::seconds;
  date::local_seconds t;
  switch (mode_) {
  case J:
    t = local_days{y / January / 0} + days{n_ + (y.is_leap() && n_ > 59)} + sec{time_};
    break;
  case M:
    t = (n_ == 5 ? local_days{y / m_ / wd_[last]} : local_days{y / m_ / wd_[n_]}) + sec{time_};
    break;
  case N:
    t = local_days{y / January / 1} + days{n_} + sec{time_};
    break;
  default:
    assert(!"rule called with bad mode");
  }
  return t;
}

inline std::string rule::to_string() const {
  using namespace std::chrono;
  auto print_offset = [](seconds off) {
    std::string nm;
    if (off != hours{2}) {
      date::hh_mm_ss<seconds> offset{off};
      nm = '/';
      nm += std::to_string(offset.hours().count());
      if (offset.minutes() != minutes{0} || offset.seconds() != seconds{0}) {
        nm += ':';
        if (offset.minutes() < minutes{10})
          nm += '0';
        nm += std::to_string(offset.minutes().count());
        if (offset.seconds() != seconds{0}) {
          nm += ':';
          if (offset.seconds() < seconds{10})
            nm += '0';
          nm += std::to_string(offset.seconds().count());
        }
      }
    }
    return nm;
  };

  std::string nm;
  switch (mode_) {
  case rule::J:
    nm = 'J';
    nm += std::to_string(n_);
    break;
  case rule::M:
    nm = 'M';
    nm += std::to_string(static_cast<unsigned>(m_));
    nm += '.';
    nm += std::to_string(n_);
    nm += '.';
    nm += std::to_string(wd_.c_encoding());
    break;
  case rule::N:
    nm = std::to_string(n_);
    break;
  default:
    break;
  }
  nm += print_offset(time_);
  return nm;
}

inline std::ostream &operator<<(std::ostream &os, const rule &r) {
  switch (r.mode_) {
  case rule::J:
    os << 'J' << r.n_ << date::format(" %T", r.time_);
    break;
  case rule::M:
    if (r.n_ == 5)
      os << r.m_ / r.wd_[date::last];
    else
      os << r.m_ / r.wd_[r.n_];
    os << date::format(" %T", r.time_);
    break;
  case rule::N:
    os << r.n_ << date::format(" %T", r.time_);
    break;
  default:
    break;
  }
  return os;
}

} // namespace detail

class time_zone {
  std::string std_abbrev_;
  std::string dst_abbrev_ = {};
  std::chrono::seconds offset_;
  std::chrono::seconds save_ = std::chrono::hours{1};
  detail::rule start_rule_;
  detail::rule end_rule_;

public:
  explicit time_zone(const detail::string_t &name);

  template <class Duration>
  date::sys_info get_info(date::sys_time<Duration> st) const;
  template <class Duration>
  date::local_info get_info(date::local_time<Duration> tp) const;

  template <class Duration>
  date::sys_time<typename std::common_type<Duration, std::chrono::seconds>::type>
  to_sys(date::local_time<Duration> tp) const;

  template <class Duration>
  date::sys_time<typename std::common_type<Duration, std::chrono::seconds>::type>
  to_sys(date::local_time<Duration> tp, date::choose z) const;

  template <class Duration>
  date::local_time<typename std::common_type<Duration, std::chrono::seconds>::type>
  to_local(date::sys_time<Duration> tp) const;

  friend std::ostream &operator<<(std::ostream &os, const time_zone &z);

  const time_zone *operator->() const {
    return this;
  }

  std::string name() const;

  friend bool operator==(const time_zone &x, const time_zone &y);
};

inline time_zone::time_zone(const detail::string_t &s) {
  using detail::read_name;
  using detail::read_signed_time;
  using detail::throw_invalid;
  auto i  = read_name(s, 0, std_abbrev_);
  i       = read_signed_time(s, i, offset_);
  offset_ = -offset_;
  if (i != s.size()) {
    i = read_name(s, i, dst_abbrev_);
    if (i != s.size()) {
      if (s[i] != ',') {
        i     = read_signed_time(s, i, save_);
        save_ = -save_ - offset_;
      }
      if (i != s.size()) {
        if (s[i] != ',')
          throw_invalid(s, i, "Expecting end of string or ',' to start rule");
        ++i;
        i = read_date(s, i, start_rule_);
        if (i == s.size() || s[i] != ',')
          throw_invalid(s, i, "Expecting ',' and then the ending rule");
        ++i;
        i = read_date(s, i, end_rule_);
        if (i != s.size())
          throw_invalid(s, i, "Found unexpected trailing characters");
      }
    }
  }
}

template <class Duration>
date::sys_info time_zone::get_info(date::sys_time<Duration> st) const {
  using date::ceil;
  using date::days;
  using date::December;
  using date::floor;
  using date::January;
  using date::last;
  using date::sys_days;
  using date::sys_info;
  using date::sys_seconds;
  using date::year;
  using date::year_month_day;
  using date::years;
  using std::chrono::minutes;
  sys_info r{};
  r.offset = offset_;
  if (start_rule_.ok()) {
    auto y     = year_month_day{floor<days>(st)}.year();
    auto start = sys_seconds{(start_rule_(y) - offset_).time_since_epoch()};
    auto end   = sys_seconds{(end_rule_(y) - (offset_ + save_)).time_since_epoch()};
    if (start <= st && st < end) {
      r.begin = start;
      r.end   = end;
      r.offset += save_;
      r.save   = ceil<minutes>(save_);
      r.abbrev = dst_abbrev_;
    } else if (st < start) {
      r.begin  = sys_seconds{(end_rule_(y - years{1}) - (offset_ + save_)).time_since_epoch()};
      r.end    = start;
      r.abbrev = std_abbrev_;
    } else // st >= end
    {
      r.begin  = end;
      r.end    = sys_seconds{(start_rule_(y + years{1}) - offset_).time_since_epoch()};
      r.abbrev = std_abbrev_;
    }
  } else //  constant offset
  {
    r.begin  = sys_days{year::min() / January / 1};
    r.end    = sys_days{year::max() / December / last};
    r.abbrev = std_abbrev_;
  }
  return r;
}

template <class Duration>
date::local_info time_zone::get_info(date::local_time<Duration> tp) const {
  using date::ceil;
  using date::days;
  using date::December;
  using date::January;
  using date::last;
  using date::local_info;
  using date::sys_days;
  using date::sys_seconds;
  using date::year;
  using date::year_month_day;
  using date::years;
  using std::chrono::minutes;
  using std::chrono::seconds;
  local_info r{};
  using date::floor;
  if (start_rule_.ok()) {
    auto y     = year_month_day{floor<days>(tp)}.year();
    auto start = sys_seconds{(start_rule_(y) - offset_).time_since_epoch()};
    auto end   = sys_seconds{(end_rule_(y) - (offset_ + save_)).time_since_epoch()};
    auto utcs  = sys_seconds{floor<seconds>(tp - offset_).time_since_epoch()};
    auto utcd  = sys_seconds{floor<seconds>(tp - (offset_ + save_)).time_since_epoch()};
    if ((utcs < start) != (utcd < start)) {
      r.first.begin = sys_seconds{(end_rule_(y - years{1}) - (offset_ + save_)).time_since_epoch()};
      r.first.end   = start;
      r.first.offset  = offset_;
      r.first.abbrev  = std_abbrev_;
      r.second.begin  = start;
      r.second.end    = end;
      r.second.abbrev = dst_abbrev_;
      r.second.offset = offset_ + save_;
      r.second.save   = ceil<minutes>(save_);
      r.result        = save_ > seconds{0} ? local_info::nonexistent : local_info::ambiguous;
    } else if ((utcs < end) != (utcd < end)) {
      r.first.begin   = start;
      r.first.end     = end;
      r.first.offset  = offset_ + save_;
      r.first.save    = ceil<minutes>(save_);
      r.first.abbrev  = dst_abbrev_;
      r.second.begin  = end;
      r.second.end    = sys_seconds{(start_rule_(y + years{1}) - offset_).time_since_epoch()};
      r.second.abbrev = std_abbrev_;
      r.second.offset = offset_;
      r.result        = save_ > seconds{0} ? local_info::ambiguous : local_info::nonexistent;
    } else if (utcs < start) {
      r.first.begin = sys_seconds{(end_rule_(y - years{1}) - (offset_ + save_)).time_since_epoch()};
      r.first.end   = start;
      r.first.offset = offset_;
      r.first.abbrev = std_abbrev_;
    } else if (utcs < end) {
      r.first.begin  = start;
      r.first.end    = end;
      r.first.offset = offset_ + save_;
      r.first.save   = ceil<minutes>(save_);
      r.first.abbrev = dst_abbrev_;
    } else {
      r.first.begin  = end;
      r.first.end    = sys_seconds{(start_rule_(y + years{1}) - offset_).time_since_epoch()};
      r.first.abbrev = std_abbrev_;
      r.first.offset = offset_;
    }
  } else //  constant offset
  {
    r.first.begin  = sys_days{year::min() / January / 1};
    r.first.end    = sys_days{year::max() / December / last};
    r.first.abbrev = std_abbrev_;
    r.first.offset = offset_;
  }
  return r;
}

template <class Duration>
date::sys_time<typename std::common_type<Duration, std::chrono::seconds>::type>
time_zone::to_sys(date::local_time<Duration> tp) const {
  using date::ambiguous_local_time;
  using date::local_info;
  using date::sys_time;
  auto i = get_info(tp);
  if (i.result == local_info::nonexistent)
    throw nonexistent_local_time(tp, i);
  else if (i.result == local_info::ambiguous)
    throw ambiguous_local_time(tp, i);
  return sys_time<Duration>{tp.time_since_epoch()} - i.first.offset;
}

template <class Duration>
date::sys_time<typename std::common_type<Duration, std::chrono::seconds>::type>
time_zone::to_sys(date::local_time<Duration> tp, date::choose z) const {
  using date::choose;
  using date::local_info;
  using date::sys_time;
  auto i = get_info(tp);
  if (i.result == local_info::nonexistent) {
    return i.first.end;
  } else if (i.result == local_info::ambiguous) {
    if (z == choose::latest)
      return sys_time<Duration>{tp.time_since_epoch()} - i.second.offset;
  }
  return sys_time<Duration>{tp.time_since_epoch()} - i.first.offset;
}

template <class Duration>
date::local_time<typename std::common_type<Duration, std::chrono::seconds>::type>
time_zone::to_local(date::sys_time<Duration> tp) const {
  using date::local_time;
  using std::chrono::seconds;
  using LT = local_time<typename std::common_type<Duration, seconds>::type>;
  auto i   = get_info(tp);
  return LT{(tp + i.offset).time_since_epoch()};
}

inline std::ostream &operator<<(std::ostream &os, const time_zone &z) {
  using date::operator<<;
  os << '{';
  os << z.std_abbrev_ << ", " << z.dst_abbrev_ << date::format(", %T, ", z.offset_)
     << date::format("%T, [", z.save_) << z.start_rule_ << ", " << z.end_rule_ << ")}";
  return os;
}

inline std::string time_zone::name() const {
  using namespace date;
  using namespace std::chrono;
  auto nm           = std_abbrev_;
  auto print_offset = [](seconds off) {
    std::string nm;
    hh_mm_ss<seconds> offset{-off};
    if (offset.is_negative())
      nm += '-';
    nm += std::to_string(offset.hours().count());
    if (offset.minutes() != minutes{0} || offset.seconds() != seconds{0}) {
      nm += ':';
      if (offset.minutes() < minutes{10})
        nm += '0';
      nm += std::to_string(offset.minutes().count());
      if (offset.seconds() != seconds{0}) {
        nm += ':';
        if (offset.seconds() < seconds{10})
          nm += '0';
        nm += std::to_string(offset.seconds().count());
      }
    }
    return nm;
  };
  nm += print_offset(offset_);
  if (!dst_abbrev_.empty()) {
    nm += dst_abbrev_;
    if (save_ != hours{1})
      nm += print_offset(offset_ + save_);
    if (start_rule_.ok()) {
      nm += ',';
      nm += start_rule_.to_string();
      nm += ',';
      nm += end_rule_.to_string();
    }
  }
  return nm;
}

inline bool operator==(const time_zone &x, const time_zone &y) {
  return x.std_abbrev_ == y.std_abbrev_ && x.dst_abbrev_ == y.dst_abbrev_ &&
         x.offset_ == y.offset_ && x.save_ == y.save_ && x.start_rule_ == y.start_rule_ &&
         x.end_rule_ == y.end_rule_;
}

inline bool operator!=(const time_zone &x, const time_zone &y) {
  return !(x == y);
}

namespace detail {

inline void throw_invalid(const string_t &s, unsigned i, const string_t &message) {
  throw std::runtime_error(std::string("Invalid time_zone initializer.\n") + std::string(message) +
                           ":\n" + std::string(s) + '\n' + "\x1b[1;32m" + std::string(i, '~') +
                           '^' + std::string(s.size() - i - 1, '~') + "\x1b[0m");
}

inline unsigned read_date(const string_t &s, unsigned i, rule &r) {
  using date::month;
  using date::weekday;
  if (i == s.size())
    throw_invalid(s, i, "Expected rule but found end of string");
  if (s[i] == 'J') {
    ++i;
    unsigned n;
    i       = read_unsigned(s, i, 3, n);
    r.mode_ = rule::J;
    r.n_    = n;
  } else if (s[i] == 'M') {
    ++i;
    unsigned m;
    i = read_unsigned(s, i, 2, m);
    if (i == s.size() || s[i] != '.')
      throw_invalid(s, i, "Expected '.' after month");
    ++i;
    unsigned n;
    i = read_unsigned(s, i, 1, n);
    if (i == s.size() || s[i] != '.')
      throw_invalid(s, i, "Expected '.' after weekday index");
    ++i;
    unsigned wd;
    i       = read_unsigned(s, i, 1, wd);
    r.mode_ = rule::M;
    r.m_    = month{m};
    r.wd_   = weekday{wd};
    r.n_    = n;
  } else if (std::isdigit(s[i])) {
    unsigned n;
    i       = read_unsigned(s, i, 3, n);
    r.mode_ = rule::N;
    r.n_    = n;
  } else
    throw_invalid(s, i, "Expected 'J', 'M', or a digit to start rule");
  if (i != s.size() && s[i] == '/') {
    ++i;
    std::chrono::seconds t;
    i       = read_unsigned_time(s, i, t);
    r.time_ = t;
  }
  return i;
}

inline unsigned read_name(const string_t &s, unsigned i, std::string &name) {
  if (i == s.size())
    throw_invalid(s, i, "Expected a name but found end of string");
  if (s[i] == '<') {
    ++i;
    while (true) {
      if (i == s.size())
        throw_invalid(s, i, "Expected to find closing '>', but found end of string");
      if (s[i] == '>')
        break;
      name.push_back(s[i]);
      ++i;
    }
    ++i;
  } else {
    while (i != s.size() && std::isalpha(s[i])) {
      name.push_back(s[i]);
      ++i;
    }
  }
  if (name.size() < 3)
    throw_invalid(s, i, "Found name to be shorter than 3 characters");
  return i;
}

inline unsigned read_signed_time(const string_t &s, unsigned i, std::chrono::seconds &t) {
  if (i == s.size())
    throw_invalid(s, i, "Expected to read signed time, but found end of string");
  bool negative = false;
  if (s[i] == '-') {
    negative = true;
    ++i;
  } else if (s[i] == '+')
    ++i;
  i = read_unsigned_time(s, i, t);
  if (negative)
    t = -t;
  return i;
}

inline unsigned read_unsigned_time(const string_t &s, unsigned i, std::chrono::seconds &t) {
  using std::chrono::hours;
  using std::chrono::minutes;
  using std::chrono::seconds;
  if (i == s.size())
    throw_invalid(s, i, "Expected to read unsigned time, but found end of string");
  unsigned x;
  i = read_unsigned(s, i, 2, x);
  t = hours{x};
  if (i != s.size() && s[i] == ':') {
    ++i;
    i = read_unsigned(s, i, 2, x);
    t += minutes{x};
    if (i != s.size() && s[i] == ':') {
      ++i;
      i = read_unsigned(s, i, 2, x);
      t += seconds{x};
    }
  }
  return i;
}

inline unsigned read_unsigned(const string_t &s, unsigned i, unsigned limit, unsigned &u) {
  if (i == s.size() || !std::isdigit(s[i]))
    throw_invalid(s, i, "Expected to find a decimal digit");
  u              = static_cast<unsigned>(s[i] - '0');
  unsigned count = 1;
  for (++i; count < limit && i != s.size() && std::isdigit(s[i]); ++i, ++count)
    u = u * 10 + static_cast<unsigned>(s[i] - '0');
  return i;
}

} // namespace detail

} // namespace Posix

namespace date {

template <>
struct zoned_traits<Posix::time_zone> {
#if HAS_STRING_VIEW

  static Posix::time_zone locate_zone(std::string_view name) {
    return Posix::time_zone{name};
  }

#else // !HAS_STRING_VIEW

  static Posix::time_zone locate_zone(const std::string &name) {
    return Posix::time_zone{name};
  }

  static Posix::time_zone locate_zone(const char *name) {
    return Posix::time_zone{name};
  }

#endif // !HAS_STRING_VIEW
};

} // namespace date

#endif // PTZ_H
