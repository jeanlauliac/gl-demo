#pragma once

#include <string>

namespace ds {

/**
 * To be used for all conditions that are not programming errors but rather
 * environment issues: ex. missing files. They are to be displayed to the user
 * directly, without a stack trace.
 */
struct SystemException {
  SystemException(std::string message): message(message) {}
  std::string message;
};

}
