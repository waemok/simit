#ifndef SIMIT_DIAGNOSTICS_H
#define SIMIT_DIAGNOSTICS_H

#include <exception>
#include <string>
#include <vector>
#include <iostream>

#include "util/util.h"

namespace simit {

class CutOffStreambuf : public std::streambuf {
public:
  CutOffStreambuf(std::streambuf *dest) :
      dest(dest), cutoff(false), cutoffEnabled(false) {}
  
  virtual int overflow(int c) {
    if (c == EOF) {
      return c;
    }
    else if (!cutoff) {
      if (cutoffEnabled && c == '\n') {
        cutoff = true;
        const std::string cutoffText = " [...]";
        dest->sputn(cutoffText.c_str(), cutoffText.size());
        return traits_type::to_int_type(c);
      }
      else {
        dest->sputc(c);
        return traits_type::to_int_type(c);
      }
    }
  }

  void setCutoff(bool enabled) {
    cutoff = false;
    if (enabled) {
      cutoffEnabled = true;
    }
    else {
      cutoffEnabled = false;
    }
  }

private:
  std::streambuf *dest;
  bool cutoff;
  bool cutoffEnabled;
};

class SimitException : public std::exception {
public:
  SimitException() : errStreambuf(errString.rdbuf()),
                     errStream(&errStreambuf) {}
  SimitException(SimitException&& other) : // TODO: No string copy
      errStreambuf(errString.rdbuf()),
      errStream(&errStreambuf) {
    // Inefficient copying
    errString << other.errString.rdbuf();
  }
  
  virtual const char* what() const throw() {
    return errString.str().c_str();
  }

  // Resets context stream cutoff, and inserts string context description
  void addContext(std::string contextDesc) {
    errStreambuf.setCutoff(false);
    errStream << std::endl;
    errStreambuf.setCutoff(true);
    errStream << contextDesc;
  }

  // Writable error stream with an underlying buffer which cuts off at newlines
  std::ostream errStream;

private:
  std::stringstream errString;
  CutOffStreambuf errStreambuf;
};

/// Provides information about errors that occur while loading Simit code.
class ParseError {
public:
  ParseError(int firstLine, int firstColumn, int lastLine, int lastColumn,
        std::string msg);
  virtual ~ParseError();

  int getFirstLine() { return firstLine; }
  int getFirstColumn() { return firstColumn; }
  int getLastLine() { return lastLine; }
  int getLastColumn() { return lastColumn; }
  const std::string &getMessage() { return msg; }

  std::string toString() const;
  friend std::ostream &operator<<(std::ostream &os, const ParseError &obj) {
    return os << obj.toString();
  }
  bool operator<(const ParseError &rhs) const {
    return ((firstLine < rhs.firstLine) || 
           ((firstLine == rhs.firstLine) && (firstColumn < rhs.firstColumn)));
  }

private:
  int firstLine;
  int firstColumn;
  int lastLine;
  int lastColumn;
  std::string msg;
  std::string line;  // TODO
};

class Diagnostic {
public:
  Diagnostic() {}

  Diagnostic &operator<<(const std::string &str) {
    msg += str;
    return *this;
  }

  std::string getMessage() const { return msg; }

private:
  std::string msg;
};

class Diagnostics {
public:
  Diagnostics() {}
  ~Diagnostics() {}

  Diagnostic &report() {
    diags.push_back(Diagnostic());
    return diags[diags.size()-1];
  }

  bool hasErrors() {
    return diags.size() > 0;
  }

  std::string getMessage() const {
    std::string result;
    auto it = diags.begin();
    if (it != diags.end()) {
      result += it->getMessage();
      ++it;
    }
    while (it != diags.end()) {
      result += "\n" + it->getMessage();
      ++it;
    }
    return result;
  }

  std::vector<Diagnostic>::const_iterator begin() const { return diags.begin();}
  std::vector<Diagnostic>::const_iterator end() const { return diags.end(); }

private:
  std::vector<Diagnostic> diags;
};

std::ostream &operator<<(std::ostream &os, const Diagnostics &f);


namespace internal {

struct ErrorReport {
  enum Kind { User, Internal, Temporary };

  std::ostringstream *msg;
  const char *file;
  const char *func;
  int line;

  bool condition;
  const char *conditionString;

  Kind kind;
  bool warning;

  ErrorReport(const char *file, const char *func, int line, bool condition,
              const char *conditionString, Kind kind, bool warning)
      : msg(NULL), file(file), func(func), line(line), condition(condition),
        conditionString(conditionString), kind(kind), warning(warning) {
    if (condition) {
      return;
    }
    msg = new std::ostringstream;

    switch (kind) {
      case User:
        if (warning) {
          (*msg) << "Warning";
        } else {
          (*msg) << "Error";
        }
        (*msg) << " in " << func << " in file " << file << ":" << line << "\n";
        break;
      case Internal:
        (*msg) << "Internal ";
        if (warning) {
          (*msg) << "warning";
        } else {
          (*msg) << "error";
        }
        (*msg) << " at " << file << ":" << line << " in " << func << "\n";
        if (conditionString) {
          (*msg) << " Condition failed: " << conditionString << "\n";
        }
        break;
      case Temporary:
        (*msg) << "Temporary assumption broken";
        (*msg) << " at " << file << ":" << line << "\n";
        if (conditionString) {
          (*msg) << " Condition failed: " << conditionString << "\n";
        }
        break;
    }
    (*msg) << " ";
  }

  template<typename T>
  ErrorReport &operator<<(T x) {
    if (condition) {
      return *this;
    }
    (*msg) << x;
    return *this;
  }

  // Support for manipulators, such as std::endl
  ErrorReport &operator<<(std::ostream& (*manip)(std::ostream&)) {
    if (condition) {
      return *this;
    }
    (*msg) << manip;
    return *this;
  }

  ~ErrorReport() noexcept(false) {
    if (condition) {
      return;
    }
    explode();
  }

  void explode();
};

// internal asserts
#ifdef SIMIT_ASSERTS
  #define iassert(c)                                                           \
    simit::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, (c), #c,    \
                               simit::internal::ErrorReport::Internal, false)
  #define ierror                                                               \
    simit::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, false, NULL,\
                               simit::internal::ErrorReport::Internal, false)
#else
  struct Dummy {
    template<typename T>
    Dummy &operator<<(T x) {
      return *this;
    }
    // Support for manipulators, such as std::endl
    Dummy &operator<<(std::ostream& (*manip)(std::ostream&)) {
      return *this;
    }
  };

  #define iassert(c) simit::internal::Dummy()
  #define ierror simit::internal::Dummy()
#endif

#define tassert(c)                                                             \
  simit::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, (c), #c,      \
                               simit::internal::ErrorReport::Temporary, false)
#define terror                                                                 \
  simit::internal::ErrorReport(__FILE__, __FUNCTION__, __LINE__, false, NULL,  \
                               simit::internal::ErrorReport::Temporary, false)

#define unreachable                                                            \
  ierror << "reached unreachable location"

#define not_supported_yet                                                      \
  ierror << "Not supported yet, but planned for the future\n "

// internal assert helpers
#define iassert_scalar(a)                                                      \
  iassert(isScalar(a.type())) << a << ": " << a.type()

#define iassert_types_equal(a,b)                                               \
  iassert(a.type() == b.type()) << a.type() << " != " << b.type() << "\n"      \
                                << #a << ": " << a << "\n" << #b << ": " << b

#define iassert_boolean_scalar(a)                                              \
  iassert(isScalar(a.type()))                                                  \
      << a << "must be a boolean scalar but is a" << a.type()

// User asserts
#define uassert(c)                                                             \
  simit::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, (c), #c,        \
                               simit::internal::ErrorReport::User, false)
#define uerror                                                                 \
  simit::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, false, nullptr, \
                               simit::internal::ErrorReport::User, false)
#define uwarning                                                               \
  simit::internal::ErrorReport(__FILE__,__FUNCTION__,__LINE__, false, nullptr, \
                               simit::internal::ErrorReport::User, true)
}}

#endif
