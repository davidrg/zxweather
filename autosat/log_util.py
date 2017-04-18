# coding=utf-8
"""
Logging utilities

autowx - https://github.com/cyber-atomus/autowx
(C) Copyright Chris Lewandowski 2017
Portions (C) Copyright David Goodwin, 2017

License: MIT

This module is based on autowx which is (C) Copyright Chris Lewandowski 2017 
and licensed under the MIT License.
https://github.com/cyber-atomus/autowx
"""

import sys


class bcolors:
    HEADER = '\033[95m'
    CYAN = '\033[96m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[97m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    GRAY = '\033[37m'
    UNDERLINE = '\033[4m'

logLineStart = bcolors.BOLD + bcolors.HEADER + "***>\t" + bcolors.ENDC + bcolors.OKGREEN
logLineEnd=bcolors.ENDC

def log(msg):
    print(logLineStart + msg + logLineEnd)


class Logger(object):
    def __init__(self, filename="Default.log"):
        self.terminal = sys.stdout
        self.log = open(filename, "a")

    def write(self, message):
        self.terminal.write(message)
        self.log.write(message)
