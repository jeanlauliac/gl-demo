#pragma once

#include <iostream>

namespace json {

enum class TokenType: int {
  boolean_literal,
  brace_close,
  brace_open,
  bracket_close,
  bracket_open,
  colon,
  comma,
  end,
  number_literal,
  string_literal,
};

struct Token {
  TokenType type;
  std::string string_value;
  float number_value;
};

template <size_t BufferSize>
class Lexer {
public:
  Lexer(std::unique_ptr<FILE, decltype(&pclose)>&& file): file_(std::move(file)) {
    next_char_ = fgets(buffer_, BufferSize, file_.get());
    token = read_token_();
  }

  void forward() {
    token = read_token_();
  }

  Token token;

private:
  void read_char_() {
    ++next_char_;
    if (*next_char_ != 0) {
      return;
    }
    next_char_ = fgets(buffer_, BufferSize, file_.get());
  }

  Token read_char_for_(const Token& token) {
    read_char_();
    return token;
  }

  Token read_string_() {
    read_char_();
    std::string value;
    while (next_char_ != nullptr && *next_char_ != '"') {
      value += *next_char_;
      read_char_();
    }
    if (next_char_ == nullptr) {
      throw std::runtime_error("unexpected end in string literal");
    }
    read_char_();
    return {.type = TokenType::string_literal, .string_value = value};
  }

  Token read_number_() {
    float value = 0;
    while (next_char_ != nullptr && *next_char_ >= '0' && *next_char_ <= '9') {
      value = value * 10 + (*next_char_ - '0');
      read_char_();
    }
    if (next_char_ == nullptr) {
      throw std::runtime_error("unexpected end in number literal");
    }
    read_char_();
    return {.type = TokenType::number_literal, .number_value = value};
  }

  Token read_token_() {
    while (next_char_ != nullptr && (
      *next_char_ == ' ' ||
      *next_char_ == '\n'
    )) {
      read_char_();
    }
    if (next_char_ == nullptr) {
      return {.type = TokenType::end};
    }
    char cc = *next_char_;
    switch (cc) {
      case '[':
        return read_char_for_({.type = TokenType::bracket_open});
      case ']':
        return read_char_for_({.type = TokenType::bracket_close});
      case '{':
        return read_char_for_({.type = TokenType::brace_open});
      case '}':
        return read_char_for_({.type = TokenType::brace_close});
      case ':':
        return read_char_for_({.type = TokenType::colon});
      case ',':
        return read_char_for_({.type = TokenType::comma});
    }
    if (cc == '"') {
      return read_string_();
    }
    if (cc >= '0' && cc <= '9') {
      return read_number_();
    }
    throw std::runtime_error(std::string("unknown JSON char: `") + cc + '`');
  }

  char* next_char_;
  char buffer_[BufferSize];
  std::unique_ptr<FILE, decltype(&pclose)> file_;
};

}
