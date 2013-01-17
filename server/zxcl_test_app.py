# coding=utf-8
from server.zxcl.command_table import keyword_table, keyword_set, keyword, verb_table, verb, synonym, parameter, syntax_table, syntax, qualifier
from server.zxcl.processor import CommandProcessor


__author__ = 'david'


verb_tbl = verb_table([
    verb(name="set", syntax="set_syntax"),
    verb(name="show", syntax="show_syntax"),
    verb(name="starttls", syntax="starttls_syntax"),
    verb(name="login", syntax="login_syntax"),
    verb(name="stream", syntax="stream_syntax"),
    synonym(name="subscribe", verb="stream"),
    verb(name="logout", syntax="logout_syntax"),
    verb(name="quit", syntax="quit_syntax")
])

show_param_0 = parameter(
    position=0,
    type="keyword",
    prompt="What?",
    required=True,
    keywords="show_keywords"
)

set_param_0 = parameter(
    position=0,
    type="keyword",
    prompt="What?",
    required=True,
    keywords="set_keywords"
)

syntax_tbl = syntax_table([
    syntax(name="show_syntax", parameters=[show_param_0]),
    syntax(name="set_syntax", parameters=[set_param_0]),
    syntax(
        name="starttls_syntax",
        deny_parameters=True,
        deny_qualifiers=True,
        handler="starttls_handler"
    ),
    syntax(
        name="login_syntax",
        deny_parameters=True,
        handler="login_handler",
        qualifiers=[
            qualifier(
                name="username",
                value_required=True,
                type="string"
            ),
            qualifier(
                name="password",
                type="string"
            )
        ]
    ),
    syntax(
        name="stream_syntax",
        deny_parameters=True,
        handler="subscribe_handler",
        qualifiers=[
            qualifier(name="up", syntax="stream_up_syntax"),
            qualifier(name="station", type="string", value_required=True),
            qualifier(name="samples"),
            qualifier(name="live"),
            qualifier(name="catchup"),
            qualifier(name="date", type="date"),
            qualifier(name="time", type="string")
        ]
    ),
    syntax(
        name="stream_up_syntax",
        deny_parameters=True,
        handler="stream_up_handler"
    ),
    syntax(
        name="logout_syntax",
        deny_parameters=True,
        deny_qualifiers=True,
        handler="logout_handler"
    ),
    syntax(
        name="quit_syntax",
        deny_parameters=True,
        deny_qualifiers=True,
        handler="quit_handler"
    ),
    syntax(
        name="show_compatibility",
        parameters=[show_param_0],
        handler="show_compatibility_handler"
    ),
    syntax(
        name="set_client",
        handler="set_client_handler",
        parameters=[
            set_param_0,
            parameter(
                position=1,
                type="string",
                required=True,
                prompt="Name:"
            )
        ],
        qualifiers=[
            qualifier(
                name="version",
                type="string",
                value_required=True
            )
        ]
    ),
    syntax(
        name="set_interface",
        handler="set_interface_handler",
        parameters=[set_param_0],
        qualifiers=[qualifier(name="coded")]
    )
])

keyword_tbl = keyword_table([
    keyword_set("show_keywords", [
        keyword(value="compatibility", syntax="show_compatibility"),
    ]),
    keyword_set("set_keywords", [
        keyword(value="client", syntax="set_client"),
        keyword(value="interface", syntax="set_interface"),
    ])
])

def prompter(prompt):
    return raw_input(prompt + " ")

processor = CommandProcessor(verb_tbl, syntax_tbl, keyword_tbl, prompter, None)


def run_command(command):
    handler, params, qualifiers = processor.process_command(command)
    print("Handler: {0}".format(handler))
    print("Parameters: {0}".format(params))
    print("Qualifiers: {0}".format(qualifiers))

def test_command(command, dont_print_command=False):

    if not dont_print_command:
        print("\nCommand: {0}".format(command.strip()))
    try:
        run_command(command)
    except Exception as e:
        print("Error: {0}".format(e.message))

test_command('set client "test app"/version="1.0.0"')
test_command('set interface/coded')
test_command('show compatibility')
test_command('starttls')
test_command('login/username="foo"/password="bar"')
test_command('stream/up')
test_command('logout')
test_command('quit')

while True:
    test_command(raw_input("$ "), True)

# ANY2
# NEG
# NOT
# AND
# OR

# DISALLOW (DATE OR TIME) AND NOT CATCHUP

allows = (

)