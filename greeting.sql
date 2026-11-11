# Creates greeting.say_hello(), a stored procedure that shows the message
# GREETING() reads from greeting.conf. Run it after
# INSTALL COMPONENT 'file://component_greeting', for example:
#   mysql < /usr/share/mysql/greeting/greeting.sql
#   CALL greeting.say_hello();

CREATE DATABASE IF NOT EXISTS greeting;

DROP PROCEDURE IF EXISTS greeting.say_hello;

CREATE PROCEDURE greeting.say_hello()
  SQL SECURITY INVOKER
  COMMENT 'Shows the message set in greeting.conf'
  SELECT GREETING() AS greeting;
