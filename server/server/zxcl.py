# coding=utf-8
"""
Parser for ZXCL
"""
from datetime import datetime
import re

__author__ = 'david'

# Some regexes for lexing
float_regex = re.compile("(\+|\-)?((\d+\.\d*((e|E)(\+|\-)?\d+)?)|(\.\d+((e|E)(\+|\-)?\d+)?)|(\d+(e|E)(\+|\-)?\d+))")
int_regex = re.compile("(\+|\-)?\d+")
identifier_regex = re.compile("([a-z]|[A-Z]|_)([a-z]|[A-Z]|[0-9]|_)*")
date_regex = re.compile("(\d\d?\-(([A-Z]|[a-z]){3})\-\d\d\d\d)")
string_regex = re.compile(r'"((\\"|[^")])*)"')

# The float regex is looking for:
#    FLOAT
#    |   ('+'|'-')? ('0'..'9')+ '.' ('0'..'9')* (('e'|'E') ('+'|'-')? ('0'..'9')+)?
#    |   ('+'|'-')? '.' ('0'..'9')+ (('e'|'E') ('+'|'-')? ('0'..'9')+)?
#    |   ('+'|'-')? ('0'..'9')+ ('e'|'E') ('+'|'-')? ('0'..'9')+
#    ;




class Token(object):
    """
    A token
    """

    type = None
    value = None
    position = None

    def __init__(self, type, value, position):
        self.type = type
        self.value = value
        self.position = position

    def name(self):
        """
        Returns a friendly name for the token
        """
        return Lexer.token_names[self.type].title()

    def __str__(self):
        return "<{value},{type_name}>".format(
            value=self.value,
            type_name=Lexer.token_names[self.type])

class LexerError(Exception):
    """
    problems in the lexer.
    """
    pass

class Lexer(object):
    """
    Tokeniser for ZXCL
    """

    EOF = 0
    IDENTIFIER = 1
    INTEGER = 2
    FLOAT = 3
    WS = 4
    STRING = 5
    DATE = 6
    COMMENT = 7

    # Characters
    FORWARD_SLASH = 8 # /
    EQUAL = 9  # =

    token_names = ["EOF","IDENTIFIER","INTEGER","FLOAT","WS","STRING","DATE",
                   "!","/","="]

    _input = None
    _pointer = 0
    _current_character = None


    def __init__(self, input_string):
        """
        Creates a new tokeniser for the specified input string
        """
        self._input = input_string

        if self._input == "":
            self._input = "!"

        self._pointer = 0
        self._current_character = self._input[self._pointer]



    def _consume(self):
        self._pointer += 1
        if self._pointer >= len(self._input):
            self._current_character = None
        else:
            self._current_character = self._input[self._pointer]

    def _consume_count(self, count):
        self._pointer += count
        if self._pointer >= len(self._input):
            self._current_character = None
        else:
            self._current_character = self._input[self._pointer]

    def _match(self, character):
        if self._current_character == character:
            self._consume()
        else:
            raise LexerError("Expected character {expected} but found {found} "
                            "at position {position}"
                .format(expected=character,
                        found=self._current_character,
                        position=self._pointer))

    def _look_ahead(self, count):
        if self._pointer + count > len(self._input):
            return None
        else:
            return self._input[self._pointer + count]

    def _white_space(self):
        while self._current_character in [' ', '\t', '\n', '\r']:
            self._consume()

    prev_separator_seen = False
    separator_seen = True # The start of the string is a separator

    def next_token(self):
        """
        Gets the next token in the input string.
        """

        while self._current_character is not None:
            char = self._current_character

            self.prev_separator_seen = self.separator_seen
            self.separator_seen = False

            if char == '!':
                # A comment
                self._pointer = len(self._input)
                self._current_character = None
                continue
            elif char in [' ', '\t', '\n', '\r']:
                self._white_space()
                self.separator_seen = True
                continue
            elif char == '=':
                pos = self._pointer
                self._consume()
                self.separator_seen = True
                return Token(self.EQUAL, char, pos)
            elif char == '/':
                pos = self._pointer
                self._consume()
                self.separator_seen = True
                return Token(self.FORWARD_SLASH, char, pos)

            # These are all types of some form (identifiers, integers, etc)
            # and they need some form of separator before them
            elif self.prev_separator_seen:
                # Regex time. We are looking for:
                #  - ints
                #  - floats
                #  - strings
                #  - identifiers

                substring = self._input[self._pointer:]

                match = date_regex.match(substring)
                if match is not None:
                    # It looks like a date...
                    value = match.group(0)
                    try:
                        date_value = datetime.strptime(value, '%d-%b-%Y').date()
                        pos = self._pointer
                        self._consume_count(match.end())
                        return Token(self.DATE, date_value, pos)
                    except ValueError:
                        # bad date. No match.
                        pass

                # Try the float first.
                match = float_regex.match(substring)
                if match is not None:
                    # Its a float.
                    pos = self._pointer
                    self._consume_count(match.end())
                    return Token(self.FLOAT, float(match.group(0)), pos)

                match = int_regex.match(substring)
                if match is not None:
                    # Its an int
                    pos = self._pointer
                    self._consume_count(match.end())
                    return Token(self.INTEGER, int(match.group(0)), pos)

                match = identifier_regex.match(substring)
                if match is not None:
                    # Its an identifier
                    pos = self._pointer
                    self._consume_count(match.end())
                    return Token(self.IDENTIFIER, match.group(0), pos)

                match = string_regex.match(substring)
                if match is not None:
                    # Its a string

                    # group 0 has quote marks, group 1 does not.
                    string_value = match.group(1)

                    #TODO: handle escape sequences
                    pos = self._pointer
                    self._consume_count(match.end())
                    return Token(self.STRING, string_value, pos)

                raise LexerError("Unexpected character {char} "
                                 "at position {pos}".format(
                    char=char,pos=self._pointer))
            else:
                raise LexerError("Unexpected character {char} "
                                 "at position {pos}".format(
                    char=char,pos=self._pointer))



        return Token(self.EOF, "<EOF>", self._pointer)

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

    # TODO: Consider dumping these once the command processor is working.
    # They can be rebuilt from the combined list if necessary anyway.
    verb = None
    keyword = None
    qualifiers = []
    parameters = []

    # This is what the command processor will look at as ordering is important
    # for handling syntax changes
    combined = []
    # It is a list of tuples between 2 and 3 items long.
    # Item 0: either v - verb
    #             or p - parameter
    #             or q - qualifier
    # Item 1: if [0]=="v": verb name
    #         if [0]=="p": parameter value
    #         if [0]=="q": qualifier name
    # Item 3: Only present if [0]=="q". In which case its the qualifiers value.
    #         None means the qualifier had no value.

    @staticmethod
    def _tokenise(string):
        # TODO: Maybe move this to Lexer
        tokens = []

        lex = Lexer(string)

        while True:
            tok = lex.next_token()

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
        self.verb = None
        self.keyword = None
        self.qualifiers = []
        self.parameters = []

        # String was entirely whitespace or comments.
        if self._look_ahead.type == Lexer.EOF:
            return

        # Parse a command
        self._command()

    def _command(self):

        # A command starts with a verb
        self._verb()

        # Optionally followed by a keyword
        if self._look_ahead.type == Lexer.IDENTIFIER:
            self.keyword = self._match(Lexer.IDENTIFIER)

        # Then potentially many qualifiers and parameters
        while self._look_ahead.type != Lexer.EOF:
            if self._look_ahead.type == Lexer.FORWARD_SLASH:
                self._qualifier()
            elif self._look_ahead.type in self._parameter_types:
                param = self._parameter()
                self.parameters.append(param)
                self.combined.append(("p",param))
            else:
                raise ParserError("Expected qualifier or parameter at position {0}".format(self._look_ahead.position))


    def _consume(self):
        self._position += 1
        self._look_ahead = self._tokens[self._position]

    def _verb(self):

        val = self._match(Lexer.IDENTIFIER)

        if val is None:
            raise ParserError("Verb expected at position {0}, got {1}".format(
                self._look_ahead.position,
                self._look_ahead.name()
            ))

        self.verb = val
        self.combined.append(("v",val))

    def _qualifier(self):
        self._match(Lexer.FORWARD_SLASH)

        qualifier_name = self._match(Lexer.IDENTIFIER)
        qualifier_value = None

        if qualifier_name is None:
            raise ParserError("Qualifier name expected at position {0}".format(
                self._look_ahead.position))

        if self._look_ahead.type == Lexer.EQUAL:
            # We have a parameter!
            self._match(Lexer.EQUAL)
            qualifier_value = self._parameter()

        self.qualifiers.append((qualifier_name,qualifier_value))
        self.combined.append(("q",qualifier_name, qualifier_value))

    def _parameter(self):
        if self._look_ahead.type in self._parameter_types:
            val = self._look_ahead.value
            self._consume()
            return val
        else:
            raise ParserError("Expected keyword, integer, float, string "
                            "or date at position {0}".format(
                self._look_ahead.position))

class CommandProcessor(object):
    """
    Processes ZXCL commands
    """

    def __init__(self, verb_table, syntax_table, keyword_table):
        """
        Initialises the command processor with the supplied command table.
        :param verb_table: A table of verbs
        :type verb_table: dict
        :param syntax_table: A table of command syntaxes
        :type syntax_table: dict
        :param keyword_table: A table of keyword types
        :type keyword_table: dict
        """

        self.verbs_table = verb_table
        self.syntax_table = syntax_table
        self.keyword_table = keyword_table

        self.parser = Parser()


    def process_command(self, command_string, prompt_callback):
        """
        Processes a command.
        :param command_string: The command to process.
        :param prompt_callback: Function to call if required parameters or
        qualifiers need to be prompted for.
        :type prompt_callback: callable or None
        :return: A callable which should be called to execute the command
        :rtype: callable.
        """

        try:
            parser.parse(command_string)
        except LexerError as le:
            pass

