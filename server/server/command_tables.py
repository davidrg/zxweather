# coding=utf-8
"""
Defines the command tables for zxweatherd.
"""
from server.commands import ShowUserCommand, ShowClientCommand, \
    SetClientCommand, LogoutCommand, TestCommand, ShowSessionCommand,\
    SetPromptCommand, SetTerminalCommand, SetInterfaceCommand, StreamCommand, \
    ListSessionsCommand, ListStationsCommand, ShowStationCommand, \
    ShowLiveCommand, ShowLatestCommand
from server.data_upload import UploadCommand
from server.zxcl.command_table import verb_table, verb, parameter, \
    syntax_table, syntax, qualifier, keyword_table, keyword_set, keyword, synonym

__author__ = 'david'


base_verbs = [
    verb(name="set", syntax="set_syntax"),
    verb(name="show", syntax="show_syntax"),
    verb(name="logout", syntax="logout_syntax"),
    synonym(name="quit", verb="logout"),
    verb(name="stream", syntax="stream_syntax"),
    synonym(name="subscribe", verb="stream"),
    verb(name="test", syntax="test_syntax"),
    verb(name="list", syntax="list_syntax"),
    verb(name="upload", syntax="upload_syntax"),
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

list_param_0 = parameter(
    position=0,
    type="keyword",
    prompt="What?",
    required=True,
    keywords="list_keywords"
)

base_keywords = [
    keyword_set(
        "show_keywords",
        [
            keyword(value="user", syntax="show_user"),
            keyword(value="client", syntax="show_client"),
            keyword(value="session", syntax="show_session"),
            keyword(value="station", syntax="show_station"),
            keyword(value="live", syntax="show_live"),
            keyword(value="latest", syntax="show_latest")
        ]
    ),
    keyword_set(
        "set_keywords",
        [
            keyword(value="client", syntax="set_client"),
            keyword(value="prompt", syntax="set_prompt"),
            keyword(value="terminal", syntax="set_terminal"),
            keyword(value="interface", syntax="set_interface"),
        ]
    ),
    keyword_set(
        "list_keywords",
        [
            keyword(value="stations", syntax="list_stations"),
            keyword(value="sessions", syntax="list_sessions"),
        ]
    ),
    keyword_set(
        "boolean",
        [
            keyword(value="true"),
            keyword(value="false")
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
    syntax(
        name="show_session",
        handler="show_session",
        parameters=[
            show_param_0
        ],
        qualifiers=[
            qualifier(
                name="id",
                value_required=True,
                type="string"
            )
        ]
    ),
    syntax(
        name="show_station",
        handler="show_station",
        parameters=[
            show_param_0,
            parameter(
                position=1,
                type="string",
                required=True,
                prompt="Station Code:",
                label="Station Code"
            )
        ],

    ),
    syntax(
        name="show_live",
        handler="show_live",
        parameters=[
            show_param_0,
            parameter(
                position=1,
                type="string",
                required=True,
                prompt="Station Code:",
                label="Station Code"
            )
        ],
        deny_qualifiers=True
    ),
    syntax(
        name="show_latest",
        handler="show_latest",
        parameters=[
            show_param_0
        ],
        qualifiers=[
            qualifier(name="json")
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
    syntax(
        name="set_prompt",
        handler="set_prompt",
        parameters=[
            set_param_0,
            parameter(
                position=1,
                type="string",
                required=True,
                prompt="Prompt: ",
                label="Prompt"
            )
        ]
    ),
    syntax(
        name="set_terminal",
        handler="set_terminal",
        parameters=[set_param_0],
        qualifiers=[
            qualifier(name="video"),
            qualifier(name="basic"),
            qualifier(
                name="echo",
                type="keyword",
                default_value="true",
                value_required=False,
                keywords="boolean",
            )
        ]
    ),
    syntax(
        name="set_interface",
        handler="set_interface",
        parameters=[set_param_0],
        qualifiers=[
            qualifier(
                name="coded",
                type="keyword",
                default_value="true",
                value_required=False,
                keywords="boolean"
            )
        ]
    ),

    ##### LIST #####
    syntax(
        name="list_syntax",
        parameters=[list_param_0]
    ),
    syntax(
        name="list_stations",
        handler="list_stations",
        parameters=[list_param_0]
    ),
    syntax(
        name="list_sessions",
        handler="list_sessions",
        parameters=[list_param_0]
    ),

    ##### STREAM/SUBSCRIBE #####
    syntax(
        name="stream_syntax",
        handler="stream",
        parameters=[
            parameter(
                position=0,
                type="string",
                required=True,
                prompt="Station: ",
                label="Station Code"
            )
        ],
        qualifiers=[
            qualifier(name="live"),
            qualifier(name="samples"),
            qualifier(name="from_timestamp", type="string"),
        ]
    ),

    ##### UPLOAD #####
    syntax(
        name="upload_syntax",
        deny_qualifiers=True,
        deny_parameters=True,
        handler="upload"
    ),

    ##### LOGOUT #####
    syntax(
        name="logout_syntax",
        deny_qualifiers=True,
        deny_parameters=True,
        handler="logout"
    ),

    syntax(
        name="test_syntax",
        handler="test_handler"
    ),
]

base_dispatch = {
    "show_user": ShowUserCommand,
    "show_client": ShowClientCommand,
    "show_session": ShowSessionCommand, # auth
    "show_station": ShowStationCommand,
    "show_live": ShowLiveCommand,
    "show_latest": ShowLatestCommand,

    "set_client": SetClientCommand,
    "set_prompt": SetPromptCommand,
    "set_terminal": SetTerminalCommand,
    "set_interface": SetInterfaceCommand,

    "list_sessions": ListSessionsCommand, # auth
    "list_stations": ListStationsCommand,

    "logout": LogoutCommand,

    "stream": StreamCommand,

    "upload": UploadCommand, # auth

    "test_handler": TestCommand, #auth
}

# Commands for authenticated users
std_verb_table = verb_table(base_verbs)
std_syntax_table = syntax_table(base_syntaxes)
std_keyword_table = keyword_table(base_keywords)
std_dispatch_table = base_dispatch

