# coding=utf-8
"""
Unit tests for zxcl
"""

from server.zxcl.lexer import Lexer, UnexpectedCharacterError

__author__ = 'david'

import unittest

class IdentifierToken(unittest.TestCase):
    """
    Tests that the lexer recognises each token type
    """

    def check(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertEquals(result.type, lexer.IDENTIFIER)
        self.assertEquals(result.value, value)

    def check_not_equals(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertNotEquals(result.type, lexer.IDENTIFIER)

    def test_alpha_lower(self):
        value = "abacus"
        self.check(value)


    def test_alpha_upper(self):
        value = "ABACUS"
        self.check(value)

    def test_single_letter(self):
        value = "a"
        self.check(value)

    def test_underscore(self):
        value = "_"
        self.check(value)

    def test_digit(self):
        value = "9"
        self.check_not_equals(value)

