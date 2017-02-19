#pragma once

namespace testing {

class ReportingError {
public:
  const std::string expr_string;
  ReportingError(const std::string& expr_string_): expr_string(expr_string_) {};
};

void expect(bool result, const std::string& expr_string) {
  if (!result) {
    throw ReportingError(expr_string);
  }
}

enum class TestCacheResult {
  ok,
  not_ok
};

void write_case_baseline(TestCacheResult result, int index, const std::string& desc) {
  const char* result_str = result == TestCacheResult::ok ? "ok" : "not ok";
  std::cout << result_str << ' ' << index << " - " << desc << std::endl;
}

template <typename TCase>
void run_case(TCase test_case, int index, const std::string& desc) {
  try {
    test_case();
  } catch (ReportingError ex) {
    write_case_baseline(TestCacheResult::not_ok, index, desc);
    std::cout << "  ---" << std::endl;
    std::cout
      << "  message: \"the expression `" << ex.expr_string
      << "` was not true\"" << std::endl;
    std::cout << "  severity: fail" << std::endl;
    std::cout << "  ..." << std::endl;
    return;
  }
  write_case_baseline(TestCacheResult::ok, index, desc);
}

void write_header(size_t test_case_count) {
  std::cout << "TAP version 13" << std::endl;
  std::cout << "1.." << test_case_count << std::endl;
}

}
