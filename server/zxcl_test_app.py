# coding=utf-8
from server.zxcl.processor import CommandProcessor


__author__ = 'david'


verbs = {
    "SET": {
        "syntax": "set_syntax"
    },
    "SHOW": {
        "syntax": "show_syntax"
    },
    "STARTTLS": {
        "syntax": "starttls_syntax"
    },
    "LOGIN": {
        "syntax": "login_syntax"
    },
    "STREAM": {
        "syntax": "stream_syntax"
    },
    "SUBSCRIBE": { # Synonym for stream
        "verb": "STREAM"
    },
    "LOGOUT": {
        "syntax": "logout_syntax"
    },
    "QUIT": {
        "syntax": "quit_syntax"
    },
}

show_param_0 = {
    "type": "keyword",
    "prompt": "What?",
    "required": True,
    "keywords": "show_keywords"
}

set_param_0 = {
    "type": "keyword",
    "prompt": "What?",
    "required": True,
    "keywords": "set_keywords"
}


syntax = {
    "show_syntax": {
        "parameters": {
            0: show_param_0
        }
    },
    "set_syntax": {
        "parameters": {
            0: set_param_0
        }
    },
    "starttls_syntax": {
        "no_parameters": True,
        "no_qualifiers": True,
        "handler": "starttls_handler"
    },
    "login_syntax": {
        "no_parameters": True,
        "qualifiers": {
            "USERNAME": {
                "value_required": True,
                "type": "string"
            },
            "PASSWORD": {
                "type": "string"
            }
        },
        "handler": "login_handler"
    },
    "stream_syntax": {
        "no_parameters": True,
        "handler": "subscribe_handler",
        "qualifiers": {
            "UP": {
                "type": None,
                "syntax": "stream_up_syntax"
            },
            "STATION": {
                "type": "string",
                "value_required": True,
            },
            "SAMPLES": {
                "type": None
            },
            "LIVE": {
                "type": None
            },
            "CATCHUP": {
                "type": None
            },

            "DATE": {
                "type": "date",
                "requires": ["CATCHUP"]
            },
            "TIME": {
                "type": "string",
                "requires": ["CATCHUP"]
            }

        }
    },
    "stream_up_syntax": {
        "no_parameters": True,
        "handler": "stream_up_handler",
    },
    "logout_syntax": {
        "no_parameters": True,
        "no_qualifiers": True,
        "handler": "logout_handler"
    },
    "quit_syntax": {
        "no_parameters": True,
        "no_qualifiers": True,
        "handler": "quit_handler"
    },
    "show_compatibility": {
        "parameters": {
            0: show_param_0
        },
        "handler": "show_compatibility_handler"
    },
    "set_client": {
        "parameters": {
            0: set_param_0
        },
        "qualifiers": {
            "NAME": {
                "required": True,
                "type": "string",
                "value_required": True
            },
            "VERSION": {
                "type": "string",
                "value_required": True
            }
        },
        "handler": "set_client_handler"
    },
    "set_interface": {
        "parameters": {
            0: set_param_0
        },
        "qualifiers": {
            "CODED": {
                "type": None
            }
        },
        "handler": "set_interface_handler"
        # TODO: require at least one qualifier
    }
}

keywords = {
    "show_keywords": {
        ("COMPATIBILITY", "show_compatibility"),
    },
    "set_keywords": {
        ("CLIENT", "set_client"),
        ("INTERFACE", "set_interface")
    }
}

processor = CommandProcessor(verbs,syntax,keywords, None, None)


def test_command(command):

    print("\nCommand: {0}".format(command.strip()))
    try:
        handler, params, qualifiers = processor.process_command(command)
        print("Handler: {0}".format(handler))
        print("Parameters: {0}".format(params))
        print("Qualifiers: {0}".format(qualifiers))
    except Exception as e:
        print("Error: {0}".format(e.message))


test_command('set client/name="test app"/version="1.0.0"')
test_command('set interface/coded')
test_command('show compatibility')
test_command('starttls')
test_command('login/username="foo"/password="bar"')
test_command('stream/up')
test_command('logout')
test_command('quit')

# ANY2
# NEG
# NOT
# AND
# OR

# DISALLOW (DATE OR TIME) AND NOT CATCHUP

allows = (

)