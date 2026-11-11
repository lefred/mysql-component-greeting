# Copyright (c) 2026 lefred (Frederic Descamps)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1335 USA

# Built in a MySQL server source tree, as components/greeting: the server
# adds every components/*/CMakeLists.txt to its build.

SET(GREETING_VERSION "1.0.2")

MYSQL_ADD_COMPONENT(greeting
  greeting.cc
  MODULE_ONLY
  )

TARGET_COMPILE_DEFINITIONS(component_greeting
  PRIVATE GREETING_VERSION="${GREETING_VERSION}")

# <etc>/mysql/greeting/ (etc in a tarball, /etc for RPM and DEB), where the
# component looks for greeting.conf by default, and share/.../greeting/.
IF(INSTALL_LAYOUT MATCHES "STANDALONE")
  SET(GREETING_CONF_DIR "etc/mysql/greeting")
ELSE()
  SET(GREETING_CONF_DIR "/etc/mysql/greeting")
ENDIF()

INSTALL(FILES greeting.conf
  DESTINATION ${GREETING_CONF_DIR} COMPONENT Server)
INSTALL(FILES greeting.sql
  DESTINATION ${INSTALL_MYSQLSHAREDIR}/greeting COMPONENT Server)
