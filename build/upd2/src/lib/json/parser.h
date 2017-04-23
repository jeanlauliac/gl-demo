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

template <typename Lexer>
struct field_value_reader {
  field_value_reader(Lexer& lexer): lexer_(lexer), read_(false) {}

  template <typename Handler, typename RetVal>
  RetVal operator()(Handler& handler) {
    read_ = true;
    return parse_expression<Lexer, Handler, RetVal>(lexer_, handler);
  }

  bool read() const {
    return read_;
  }

private:
  Lexer& lexer_;
  bool read_;
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
      field_value_reader<Lexer> read_field_value(lexer_);
      read_field(field_name, read_field_value);
      if (!read_field_value.read()) {
        throw std::runtime_error("field value expression was not read");
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




// CHANGE THAT
// instead of having parser take Handler that must handle everything, make an
// `expression_parser` that have Handler accepting only expressions: object,
// array, number, string. Then, have the object() handler take as argument
// another instance of an `object_parser`. This different parser should take a
// Handler accepting only fields: name + expression_parser
// expression_parser should actually be a function, not a class with next(),
// because it's always parsing a single expression, not a collection of items
//
// then we have `object_parser` and `array_parser`, 3 types of parsers total.
// pretty simple
// it'll make implementing high-level parsing functions much simpler, ex.
// parse_manifest, as it'll only have to parse a single expression

// Also, make Lexer a template parameter so that we can implement a
// `json_stream` in the future, that checks for `lexer::end()` between each expression




template <typename Lexer, typename Handler, typename RetVal>
RetVal parse_expression(Lexer& lexer, Handler& handler) {
  typedef parse_expression_handler<Lexer, Handler, RetVal> lexer_handler;
  lexer_handler lx_handler(lexer, handler);
  return lexer.template next<lexer_handler, RetVal>(lx_handler);
}


}
}
