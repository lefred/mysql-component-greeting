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

SET(COMPONENT_NAME "greeting")
SET(COMPONENT_VERSION "1.0.2")
SET(COMPONENT_AUTHOR "lefred")
SET(COMPONENT_DESCRIPTION "Sample component whose GREETING() function return the message set in its config file")
SET(COMPONENT_LICENSE "PLUGIN_LICENCE_GPL")

MYSQL_ADD_COMPONENT(greeting
  greeting.cc
  MODULE_ONLY
  )

TARGET_COMPILE_DEFINITIONS(component_greeting
  PRIVATE COMPONENT_NAME="${COMPONENT_NAME}"
  PRIVATE COMPONENT_VERSION="${COMPONENT_VERSION}"
  PRIVATE COMPONENT_AUTHOR="${COMPONENT_AUTHOR}"
  PRIVATE COMPONENT_DESCRIPTION="${COMPONENT_DESCRIPTION}"
  PRIVATE COMPONENT_LICENSE="${COMPONENT_LICENSE}"
)

# <etc>/mysql/greeting/ (etc in a tarball, /etc for RPM and DEB), where the
# component looks for greeting.conf by default, and share/.../greeting/.
IF(INSTALL_LAYOUT MATCHES "STANDALONE")
  SET(GREETING_CONF_DIR "etc/mysql/greeting")
  SET(GREETING_SHARE_DIR "share/mysql/greeting")
ELSE()
  SET(GREETING_CONF_DIR "/etc/mysql/greeting")
  SET(GREETING_SHARE_DIR "${INSTALL_MYSQLSHAREDIR}/mysql/greeting")
ENDIF()

INSTALL(FILES greeting.conf
  DESTINATION ${GREETING_CONF_DIR} COMPONENT Server)
INSTALL(FILES greeting.sql
  DESTINATION ${GREETING_SHARE_DIR} COMPONENT Server)
