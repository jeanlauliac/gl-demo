#pragma once

#include "lexer.h"

namespace upd {
namespace json {

struct unexpected_end_error {};
struct unexpected_number_error {};
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

template <typename Lexer>
struct object_reader {
  object_reader(Lexer& lexer): lexer_(lexer) {}

  template <typename FieldReader>
  void operator()(FieldReader field_reader) {
    std::string field_name;
    read_field_name_handler rfn_handler(field_name);
    if (lexer_.template next<read_field_name_handler, bool>(rfn_handler)) {
      throw std::runtime_error("not implemented");
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
