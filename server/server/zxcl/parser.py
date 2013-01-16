# coding=utf-8
"""
The ZXCL Parser and related bits.
"""
from lexer import Lexer, UnexpectedCharacterError

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

    _tokens = None
    _position = None
    _look_ahead = None

    _parameter_types = [
        Lexer.IDENTIFIER,
        Lexer.INTEGER,
        Lexer.FLOAT,
        Lexer.STRING,
        Lexer.DATE,
        ]

    # This is what the command processor will look at as ordering is important
    # for handling syntax changes
    _combined = []
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
    COMP_TYPE_VERB="v"
    COMP_TYPE_PARAMETER="p"
    COMP_TYPE_QUALIFIER="q"

    @staticmethod
    def _tokenise(string):
        # TODO: Maybe move this to Lexer
        tokens = []

        lex = Lexer(string)

        while True:
            try:
                tok = lex.next_token()
            except UnexpectedCharacterError as e:
                # translate the lexer error into a parser error.
                raise ParserError(e.message)

            # Store the token
            tokens.append(tok)

            # If we're finished...
            if tok.type == Lexer.EOF:
                return tokens

    def _match(self, token):

        if self._look_ahead.type == token:
            val = self._look_ahead.value
            self._consume()
            return val
        else:
            return None

    def _whitespace(self):
        """
        Sometimes whitespace is required (eg, to separate parameters). This
        enforces whitespace where as _match just skips over it.
        :returns: True if whitespace was found, false otherwise
        :rtype: bool
        """
        if self._look_ahead.type == Lexer.WS:
            self._consume()
            return True
        else:
            return False

    def parse(self, string):
        """
        Parses a ZXCL command
        """

        self._look_ahead = None

        self._tokens = Parser._tokenise(string)

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

        # A command starts with a verb
        self._verb()

        parameter_number = 0

        # Then potentially many qualifiers and parameters
        while self._look_ahead.type != Lexer.EOF:
            if self._look_ahead.type == Lexer.FORWARD_SLASH:
                self._qualifier()
            elif self._look_ahead.type in self._parameter_types:
                pos = self._look_ahead.position
                param,type = self._parameter()

                self._store_component(Parser.COMP_TYPE_PARAMETER, pos, parameter_number, (param,type))

                parameter_number += 1
            else:
                raise ParserError("Expected qualifier or parameter at position {0}".format(self._look_ahead.position))


    def _consume(self):
        self._position += 1
        self._look_ahead = self._tokens[self._position]

    def _verb(self):

        pos = self._look_ahead.position
        val = self._match(Lexer.IDENTIFIER)

        if val is None:
            raise ParserError("Verb expected at position {0}, got {1}".format(
                self._look_ahead.position,
                self._look_ahead.name()
            ))

        self._store_component(Parser.COMP_TYPE_VERB, pos, val)

    def _qualifier(self):
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
            qualifier_value, qualifier_type = self._parameter()

            qual_val = (qualifier_value,qualifier_type)

        self._store_component(
            Parser.COMP_TYPE_QUALIFIER,
            pos,
            qualifier_name,
            qual_val
        )

    def _parameter(self):

        # TODO: dict this?
        if self._look_ahead.type == Lexer.INTEGER:
            type = "int"
        elif self._look_ahead.type == Lexer.IDENTIFIER:
            type = "keyword"
        elif self._look_ahead.type == Lexer.FLOAT:
            type = "float"
        elif self._look_ahead.type == Lexer.STRING:
            type = "string"
        elif self._look_ahead.type == Lexer.DATE:
            type = "date"
        else:
            raise ParserError("Expected keyword, integer, float, string "
                              "or date at position {0}".format(
                self._look_ahead.position))


        val = self._look_ahead.value

        self._consume()

        return val,type

