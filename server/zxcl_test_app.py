# coding=utf-8
from server.zxcl.processor import CommandProcessor


__author__ = 'david'


verbs = {
    "show": {
        "syntax": "show_syntax"
    },

    # This is a synonym for
    "synonym": {
        "verb": "abacus"
    }
}

show_param_0 = {
    "type": "keyword",
    "prompt": "What?",
    "required": True,
    "keywords": "show_keywords"
}


syntax = {
    "show_syntax": {
        "parameters": {
            0: show_param_0
        }
    },
    "show_system": {
        "parameters": {
            0: show_param_0
        },
        "handler": "system_handler"
    },
    "show_statistics": {
        "parameters": {
            0: show_param_0,
            1: {
                "type": "int"
            }
        },
        "qualifiers": {
            "date": {
                "value_required": True,
                "type": "date"
            },
            "full": {
                "type": None # No value
            }
        },
        "handler": "statistics_handler"
    }
}

keywords = {
    "show_keywords": {
        ("system", "show_system"),
        ("statistics", "show_statistics")
    }
}

processor = CommandProcessor(verbs,syntax,keywords)


def test_command(command):

    print("Command: {0}".format(command.strip()))
    handler, params, qualifiers = processor.process_command(command, None)

    print("Handler: {0}".format(handler))
    print("Parameters: {0}".format(params))
    print("Qualifiers: {0}".format(qualifiers))

test_command("""
show system/full/date=12-JAN-2013
""")


#
#test_string = r"""
#TEST COMMAND/QUAL /QUAL2=123 /QUAL3=this_is_a_keyword
#/some_date=14-JUN-2012 /QUAL5=12.2 /QUAL6="String value\" ok"
#123 another_keyword 12-OCT-2013 12.2 "Another\"String\""
#"""
##
##lex = Lexer(test_string)
##
##while True:
##    tok = lex.next_token()
##    print(tok)
##    if tok.type == Lexer.EOF:
##        break
#
#parser = Parser()
#
#parser.parse(test_string)
#print parser.verb
#print parser.keyword
#print parser.qualifiers
#print parser.parameters
#print "---"
#print parser.combined
#
#while True:
#    value = raw_input(">")
#    try:
#        parser.parse(value)
#
#        print parser.verb
#        print parser.keyword
#        print parser.qualifiers
#        print parser.parameters
#    except Exception as e:
#        print("Error: " + e.message)
#
#
