# coding=utf-8
from server.zxcl import Parser

__author__ = 'david'


test_string = r"""
TEST COMMAND/QUAL /QUAL2=123 /QUAL3=this_is_a_keyword
/some_date=14-JUN-2012 /QUAL5=12.2 /QUAL6="String value\" ok"
123 another_keyword 12-OCT-2013 12.2 "Another\"String\""
"""
#
#lex = Lexer(test_string)
#
#while True:
#    tok = lex.next_token()
#    print(tok)
#    if tok.type == Lexer.EOF:
#        break

parser = Parser()

parser.parse(test_string)
print parser.verb
print parser.keyword
print parser.qualifiers
print parser.parameters
print "---"
print parser.combined

while True:
    value = raw_input(">")
    try:
        parser.parse(value)

        print parser.verb
        print parser.keyword
        print parser.qualifiers
        print parser.parameters
    except Exception as e:
        print("Error: " + e.message)


