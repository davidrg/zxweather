# coding=utf-8
"""
Defines the command tables for zxweatherd.
"""
from server.commands import ShowUserCommand, ShowClientCommand, SetClientCommand, LogoutCommand
from server.zxcl.command_table import verb_table, verb, parameter, \
    syntax_table, syntax, qualifier, keyword_table, keyword_set, keyword, synonym

__author__ = 'david'


base_verbs = [
    verb(name="set", syntax="set_syntax"),
    verb(name="show", syntax="show_syntax"),
    verb(name="logout", syntax="logout_syntax"),
    synonym(name="quit", verb="logout")
]

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

base_keywords = [
    keyword_set(
        "show_keywords",
        [
            keyword(value="user", syntax="show_user"),
            keyword(value="client", syntax="show_client"),
        ]
    ),
    keyword_set(
        "set_keywords",
        [
            keyword(value="client", syntax="set_client")
        ]
    )
]

base_syntaxes = [
    ##### SHOW #####
    syntax(
        name="show_syntax",
        parameters=[
            show_param_0
        ]
    ),
    syntax(
        name="show_user",
        handler="show_user",
        parameters=[
            show_param_0
        ]
    ),
    syntax(
        name="show_client",
        handler="show_client",
        parameters=[
            show_param_0
        ]
    ),

    ##### SET #####
    syntax(
        name="set_syntax",
        parameters=[
            set_param_0
        ]
    ),
    syntax(
        name="set_client",
        handler="set_client",
        parameters=[
            set_param_0,
            parameter(
                position=1,
                type="string",
                required=True,
                prompt="Client name:",
                label="Client Name"
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

    ##### LOGOUT #####
    syntax(
        name="logout_syntax",
        deny_qualifiers=True,
        deny_parameters=True,
        handler="logout"
    ),
]

base_dispatch = {
    "show_user": ShowUserCommand,
    "show_client": ShowClientCommand,
    "set_client": SetClientCommand,
    "logout": LogoutCommand,
}

# Commands for authenticated users
authenticated_verb_table = verb_table(base_verbs)
authenticated_syntax_table = syntax_table(base_syntaxes)
authenticated_keyword_table = keyword_table(base_keywords)
authenticated_dispatch_table = base_dispatch