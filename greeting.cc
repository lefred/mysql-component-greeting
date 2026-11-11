/* Copyright (c) 2026 lefred (Frédéric Descamps)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

/*
  A sample component: GREETING() returns the message set in its config
  file, greeting.conf, read on every call. The package also ships
  greeting.sql, which creates a stored procedure calling GREETING().

  The config file is greeting.config_file, by default
  <etc>/mysql/greeting/greeting.conf, where <etc> is <basedir>/etc for a
  server installed from a tarball, whose plugin directory is
  <basedir>/lib/plugin, and /etc otherwise.
*/

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/my_err_bits.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_runtime_warning.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>

#include <mysqld_error.h>

#include <limits.h>
#include <stdlib.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_warning);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_reader);

namespace {

const char *COMPONENT_NAME = "greeting";
const char *UDF_NAME = "greeting";
const size_t MAX_CONFIG_BYTES = 64 * 1024;

char *config_file = nullptr;
std::string default_config_file;

/* The value of a server variable, or "" when it cannot be read. */
std::string server_variable(const char *name) {
  char buffer[PATH_MAX + 1];
  char *value = buffer;
  size_t length = sizeof(buffer) - 1;
  if (mysql_service_mysql_system_variable_reader->get(
          nullptr, "GLOBAL", "mysql_server", name,
          reinterpret_cast<void **>(&value), &length))
    return "";
  return std::string(value, length);
}

/* <basedir>/etc when the plugin directory is <basedir>/lib/plugin. */
std::string default_etc_dir() {
  std::string plugin_dir = server_variable("plugin_dir");
  char resolved[PATH_MAX];
  if (!plugin_dir.empty() && realpath(plugin_dir.c_str(), resolved))
    plugin_dir = resolved;
  while (plugin_dir.size() > 1 && plugin_dir.back() == '/')
    plugin_dir.pop_back();
  for (const char *lib : {"/lib/plugin", "/lib64/plugin"}) {
    size_t length = strlen(lib);
    if (plugin_dir.size() > length &&
        plugin_dir.compare(plugin_dir.size() - length, length, lib) == 0)
      return plugin_dir.substr(0, plugin_dir.size() - length) + "/etc";
  }
  return "/etc";
}

bool register_variables() {
  default_config_file = default_etc_dir() + "/mysql/greeting/greeting.conf";
  STR_CHECK_ARG(str) arg;
  arg.def_val = const_cast<char *>(default_config_file.c_str());
  return mysql_service_component_sys_variable_register->register_variable(
      COMPONENT_NAME, "config_file",
      PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY |
          PLUGIN_VAR_RQCMDARG,
      "The file GREETING() reads its message from. Default: "
      "<etc>/mysql/greeting/greeting.conf, where <etc> is <basedir>/etc "
      "when the plugin directory is <basedir>/lib/plugin, and /etc otherwise",
      nullptr, nullptr, &arg, &config_file);
}

std::string trim(const std::string &text) {
  const char *space = " \t\r";
  size_t begin = text.find_first_not_of(space);
  if (begin == std::string::npos) return "";
  return text.substr(begin, text.find_last_not_of(space) - begin + 1);
}

/*
  Reads `message = TEXT` from the config file: blank lines and lines
  starting with # are ignored, and TEXT may be in double quotes.
*/
bool read_message(std::string *message, std::string *error) {
  const char *path = config_file ? config_file : "";
  FILE *file = fopen(path, "r");
  if (!file) {
    *error = std::string("cannot open ") + path + ": " + strerror(errno);
    return false;
  }
  std::string text;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0 &&
         text.size() <= MAX_CONFIG_BYTES)
    text.append(buffer, n);
  bool failed = ferror(file);
  fclose(file);
  if (failed) {
    *error = std::string("cannot read ") + path;
    return false;
  }
  if (text.size() > MAX_CONFIG_BYTES) {
    *error = std::string(path) + " is larger than 64 KiB";
    return false;
  }

  size_t start = 0;
  unsigned line_number = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = trim(text.substr(start, end - start));
    start = end + 1;
    ++line_number;
    if (line.empty() || line[0] == '#') continue;
    size_t equals = line.find('=');
    if (equals == std::string::npos) {
      *error = std::string(path) + ":" + std::to_string(line_number) +
               ": expected key = value";
      return false;
    }
    if (trim(line.substr(0, equals)) != "message") continue;
    std::string value = trim(line.substr(equals + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
      value = value.substr(1, value.size() - 2);
    *message = value;
    return true;
  }
  *error = std::string(path) + " sets no message";
  return false;
}

/* Adds "greeting UDF failed; <error>" to the session's warnings. */
void warn(const std::string &error) {
  MYSQL_THD thd = nullptr;
  if (mysql_service_mysql_current_thread_reader->get(&thd) || !thd) return;
  mysql_service_mysql_runtime_warning->emit(thd, ER_UDF_ERROR, UDF_NAME,
                                            error.c_str());
}

bool greeting_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count != 0) {
    snprintf(message, MYSQL_ERRMSG_SIZE, "GREETING() takes no arguments");
    return true;
  }
  char charset[] = "utf8mb4";
  if (mysql_service_mysql_udf_metadata->result_set(initid, "charset",
                                                   charset)) {
    snprintf(message, MYSQL_ERRMSG_SIZE, "cannot set the result charset");
    return true;
  }
  initid->maybe_null = true;
  initid->const_item = false;
  initid->max_length = MAX_CONFIG_BYTES;
  initid->ptr = reinterpret_cast<char *>(new (std::nothrow) std::string());
  if (!initid->ptr) {
    snprintf(message, MYSQL_ERRMSG_SIZE, "out of memory");
    return true;
  }
  return false;
}

void greeting_deinit(UDF_INIT *initid) {
  delete reinterpret_cast<std::string *>(initid->ptr);
}

char *greeting(UDF_INIT *initid, UDF_ARGS *, char *, unsigned long *length,
               unsigned char *is_null, unsigned char *) {
  std::string *result = reinterpret_cast<std::string *>(initid->ptr);
  std::string error;
  if (!read_message(result, &error)) {
    warn(error);
    *is_null = 1;
    return nullptr;
  }
  *is_null = 0;
  *length = result->size();
  return result->data();
}

mysql_service_status_t greeting_component_init() {
  if (register_variables()) return 1;
  if (mysql_service_udf_registration->udf_register(
          UDF_NAME, STRING_RESULT, reinterpret_cast<Udf_func_any>(greeting),
          greeting_init, greeting_deinit)) {
    mysql_service_component_sys_variable_unregister->unregister_variable(
        COMPONENT_NAME, "config_file");
    return 1;
  }
  return 0;
}

mysql_service_status_t greeting_component_deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister(UDF_NAME, &was_present) &&
      was_present)
    return 1;  // still in use
  mysql_service_component_sys_variable_unregister->unregister_variable(
      COMPONENT_NAME, "config_file");
  return 0;
}

}  // namespace

BEGIN_COMPONENT_PROVIDES(greeting)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(greeting)
REQUIRES_SERVICE(udf_registration), REQUIRES_SERVICE(mysql_udf_metadata),
    REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(mysql_current_thread_reader),
    REQUIRES_SERVICE(mysql_runtime_warning),
    REQUIRES_SERVICE(mysql_system_variable_reader), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(greeting)
METADATA("mysql.author", "lefred"), METADATA("mysql.license", "GPL"),
    METADATA("greeting.version", GREETING_VERSION),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(greeting, "greeting")
greeting_component_init, greeting_component_deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(greeting)
    END_DECLARE_LIBRARY_COMPONENTS
