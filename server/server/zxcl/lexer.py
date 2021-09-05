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
string_regex = re.compile(r'"((\\"|[^"])*)"')

# These regexes are used for determining raw value types entered at user
# prompts. These only differ from the lexing regexes above in that they match
# the end of the string (they all end in '$')
float_regex_v = re.compile("(\+|\-)?((\d+\.\d*((e|E)(\+|\-)?\d+)?)|(\.\d+((e|E)(\+|\-)?\d+)?)|(\d+(e|E)(\+|\-)?\d+))$")
int_regex_v = re.compile("(\+|\-)?\d+$")
identifier_regex_v = re.compile("([a-z]|[A-Z]|_)([a-z]|[A-Z]|[0-9]|_)*$")
date_regex_v = re.compile("(\d\d?\-(([A-Z]|[a-z]){3})\-\d\d\d\d)$")
string_regex_v = re.compile(r'"((\\"|[^"])*)"$')

# The float regex is looking for:
#    FLOAT
#    |   ('+'|'-')? ('0'..'9')+ '.' ('0'..'9')* (('e'|'E') ('+'|'-')? ('0'..'9')+)?
#    |   ('+'|'-')? '.' ('0'..'9')+ (('e'|'E') ('+'|'-')? ('0'..'9')+)?
#    |   ('+'|'-')? ('0'..'9')+ ('e'|'E') ('+'|'-')? ('0'..'9')+
#    ;


# Ref: https://stackoverflow.com/questions/14820429/how-do-i-decodestring-escape-in-python3
def string_escape(s, encoding='utf-8'):
    return (s.encode('latin1')          # To bytes, required by 'unicode-escape'
             .decode('unicode-escape')  # Perform the actual octal-escaping decode
             .encode('latin1')          # 1:1 mapping back to bytes
             .decode(encoding))         # Decode original encoding


class Token(object):
    """
    A token
    """

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

                    # Handle escape sequences
                    string_value = string_escape(string_value)

                    pos = self._pointer
                    self._consume_count(match.end())
                    return Token(self.STRING, string_value, pos)

                raise UnexpectedCharacterError(char, self._pointer)
            else:
                raise UnexpectedCharacterError(char, self._pointer)



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

    @staticmethod
    def get_value_token_type(value, string_default):
        """
        Gets the token type for the supplied value string. This should be
        either a date, float, integer, identifier or a string.
        :param value: Value
        :type value: str or unicode
        :param string_default: If the value should be treated as a string when
        nothing else matches
        :type string_default: bool
        :return: Token type
        :rtype: int
        """
        match = date_regex_v.match(value)
        if match is not None:
            # It looks like a date...
            value = match.group(0)
            try:
                datetime.strptime(value, '%d-%b-%Y').date()
                return Lexer.DATE
            except ValueError:
                # bad date. No match.
                pass

            # Try the float first.
        match = float_regex_v.match(value)
        if match is not None:
            # Its a float.
            return Lexer.FLOAT

        match = int_regex_v.match(value)
        if match is not None:
            # Its an int
            return Lexer.INTEGER

        match = identifier_regex_v.match(value)
        if match is not None:
            # Its an identifier
            return Lexer.IDENTIFIER

        match = string_regex_v.match(value)
        if match is not None:
            # Its a string
            return Lexer.STRING

        # Unknown value type
        if string_default:
            return Lexer.STRING
        return None