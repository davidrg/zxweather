# coding=utf-8
"""
Unit tests for zxcl
"""
from datetime import datetime

from server.zxcl.lexer import Lexer, Token, UnexpectedCharacterError

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

    def assertTokenType(self, command_string, token_type, value=None):
        lex = Lexer(command_string)
        tok = lex.next_token()
        self.assertEquals(tok.type, token_type)

        if value is not None:
            self.assertEqual(tok.value, value)

    def test_whitespace(self):
        self.assertTokenType(" \t\n\r   \t \n \r", Lexer.EOF)

    def test_equals(self):
        self.assertTokenType("=", Lexer.EQUAL)

    def test_slash(self):
        self.assertTokenType("/", Lexer.FORWARD_SLASH)

    def test_value_can_be_at_start_of_command(self):
        self.assertTokenType("42", Lexer.INTEGER, 42)

    def test_value_can_be_after_equal(self):
        lex = Lexer("=42")
        tok = lex.next_token()
        self.assertEqual(tok.type, Lexer.EQUAL)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_value_can_be_after_slash(self):
        lex = Lexer("/42")
        tok = lex.next_token()
        self.assertEqual(tok.type, Lexer.FORWARD_SLASH)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_value_can_be_after_white_space(self):
        lex = Lexer(" \t\n\r   \t \n \r42")
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_bad_value_throws_exception(self):
        lex = Lexer("..42")
        with self.assertRaises(UnexpectedCharacterError):
            # This should blow up as ..42 does not match any value type
            lex.next_token()

    def test_tokenise(self):
        command_string = "tokenise this string 42"
        tokens = Lexer.tokenise(command_string)

        self.assertEquals(tokens[0].type, Lexer.IDENTIFIER)
        self.assertEquals(tokens[1].type, Lexer.IDENTIFIER)
        self.assertEquals(tokens[2].type, Lexer.IDENTIFIER)
        self.assertEquals(tokens[3].type, Lexer.INTEGER)
        self.assertEquals(tokens[4].type, Lexer.EOF)

    def test_get_value_type_string_default_option(self):
        """
        When the string_default parameter is True the get_value_token_type
        method should just say its a string if it doesn't appear to be anything
        else.
        """
        self.assertEquals(Lexer.get_value_token_type("test string", True),
            Lexer.STRING)

    def test_empty_input_string_is_treated_as_a_comment(self):
        """
        If the input string is empty the lexer should just treat it as an empty
        command (as if the first character were a comment). This means the
        first token should be EOF signaling the end of the token stream.
        """
        self.assertTokenType("", Lexer.EOF)


class LexerValueSeparationTests(unittest.TestCase):
    """
    Tests that all value types must be separated from other value types
    somehow.
    """

    def test_values_can_be_separated_by_space(self):
        lex = Lexer("abc 42")
        tok = lex.next_token()
        self.assertEqual(tok.type, Lexer.IDENTIFIER)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_values_can_be_separated_by_equal(self):
        lex = Lexer("abc=42")
        tok = lex.next_token()
        self.assertEqual(tok.type, Lexer.IDENTIFIER)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.EQUAL)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_values_can_be_separated_by_slash(self):
        lex = Lexer("abc/42")
        tok = lex.next_token()
        self.assertEqual(tok.type, Lexer.IDENTIFIER)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.FORWARD_SLASH)
        tok = lex.next_token()
        self.assertEquals(tok.type, Lexer.INTEGER)

    def test_unseparated_bad_value_throws_exception(self):
        lex = Lexer("42..42")
        lex.next_token() # This will match 42 as an int
        with self.assertRaises(UnexpectedCharacterError):
            # This should blow up as ..42 does not match any value type
            lex.next_token()

    def test_date_requires_separation(self):
        lex = Lexer('"str"18-JAN-2013')
        lex.next_token() # match the string
        with self.assertRaises(UnexpectedCharacterError):
            # Die because the date isn't separated from the string
            lex.next_token()

    def test_float_requires_separation(self):
        lex = Lexer('"str".42')
        lex.next_token() # match the string
        with self.assertRaises(UnexpectedCharacterError):
            lex.next_token() # Die because .42 isn't separated from the string

    def test_integer_requires_separation(self):
        lex = Lexer('"str"42')
        lex.next_token() # match the string
        with self.assertRaises(UnexpectedCharacterError):
            lex.next_token() # Die because 42 isn't separated from the string

    def test_identifier_requires_separation(self):
        lex = Lexer('"str"identifier_goes_here')
        lex.next_token() # match the string
        with self.assertRaises(UnexpectedCharacterError):
            # Die because the identifier isn't separated from the string
            lex.next_token()

    def test_string_requires_separation(self):
        lex = Lexer('"string 1""string 2"')
        lex.next_token() # match the first string
        with self.assertRaises(UnexpectedCharacterError):
            # Die because the strings aren't separated
            lex.next_token()



class IdentifierValueType(unittest.TestCase):
    """
    Tests that the get_value_token_type recognises identifiers
    """

    def check(self, value):

        self.assertEquals(Lexer.get_value_token_type(value,False), Lexer.IDENTIFIER)

    def check_not_equals(self, value):
        self.assertNotEquals(Lexer.get_value_token_type(value,False), Lexer.IDENTIFIER)

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

class FloatValueType(unittest.TestCase):
    """
    Tests that the get_value_token_type recognises Floats
    """

    def check(self, value):
        self.assertEquals(Lexer.get_value_token_type(value,False), Lexer.FLOAT)

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

class IntegerValueType(unittest.TestCase):
    """
    Tests that the get_value_token_type recognises integers.
    """

    def check(self, value):
        self.assertEquals(Lexer.get_value_token_type(value,False), Lexer.INTEGER)

    def check_not_equals(self, value):
        self.assertNotEquals(Lexer.get_value_token_type(value,False), Lexer.INTEGER)

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

class DateValueType(unittest.TestCase):
    """
    Tests that the get_value_token_type recognises dates
    """

    def check(self, value):
        self.assertEquals(Lexer.get_value_token_type(value,False), Lexer.DATE)

    def check_not_equals(self, value):
        self.assertNotEquals(Lexer.get_value_token_type(value,False), Lexer.DATE)

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

class StringValueType(unittest.TestCase):
    """
    Tests that the get_value_token_type recognises strings
    """

    def check(self, value):
        self.assertEquals(Lexer.get_value_token_type(value,False), Lexer.STRING)

    def test_string(self):
        self.check('"test"')

    def test_string_with_double_quote(self):
        self.check(r'"\""')

    def test_string_with_single_quote(self):
        self.check(r'"\'"')

    def test_string_with_newline(self):
        self.check(r'"Hello\nWorld!"')

    def test_string_with_backslash(self):
        self.check(r'"Hello\\World!"')

    def test_string_with_bell(self):
        self.check(r'"Hello\aWorld!"')

    def test_string_with_backspace(self):
        self.check(r'"Hello\bWorld!"')

    def test_string_with_tab(self):
        self.check(r'"Hello\tWorld!"')

    def test_string_with_form_feed(self):
        self.check(r'"Hello\fWorld!"')

    def test_string_with_carriage_return(self):
        self.check(r'"Hello\rWorld!"')

    def test_string_with_vertical_tab(self):
        self.check(r'"Hello\vWorld!"')

    def test_string_with_hex_ascii(self):
        self.check(r'"Hello\x5cWorld!"')

    def test_string_with_oct_ascii(self):
        self.check(r'"Hello\134World!"')