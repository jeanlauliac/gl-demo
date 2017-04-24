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
  field_value_reader(Lexer& lexer): lexer_(lexer) {}

  template <typename Handler, typename RetVal>
  RetVal read(Handler& handler) {
    return parse_expression<Lexer, Handler, RetVal>(lexer_, handler);
  }

private:
  Lexer& lexer_;
};

template <typename Lexer>
struct object_reader {
  typedef Lexer lexer_type;
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

template <typename Lexer>
struct array_reader;

template <typename Lexer, typename ItemHandler>
struct read_array_first_item_handler {
  read_array_first_item_handler(Lexer& lexer, ItemHandler& item_handler):
    lexer_(lexer), item_handler_(item_handler) {}

  bool end() const { throw unexpected_end_error(); }

  bool punctuation(punctuation_type type) const {
    if (type == punctuation_type::brace_open) {
      object_reader<Lexer> reader(lexer_);
      item_handler_.object(reader);
      return true;
    }
    if (type == punctuation_type::bracket_open) {
      array_reader<Lexer> reader(lexer_);
      item_handler_.array(reader);
      return true;
    }
    if (type == punctuation_type::bracket_close) {
      return false;
    }
    throw unexpected_punctuation_error();
  }

  bool string_literal(const std::string& literal) const {
    item_handler_.string_literal(literal);
    return true;
  }

  bool number_literal(float literal) const {
    item_handler_.number_literal(literal);
    return true;
  }

private:
  Lexer& lexer_;
  ItemHandler& item_handler_;
};

struct array_post_item_handler {
  bool end() const { throw unexpected_end_error(); }

  bool punctuation(punctuation_type type) const {
    if (type == punctuation_type::comma) return true;
    if (type == punctuation_type::bracket_close) return false;
    throw unexpected_punctuation_error();
  }

  bool string_literal(const std::string& literal) const {
    throw unexpected_string_error();
  }

  bool number_literal(float literal) const {
    throw unexpected_number_error();
  }
};

template <typename Lexer>
struct array_reader {
  typedef Lexer lexer_type;
  array_reader(Lexer& lexer): lexer_(lexer) {}

  template <typename ItemHandler>
  void operator()(ItemHandler& item_handler) {
    typedef read_array_first_item_handler<Lexer, ItemHandler> first_item_handler;
    first_item_handler rafi_handler(lexer_, item_handler);
    bool has_more_items = lexer_.template next<first_item_handler, bool>(rafi_handler);
    if (!has_more_items) return;
    array_post_item_handler api_handler;
    has_more_items = lexer_.template next<array_post_item_handler, bool>(api_handler);
    while (has_more_items) {
      parse_expression<Lexer, ItemHandler, void>(lexer_, item_handler);
      has_more_items = lexer_.template next<array_post_item_handler, bool>(api_handler);
    };
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
    if (type == punctuation_type::bracket_open) {
      array_reader<Lexer> reader(lexer_);
      return handler_.array(reader);
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
