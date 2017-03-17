#include "testing.h"

namespace testing {

void expect(bool result, const std::string& expr_string) {
  if (!result) {
    throw expectation_failed_error(expr_string);
  }
}

void write_case_baseline(test_case_result result, int index, const std::string& desc) {
  const char* result_str = result == test_case_result::ok ? "ok" : "not ok";
  std::cout << result_str << ' ' << index << " - " << desc << std::endl;
}

void write_header() {
  std::cout << "TAP version 13" << std::endl;
}

void write_plan(int last_index) {
  std::cout << "1.." << last_index << std::endl;
}

}
