#pragma once

#include <iostream>

namespace upd {
namespace json {

enum class punctuation_type {
  brace_close,
  brace_open,
  bracket_close,
  bracket_open,
  colon,
  comma,
};

template <typename CharReader>
struct lexer {
  lexer(CharReader& char_reader):
    char_reader_(char_reader),
    has_lookahead_(false) {}

  template <typename Handler, typename RetVal>
  RetVal next(Handler& handler) {
    do { next_char_(); } while (good_ && is_whitespace_());
    if (!good_) return handler.end();
    switch (c_) {
      case '[':
        return handler.punctuation(punctuation_type::bracket_open);
      case ']':
        return handler.punctuation(punctuation_type::bracket_close);
      case '{':
        return handler.punctuation(punctuation_type::brace_open);
      case '}':
        return handler.punctuation(punctuation_type::brace_close);
      case ':':
        return handler.punctuation(punctuation_type::colon);
      case ',':
        return handler.punctuation(punctuation_type::comma);
      case '"':
        next_char_();
        return read_string_<Handler, RetVal>(handler);
    }
    if (c_ >= '0' && c_ <= '9') {
      return read_number_<Handler, RetVal>(handler);
    }
    throw std::runtime_error(std::string("unhandled JSON character: `") + c_ + '`');
  }

private:
  template <typename Handler, typename RetVal>
  RetVal read_string_(Handler& handler) {
    std::string value;
    while (good_ && c_ != '"') {
      if (c_ == '\\') {
        next_char_();
        if (!good_) continue;
      }
      value += c_;
      next_char_();
    }
    if (!good_) throw std::runtime_error("unexpected end in string literal");
    return handler.string_literal(value);
  }

  template <typename Handler, typename RetVal>
  RetVal read_number_(Handler& handler) {
    float value = 0;
    do {
      value = value * 10 + (c_ - '0');
      next_char_();
    } while (good_ && c_ >= '0' && c_ <= '9');
    has_lookahead_ = true;
    return handler.number_literal(value);
  }

  void next_char_() {
    if (has_lookahead_) {
      has_lookahead_ = false;
      return;
    }
    good_ = char_reader_.next(c_);
  }

  bool is_whitespace_() {
    return c_ == ' ' || c_ == '\n';
  }

  CharReader char_reader_;
  bool has_lookahead_;
  bool good_;
  char c_;
};

}
}
