# coding=utf-8
"""
The ZXCL Lexer and related bits
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


class UnexpectedCharacterError(Exception):
    """
    Thrown when the parser finds a character that it wasn't expecting to
    find there. This likely means that it got to the end of its list of
    tokens without anything matching.
    """
    def __init__(self, character, position):
        self.message = "Unexpected character {char} at position {pos}".format(
            char=character,
            pos=position
        )
        self.character = character
        self.position = position


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

                raise UnexpectedCharacterError("Unexpected character {char} "
                                 "at position {pos}".format(
                    char=char,pos=self._pointer), self._pointer)
            else:
                raise UnexpectedCharacterError("Unexpected character {char} "
                                 "at position {pos}".format(
                    char=char,pos=self._pointer), self._pointer)



        return Token(self.EOF, "<EOF>", self._pointer)

    @staticmethod
    def tokenise(string):
        """
        Tokenises the entire string and returns it as a list of tokens
        :param string: String to tokenise
        :type string: str or unicode
        :returns: A list of tokens
        :rtype: list
        """
        tokens = []

        lex = Lexer(string)

        while True:
            tok = lex.next_token()

            # Store the token
            tokens.append(tok)

            # If we're finished...
            if tok.type == Lexer.EOF:
                return tokens