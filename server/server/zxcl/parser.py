# coding=utf-8
"""
The ZXCL Parser and related bits.
"""
from lexer import Lexer

__author__ = 'david'


class ParserError(Exception):
    """
    Problems in the parser.
    """
    pass


class Parser(object):
    """
    Parses ZXCL stuff
    """

    _parameter_types = [
        Lexer.IDENTIFIER,
        Lexer.INTEGER,
        Lexer.FLOAT,
        Lexer.STRING,
        Lexer.DATE,
        ]

    COMP_TYPE_VERB="v"
    COMP_TYPE_PARAMETER="p"
    COMP_TYPE_QUALIFIER="q"

    _value_types = {
        Lexer.INTEGER: "int",
        Lexer.IDENTIFIER: "keyword",
        Lexer.FLOAT: "float",
        Lexer.STRING: "string",
        Lexer.DATE: "date"
    }

    def __init__(self):
        self._tokens = None
        self._position = None
        self._look_ahead = None

        # This is what the command processor will look at as ordering is important
        # for handling syntax changes
        self._combined = []
        # It is a list of 4-item tuples.
        # Item 0: type - either v - verb
        #                    or p - parameter
        #                    or q - qualifier
        # Item 1: position in the command
        # Item 2: if [0]=="v": verb name
        #         if [0]=="p": parameter number
        #         if [0]=="q": qualifier name
        # Item 2: if [0]=="p": parameter value, parameter type
        #         if [0]=="q": qualifier value, qualifier type. None,None = no parameter
        #         if [0]=="v": None

    def _match(self, token):

        if self._look_ahead.type == token:
            val = self._look_ahead.value
            self._consume()
            return val
        else:
            return None

    def parse(self, string):
        """
        Parses a ZXCL command
        :param string: The command string to parse
        :type string: str or unicode
        :returns: The parsed command as a list of command components
        :rtype: list
        """

        self._look_ahead = None

        self._tokens = Lexer.tokenise(string)

        self._position = -1
        self._consume()
        self._combined = []

        # String was entirely whitespace or comments.
        if self._look_ahead.type == Lexer.EOF:
            return None

        # Parse a command
        self._command()

        return self._combined

    def _store_component(self, type, position, name, value=None):
        """
        Stores a command in the combined command list.
        :param type: Component type
        :param name: Name of the component
        :param value: Components value if it has one
        :param position: Components position in the command.
        """

        self._combined.append((type,position,name,value))

    def _command(self):
        """
        Recognises a command.
        :raise: A ParserError if something is encountered that isn't a
        qualifier or parameter
        """

        # A command starts with a verb
        self._verb()

        parameter_number = 0

        # Then potentially many qualifiers and parameters
        while self._look_ahead.type != Lexer.EOF:
            if self._look_ahead.type == Lexer.FORWARD_SLASH:
                self._qualifier()
            elif self._look_ahead.type in self._parameter_types:
                self._parameter(parameter_number)
                parameter_number += 1
            else:
                raise ParserError("Expected qualifier or parameter at position {0}".format(self._look_ahead.position))

    def _parameter(self, parameter_number):
        """
        Recognises one parameter.
        :param parameter_number: The parameter number that should be here.
        """
        pos = self._look_ahead.position
        param, type = self._value()

        # Store the parameter
        self._store_component(Parser.COMP_TYPE_PARAMETER, pos, parameter_number,
            (param, type))


    def _consume(self):
        """
        Consumes one token.
        """
        self._position += 1
        self._look_ahead = self._tokens[self._position]

    def _verb(self):
        """
        Recognises a verb. The verb is added to the command components list.
        :return:
        """
        pos = self._look_ahead.position
        val = self._match(Lexer.IDENTIFIER)

        if val is None:
            raise ParserError("Verb expected at position {0}, got {1}".format(
                self._look_ahead.position,
                self._look_ahead.name()
            ))

        self._store_component(Parser.COMP_TYPE_VERB, pos, val)

    def _qualifier(self):
        """
        Recognises one qualifier.
        """
        pos = self._look_ahead.position
        self._match(Lexer.FORWARD_SLASH)

        qualifier_name = self._match(Lexer.IDENTIFIER)
        qual_val = None

        if qualifier_name is None:
            raise ParserError("Qualifier name expected at position {0}".format(
                self._look_ahead.position))

        if self._look_ahead.type == Lexer.EQUAL:
            # We have a parameter!
            self._match(Lexer.EQUAL)
            qualifier_value, qualifier_type = self._value()

            qual_val = (qualifier_value,qualifier_type)

        self._store_component(
            Parser.COMP_TYPE_QUALIFIER,
            pos,
            qualifier_name,
            qual_val
        )

    def _value(self):
        """
        Recognises a value. This could be part of either a parameter or a
        qualifier.
        :return:
        """

        try:
            type = self._value_types[self._look_ahead.type]
        except Exception:
            raise ParserError("Expected keyword, integer, float, string "
                              "or date at position {0}".format(
                self._look_ahead.position))

        val = self._look_ahead.value

        self._consume()

        return val,type

    @staticmethod
    def get_value_type(value, string_default):
        """
        Gets the type of the supplied value
        :param value: The value to get the type for
        :type value: str or unicode
        :param string_default: If the values type should be treated as a string
         if it can't be parsed as anything else.
        :type string_default: bool
        :return: The values type
        :rtype: str
        :raise: Exception if an invalid value is supplied
        """
        return Parser._value_types[Lexer.get_value_token_type(value, string_default)]
