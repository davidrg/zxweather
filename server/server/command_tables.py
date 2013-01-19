# coding=utf-8
"""
Defines the command tables for zxweatherd.
"""
from server.commands import setenv, show_user, set_client, show_client
from server.zxcl.command_table import verb_table, verb, synonym, parameter, \
    syntax_table, syntax, qualifier, keyword_table, keyword_set, keyword

__author__ = 'david'


base_verbs = [
    verb(name="set", syntax="set_syntax"),
    verb(name="show", syntax="show_syntax")
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
    )
]

base_dispatch = {
    "show_user": show_user,
    "show_client": show_client,
    "set_client": set_client,
}

# These commands are used by shells to setup the dispatcher as required.
shell_init_verb_table = verb_table([verb("set_env","set_env")])
shell_init_syntax_table = syntax_table([
    syntax(
        name="set_env",
        handler="shinit_set_env",
        parameters=[
            parameter(position=0,type="string",required=True),
            parameter(position=1,type="string",required=True)
        ])
])
shell_init_keyword_table = {}
shell_init_dispatch_table = {
    "shinit_set_env": setenv
}

# Commands for authenticated users
authenticated_verb_table = verb_table(base_verbs)
authenticated_syntax_table = syntax_table(base_syntaxes)
authenticated_keyword_table = keyword_table(base_keywords)
authenticated_dispatch_table = base_dispatch