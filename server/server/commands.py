# coding=utf-8
"""
Some basic commands
"""
from twisted.internet import defer

__author__ = 'david'

def setenv(writer, reader, environment, parameters, qualifiers):
    """
    Sets an environment variable
    :param environment:
    :param parameters:
    :param qualifiers:
    """
    key = parameters[0]
    value = parameters[1]
    environment[key] = value

def show_user(writer, reader,environment, parameters, qualifiers):
    writer(environment["username"] + "\r\n")

def set_client(writer, reader, environment, parameters, qualifiers):
    client_name = parameters[1]
    if "version" in qualifiers:
        client_version = qualifiers["version"]
    else:
        client_version = "Not specified"

    version_info = {
        "name": client_name,
        "version": client_version
    }

    environment["client"] = version_info

def show_client(writer, reader, environment, parameters, qualifiers):
    if "client" not in environment:
        writer("Unknown client.\r\n")
        return

    client_info = environment["client"]

    info_str = "Client: {0}\r\nVersion: {1}\r\n".format(
        client_info["name"], client_info["version"])
    writer(info_str)


