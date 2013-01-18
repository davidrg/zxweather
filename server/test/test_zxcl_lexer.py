# coding=utf-8
"""
Unit tests for zxcl
"""
from datetime import datetime

from server.zxcl.lexer import Lexer, Token

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

class FloatToken(unittest.TestCase):
    """
    Tests the Float token
    """

    def check(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertEquals(result.type, lexer.FLOAT)
        self.assertEquals(result.value, float(value))

    def test_1(self):
        value = "12.0"
        self.check(value)

    def test_2(self):
        value = "0.0"
        self.check(value)

    def test_3(self):
        value = ".123"
        self.check(value)

    def test_positive(self):
        value = "+0.123"
        self.check(value)

    def test_negative(self):
        value = "-0.123"
        self.check(value)

    def test_e(self):
        value = "+0.123e5"
        self.check(value)

    def test_e_plus(self):
        value = "+.123e+20"
        self.check(value)

    def test_e_minus(self):
        value = "+123e-20"
        self.check(value)

class IntegerToken(unittest.TestCase):
    """
    Tests recognition of integers.
    """

    def check(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertEquals(result.type, lexer.INTEGER)
        self.assertEquals(result.value, int(value))

    def check_not_equals(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertNotEquals(result.type, lexer.INTEGER)

    def test_int(self):
        self.check("1")

    def test_positive(self):
        self.check("+100")

    def test_negative(self):
        self.check("-1000")

    def test_float_1(self):
        self.check_not_equals("1.0")

    def test_float_2(self):
        self.check_not_equals(".0")

    def test_float_3(self):
        self.check_not_equals("1.")

class DateToken(unittest.TestCase):
    """
    Tests recognition of the date token
    """

    def check(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertEquals(result.type, lexer.DATE)
        self.assertEquals(result.value, datetime.strptime(value, '%d-%b-%Y').date())

    def check_not_equals(self, value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertNotEquals(result.type, lexer.DATE)

    def test_one_digit_day(self):
        self.check("1-JUN-2012")

    def test_two_digit_day(self):
        self.check("01-JUN-2012")

    def test_valid_day_required(self):
        self.check_not_equals("99-JUN-2012")

    def test_valid_month_required(self):
        self.check_not_equals("01-FOO-2012")

    def test_four_digit_year_required(self):
        """
        Dates must use four digit year numbers
        """
        self.check_not_equals("01-JUN-12")

    def test_invalid_leap_day(self):
        self.check_not_equals("29-FEB-2015")

    def test_valid_leap_day(self):
        self.check("29-FEB-2016")

class StringToken(unittest.TestCase):
    """
    Tests the string token is recognised and converted correctly.
    """

    def check(self, value, parsed_value):
        lexer = Lexer(value)

        result = lexer.next_token()

        self.assertEquals(result.type, lexer.STRING)
        self.assertEquals(result.value, parsed_value)

    def test_string(self):
        self.check('"test"', 'test')

    def test_string_with_double_quote(self):
        self.check(r'"\""', '"')

    def test_string_with_single_quote(self):
        self.check(r'"\'"', "'")

    def test_string_with_newline(self):
        self.check(r'"Hello\nWorld!"', 'Hello\nWorld!')

    def test_string_with_backslash(self):
        self.check(r'"Hello\\World!"', 'Hello\\World!')

    def test_string_with_bell(self):
        self.check(r'"Hello\aWorld!"', 'Hello\aWorld!')

    def test_string_with_backspace(self):
        self.check(r'"Hello\bWorld!"', 'Hello\bWorld!')

    def test_string_with_tab(self):
        self.check(r'"Hello\tWorld!"', 'Hello\tWorld!')

    def test_string_with_form_feed(self):
        self.check(r'"Hello\fWorld!"', 'Hello\fWorld!')

    def test_string_with_carriage_return(self):
        self.check(r'"Hello\rWorld!"', 'Hello\rWorld!')

    def test_string_with_vertical_tab(self):
        self.check(r'"Hello\vWorld!"', 'Hello\vWorld!')

    def test_string_with_hex_ascii(self):
        self.check(r'"Hello\x5cWorld!"', 'Hello\x5cWorld!')

    def test_string_with_oct_ascii(self):
        self.check(r'"Hello\134World!"', 'Hello\134World!')

class TokenTests(unittest.TestCase):
    """
    Tests the Token class
    """

    def test_constructor(self):
        token = Token(1,"value",12)

        self.assertEqual(token.type, 1)
        self.assertEqual(token.value, "value")
        self.assertEqual(token.position, 12)

    def test_name(self):
        token = Token(1, "value", 12)

        self.assertEqual(token.name(), Lexer.token_names[1].title())

    def test_str(self):
        token = Token(1, "value", 12)

        self.assertEqual(str(token), "<{val},{typ}>".format(
            val="value", typ=Lexer.token_names[1]))


class LexerTests(unittest.TestCase):
    """
    Tests for the Lexer class. Only the basic tokens (= and /) as well as
    whitespace and comments are tested here. Value-type tokens (strings,
    integers, etc) are tested elsewhere.

    Spacing around value-type tokens is however tested here.
    """

    def test_comment_character_only(self):
        """
        The comment character should be treated as white space. If it appears
        at the start of the command then the only token should be EOF
        """
        lex = Lexer("! foo")

        tok = lex.next_token()

        self.assertEquals(tok.type, Lexer.EOF)

    def test_comment_at_end_of_statement(self):
        """
        Only two tokens (IDENTIFIER, EOF) should come out
        """
        lex = Lexer("test ! foo")

        tok = lex.next_token()

        self.assertEquals(tok.type, Lexer.IDENTIFIER)

        tok = lex.next_token()

        self.assertEquals(tok.type, Lexer.EOF)

    def test_whitespace(self):
        lex = Lexer(" \t\n\r   \t \n \r")

        tok = lex.next_token()

        self.assertEquals(tok.type, Lexer.EOF)

    def test_equals(self):
        lex = Lexer("=")
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.EQUAL)

    def test_slash(self):
        lex = Lexer("/")
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.FORWARD_SLASH)
