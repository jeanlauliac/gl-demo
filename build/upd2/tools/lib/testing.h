#pragma once

#include <iostream>

namespace testing {

class expectation_failed_error {
public:
  const std::string expr_string;
  expectation_failed_error(const std::string& expr_string_):
    expr_string(expr_string_) {};
};

void expect(bool result, const std::string& expr_string);

enum class test_case_result { ok, not_ok };

void write_case_baseline(test_case_result result, int index, const std::string& desc);

template <typename TCase>
void run_case(TCase test_case, int& index, const std::string& desc) {
  ++index;
  try {
    test_case();
  } catch (expectation_failed_error ex) {
    write_case_baseline(test_case_result::not_ok, index, desc);
    std::cout << "  ---" << std::endl;
    std::cout
      << "  message: \"expectation failed: `" << ex.expr_string
      << "`\"" << std::endl;
    std::cout << "  severity: fail" << std::endl;
    std::cout << "  ..." << std::endl;
    return;
  }
  write_case_baseline(test_case_result::ok, index, desc);
}

void write_header();
void write_plan(int last_index);

}
