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

    print("\nCommand: {0}".format(command.strip()))
    try:
        handler, params, qualifiers = processor.process_command(command, None)
        print("Handler: {0}".format(handler))
        print("Parameters: {0}".format(params))
        print("Qualifiers: {0}".format(qualifiers))
    except Exception as e:
        print("Error: {0}".format(e.message))


test_command("""
show system/full/date=12-JAN-2013
""")

test_command("""
show statistics/full/date=12-JAN-2013
""")

test_command("""
show statistics/full/date=12-JAN-2013 42
""")