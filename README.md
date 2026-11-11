# mysql-component-greeting

A sample MySQL component that ships more than a library: a config file and
a SQL script. It is the MySQL counterpart of
[mariadb-plugin-greeting](https://github.com/lefred/mariadb-plugin-greeting).

- `GREETING()` returns the message set in the config file, `greeting.conf`.
  The component reads the file on every call, so an edit applies at once.
- `greeting.sql` creates the stored procedure `greeting.say_hello()`, which
  calls `GREETING()`.

It needs MySQL 9.6 or later, for the `mysql_runtime_warning` service that
`GREETING()` uses to report why it returns NULL.

## Files

| File | tar.gz (server basedir) | rpm / deb |
|------|-------------------------|-----------|
| `component_greeting.so` | `lib/plugin/` | the server's plugin directory |
| `greeting.conf` | `etc/mysql/greeting/` | `/etc/mysql/greeting/` |
| `greeting.sql` | `share/greeting/` | `<INSTALL_MYSQLSHAREDIR>/greeting/` |

## Config file

```ini
# Blank lines and lines starting with # are ignored.
message = Hello from the greeting sample component!
```

The value can be in double quotes, to keep leading or trailing spaces.
Other `key = value` lines are ignored.

When the file cannot be read, is larger than 64 KiB, has no `message`, or
has a line that is not `key = value`, `GREETING()` returns NULL with a
warning that says why:

```
Warning 3200 greeting UDF failed; /etc/mysql/greeting/greeting.conf sets no message
```

The file is `greeting.config_file`, a read-only variable. Set it in the
server's config file:

```ini
[mysqld]
loose-greeting.config_file = /path/to/greeting.conf
```

or when installing the component:

```sql
INSTALL COMPONENT 'file://component_greeting'
  SET GLOBAL greeting.config_file = '/path/to/greeting.conf';
```

By default it is `<etc>/mysql/greeting/greeting.conf`. `<etc>` is
`<basedir>/etc` when the plugin directory is `<basedir>/lib/plugin` (or
`lib64/plugin`), as in a tarball install, and `/etc` otherwise. The plugin
directory is resolved first, so a symlinked `lib/plugin` counts as where it
points.

## Use

```sql
INSTALL COMPONENT 'file://component_greeting';
SELECT GREETING();
```

```sh
mysql < /usr/share/mysql/greeting/greeting.sql
```

```sql
CALL greeting.say_hello();
```

The procedure is `SQL SECURITY INVOKER`. Like any loadable function,
`GREETING()` cannot be used in a generated column.

```sql
UNINSTALL COMPONENT 'file://component_greeting';
```

## Build

In a MySQL server source tree, which builds every
`components/*/CMakeLists.txt`:

```sh
ln -s /path/to/mysql-component-greeting mysql-server/components/greeting
cmake -S mysql-server -B build
cmake --build build --target component_greeting
```

The library is `build/plugin_output_directory/component_greeting.so`.

## Tests

The MTR suite is `mysql-test/suite/component_greeting`. Link it into the
server tree, then run it from the build:

```sh
ln -s /path/to/mysql-component-greeting/mysql-test/suite/component_greeting \
      mysql-server/mysql-test/suite/component_greeting
cd build/mysql-test
./mtr --suite=component_greeting
```

`script.test` runs `greeting.sql`. It finds the script at
`components/greeting/` in the server tree, or at `share/greeting/` in a
tarball install. Without the script, the test is skipped.
