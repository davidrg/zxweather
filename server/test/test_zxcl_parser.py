# coding=utf-8
"""
Unit tests for the ZXCL Parser
"""
from datetime import datetime
import unittest
from server.zxcl.parser import Parser, ParserError

__author__ = 'david'

class ParserTests(unittest.TestCase):

    def test_verb(self):
        parser = Parser()
        result = parser.parse("verb")

        self.assertEquals(len(result), 1)

        verb = result[0]

        self.assertEquals(verb[0], 'v') # v for verb
        self.assertEquals(verb[1], 0) # position is right at the start
        self.assertEquals(verb[2], "verb") # Name of the verb
        self.assertEquals(verb[3], None) # Verbs don't have a value


    def test_verb_required_at_start(self):
        """
        A command must start with a verb.
        """
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("42") # 42 is not a verb

    def test_reparses(self):
        parser = Parser()

        parser.parse("verb /qual=value parameter")

        result = parser.parse("verb")

        self.assertEquals(len(result), 1)

    def test_returns_None_on_empty_command(self):
        parser = Parser()
        result = parser.parse("! this is a comment")

        self.assertIsNone(result)

    def test_value_type_int(self):
        self.assertEquals(Parser.get_value_type("42", False), "int")

    def test_value_type_float(self):
        self.assertEquals(Parser.get_value_type("42.42", False), "float")

    def test_value_type_keyword(self):
        self.assertEquals(Parser.get_value_type("keyword", False), "keyword")

    def test_value_type_date(self):
        self.assertEquals(Parser.get_value_type("18-JAN-2013", False), "date")

    def test_value_type_quoted_string(self):
        self.assertEquals(Parser.get_value_type('"test~"', False), "string")

    def test_value_type_unquoted_string(self):
        self.assertEquals(Parser.get_value_type('~test~', True), "string")



class ParameterTypes(unittest.TestCase):
    """
    Checks that only valid parameter types are recognised as parameters.
    """
    def test_recognises_int_parameter(self):
        parser = Parser()
        result = parser.parse("verb 42")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'p') # p for parameter
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], 0) # Parameter number
        self.assertEquals(parameter[3][0], 42) # value of the parameter
        self.assertEquals(parameter[3][1], "int") # type of the parameter

    def test_recognises_str_parameter(self):
        parser = Parser()
        result = parser.parse("verb \"test\"")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'p') # p for parameter
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], 0) # Parameter number
        self.assertEquals(parameter[3][0], "test") # value of the parameter
        self.assertEquals(parameter[3][1], "string") # type of the parameter

    def test_recognises_keyword_parameter(self):
        parser = Parser()
        result = parser.parse("verb keyword_of_doom")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'p') # p for parameter
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], 0) # Parameter number
        self.assertEquals(parameter[3][0], "keyword_of_doom") # value of the parameter
        self.assertEquals(parameter[3][1], "keyword") # type of the parameter

    def test_recognises_float_parameter(self):
        parser = Parser()
        result = parser.parse("verb -42.12e+11")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'p') # p for parameter
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], 0) # Parameter number
        self.assertEquals(parameter[3][0], float("-42.12e+11")) # value of the parameter
        self.assertEquals(parameter[3][1], "float") # type of the parameter

    def test_recognises_date_parameter(self):
        parser = Parser()
        result = parser.parse("verb 18-JAN-2013")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'p') # p for parameter
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], 0) # Parameter number
        self.assertEquals(parameter[3][0],
            datetime.strptime("18-JAN-2013", '%d-%b-%Y').date()) # value of the parameter
        self.assertEquals(parameter[3][1], "date") # type of the parameter

    def test_invalid_parameter_value_raises_exception(self):
        parser = Parser()
        with self.assertRaises(ParserError):
            parser.parse("verb =")

class TestQualifiers(unittest.TestCase):
    def test_recognises_bare_qualifier(self):
        parser = Parser()
        result = parser.parse("verb /qual")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for Qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual") # Qualifier number
        self.assertEquals(parameter[3], None) # No value

    def test_qualifier_with_equal_symbol_requires_value(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /qual=")

    def test_recognises_qualifier_with_integer_parameter(self):
        parser = Parser()
        result = parser.parse("verb /qual=42")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual") # Qualifier number
        self.assertEquals(parameter[3][0], 42) # Qualifier value
        self.assertEquals(parameter[3][1], "int") # Qualifier type

    def test_recognises_qualifier_with_keyword_parameter(self):
        parser = Parser()
        result = parser.parse("verb /qual=keyword_goes_here")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual") # Qualifier number
        self.assertEquals(parameter[3][0], "keyword_goes_here")# Qualifier value
        self.assertEquals(parameter[3][1], "keyword") # Qualifier type

    def test_recognises_qualifier_with_float_parameter(self):
        parser = Parser()
        result = parser.parse("verb /qual=+42.127E-12")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual") # Qualifier number
        self.assertEquals(parameter[3][0],
            float("+42.127E-12")) # Qualifier value
        self.assertEquals(parameter[3][1], "float") # Qualifier type

    def test_recognises_qualifier_with_string_parameter(self):
        parser = Parser()
        result = parser.parse("verb /qual=\"this is a string\"")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual") # Qualifier number
        self.assertEquals(parameter[3][0], "this is a string")# Qualifier value
        self.assertEquals(parameter[3][1], "string") # Qualifier type

    def test_recognises_qualifier_with_date_parameter(self):
        parser = Parser()
        result = parser.parse("verb /qual_name12=18-JAN-2013")

        self.assertEquals(len(result), 2)

        parameter = result[1]

        self.assertEquals(parameter[0], 'q') # q for qualifier
        self.assertEquals(parameter[1], 5) # position
        self.assertEquals(parameter[2], "qual_name12") # Qualifier number
        self.assertEquals(parameter[3][0],
            datetime.strptime("18-JAN-2013", '%d-%b-%Y').date()) # Qualifier value
        self.assertEquals(parameter[3][1], "date") # Qualifier type

class ValidQualifierNames(unittest.TestCase):
    """
    Checks that only an identifier will do as a qualifier name
    """
    def test_qualifier_name_cant_be_integer(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /42")

    def test_qualifier_name_cant_be_float(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /42.0")

    def test_qualifier_name_cant_be_date(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /18-JAN-2012")

    def test_qualifier_name_cant_be_string(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /\"foo\"")

    def test_qualifier_name_cant_be_equal_symbol(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /=")

    def test_qualifier_name_cant_be_forward_slash(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb //")

    def test_qualifier_name_required(self):
        parser = Parser()

        with self.assertRaises(ParserError):
            parser.parse("verb /")