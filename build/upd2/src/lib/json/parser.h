#pragma once

#include "lexer.h"

namespace upd {
namespace json {

template <typename Lexer, typename Handler, typename RetVal>
RetVal parse_expression(Lexer& lexer, Handler& handler);

struct unexpected_end_error {};
struct unexpected_number_error {};
struct unexpected_string_error {};
struct unexpected_punctuation_error {};
struct object_reader_invalid {};

struct read_field_name_handler {
  read_field_name_handler(std::string& field_name): field_name_(field_name) {}

  bool end() const { throw unexpected_end_error(); }

  bool punctuation(punctuation_type type) const {
    if (type == punctuation_type::brace_close) return false;
    throw unexpected_punctuation_error();
  }

  bool string_literal(const std::string& literal) const {
    field_name_ = literal;
    return true;
  }

  bool number_literal(float literal) const {
    throw unexpected_number_error();
  }

private:
  std::string& field_name_;
};

struct read_new_field_name_handler {
  read_new_field_name_handler(std::string& field_name): field_name_(field_name) {}

  bool end() const { throw unexpected_end_error(); }

  bool punctuation(punctuation_type type) const {
    throw unexpected_punctuation_error();
  }

  bool string_literal(const std::string& literal) const {
    field_name_ = literal;
    return true;
  }

  bool number_literal(float literal) const {
    throw unexpected_number_error();
  }

private:
  std::string& field_name_;
};

struct post_field_handler {
  bool end() const { throw unexpected_end_error(); }

  bool punctuation(punctuation_type type) const {
    if (type == punctuation_type::brace_close) return false;
    if (type == punctuation_type::comma) return true;
    throw unexpected_punctuation_error();
  }

  bool string_literal(const std::string& literal) const {
    throw unexpected_string_error();
  }

  bool number_literal(float literal) const {
    throw unexpected_number_error();
  }
};

struct read_field_colon_handler {
  void end() const { throw unexpected_end_error(); }

  void punctuation(punctuation_type type) const {
    if (type != punctuation_type::colon) {
      throw unexpected_punctuation_error();
    }
  }

  void string_literal(const std::string& literal) const {
    throw unexpected_string_error();
  }

  void number_literal(float literal) const {
    throw unexpected_number_error();
  }
};

struct field_value_reading_proof {
  field_value_reading_proof(): token(this) {}
  const void* token;
};

bool operator==(
  const field_value_reading_proof& left,
  const field_value_reading_proof& right
) {
  return left.token == right.token;
}

bool operator!=(
  const field_value_reading_proof& left,
  const field_value_reading_proof& right
) {
  return !(left == right);
}

template <typename Lexer>
struct field_value_reader {
  field_value_reader(
    Lexer& lexer,
    const field_value_reading_proof& proof
  ): lexer_(lexer), proof_(proof) {}

  template <typename Handler, typename RetVal>
  std::pair<RetVal, const field_value_reading_proof&> read(Handler& handler) {
    return std::pair<RetVal, const field_value_reading_proof&>({
      parse_expression<Lexer, Handler, RetVal>(lexer_, handler),
      proof_,
    });
  }

private:
  Lexer& lexer_;
  const field_value_reading_proof& proof_;
};

template <typename Lexer>
struct object_reader {
  object_reader(Lexer& lexer): lexer_(lexer) {}

  template <typename FieldReader>
  void operator()(FieldReader read_field) {
    std::string field_name;
    read_field_name_handler rfn_handler(field_name);
    read_field_colon_handler rfc_handler;
    bool has_field = lexer_.template next<read_field_name_handler, bool>(rfn_handler);
    while (has_field) {
      lexer_.template next<read_field_colon_handler, void>(rfc_handler);
      field_value_reading_proof proof;
      field_value_reader<Lexer> read_field_value(lexer_, proof);
      const field_value_reading_proof& ret_proof =
        read_field(field_name, read_field_value);
      if (proof != ret_proof) {
        throw std::runtime_error("invalid field-value-reading proof");
      }
      post_field_handler pf_handler;
      has_field = lexer_.template next<post_field_handler, bool>(pf_handler);
      if (!has_field) continue;
      read_new_field_name_handler rnfn_handler(field_name);
      lexer_.template next<read_new_field_name_handler, bool>(rnfn_handler);
    }
  }

private:
  Lexer& lexer_;
};

template <typename Lexer, typename Handler, typename RetVal>
struct parse_expression_handler {
  parse_expression_handler(Lexer& lexer, Handler& handler):
    lexer_(lexer), handler_(handler) {}

  RetVal end() const {
    throw unexpected_end_error();
  }

  RetVal punctuation(punctuation_type type) const {
    if (type == punctuation_type::brace_open) {
      object_reader<Lexer> reader(lexer_);
      return handler_.object(reader);
    }
    throw unexpected_punctuation_error();
  }

  RetVal string_literal(const std::string& literal) const {
    return handler_.string_literal(literal);
  }

  RetVal number_literal(float literal) const {
    return handler_.number_literal(literal);
  }

private:
  Lexer& lexer_;
  Handler& handler_;
};

template <typename Lexer, typename Handler, typename RetVal>
RetVal parse_expression(Lexer& lexer, Handler& handler) {
  typedef parse_expression_handler<Lexer, Handler, RetVal> lexer_handler;
  lexer_handler lx_handler(lexer, handler);
  return lexer.template next<lexer_handler, RetVal>(lx_handler);
}

}
}
