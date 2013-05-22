# coding=utf-8
"""
Functions for building command tables
"""
__author__ = 'david'

# Constants for the various command table keys
K_VERB_NAME = "name"
K_VERB_SYNTAX = "syntax"
K_VERB_VERB = "verb"

K_SYNTAX_NAME = "name"
K_SYNTAX_HANDLER = "handler"
K_SYNTAX_NO_PARAMETERS = "no_parameters"
K_SYNTAX_NO_QUALIFIERS = "no_qualifiers"
K_SYNTAX_PARAMETERS = "parameters"
K_SYNTAX_QUALIFIERS = "qualifiers"

K_PARAMETER_POSITION = "position"
K_PARAMETER_TYPE = "type"
K_PARAMETER_PROMPT = "prompt"
K_PARAMETER_REQUIRED = "required"
K_PARAMETER_DEFAULT = "default"
K_PARAMETER_KEYWORDS = "keywords"
K_PARAMETER_LABEL = "label"

K_QUALIFIER_NAME = "name"
K_QUALIFIER_TYPE = "type"
K_QUALIFIER_DEFAULT_VALUE = "default_value"
K_QUALIFIER_VALUE_REQUIRED = "value_required"
K_QUALIFIER_DEFAULT = "default"
K_QUALIFIER_SYNTAX = "syntax"
K_QUALIFIER_KEYWORDS = "keywords"

K_KEYWORD_SET_NAME = "name"
K_KEYWORD_SET_KEYWORDS = "keywords"

def verb(name,syntax):
    """
    Defines a new verb.
    :param name: The name of the verb.
    :type name: str
    :param syntax: Name of the default syntax for the verb
    :type syntax: str
    :return: The new verb
    :rtype: dict
    """

    return {
        K_VERB_NAME: name.upper(),
        K_VERB_SYNTAX: syntax
    }

def synonym(name, verb):
    """
    Defines a synonym for another verb
    :param name: Synonym name
    :type name: str
    :param verb: Name of the verb this is a synonym for
    :type verb: str
    :rtype: dict
    """
    return {
        K_VERB_NAME: name.upper(),
        K_VERB_VERB: verb.upper()
    }

def syntax(name, handler = None, deny_parameters=False, deny_qualifiers=False,
           parameters = None, qualifiers = None):
    """
    Defines a new command syntax
    :param name: Name of the syntax
    :type name: str
    :param handler: Name of the handler for the syntax
    :type handler: str
    :param deny_parameters: If this syntax should prevent parameters from being
    used. If set then any previous syntaxes parameters will not be accepted
    after switching to this syntax.
    :type deny_parameters: bool
    :param deny_qualifiers: If this syntax should prevent qualifiers from being
    used. If set then any previous syntaxes qualifiers will not be accepted
    after switching to this syntax.
    :type deny_qualifiers: bool
    :param parameters: A list of parameters for this syntax
    :type parameters: list
    :param qualifiers: A list of qualifiers for this syntax
    :type qualifiers: list
    :return: The new syntax
    :rtype: dict
    """

    qual = {}
    param = {}

    # TODO: determine minimum unique prefix length and transform qualifier
    # names to use that

    result = {
        K_SYNTAX_NAME: name,
        K_SYNTAX_NO_PARAMETERS: deny_parameters,
        K_SYNTAX_NO_QUALIFIERS: deny_qualifiers,
        }

    if qualifiers is not None:
        for qualifier in qualifiers:
            qual[qualifier[K_QUALIFIER_NAME]] = qualifier
        result[K_SYNTAX_QUALIFIERS] = qual

    if parameters is not None:
        for parameter in parameters:
            param[parameter[K_PARAMETER_POSITION]] = parameter
        result[K_SYNTAX_PARAMETERS] = param

    if handler is not None:
        result[K_SYNTAX_HANDLER]= handler
    return result

def parameter(position, type, required=False, prompt=None, default=None,
              keywords=None, label=None):
    """
    Defines a new parameter
    :param position: The parameter number
    :type position: int
    :param type: The data type. One of int, keyword, float, string or date.
    :type type: str or None
    :param required: If the parameter is required or not
    :type required: bool
    :param prompt: The text to use when prompting the user for the parameter.
    This is only used if the parameter is required and the user does not supply
    it.
    :type prompt: str
    :param default: The parameters default value. Only used for optional
    parameters when the user does not supply a value. Use of the default value
    will not cause syntaxes to be switched.
    :type default: str or int or float or datetime.datetime
    :param keywords: The name of the keywords set. Only used when the type is
    keywords.
    :type keywords: str
    :param label: The user-visible name for the parameter. If this is not
    specified then the label will be 'P' followed by the parameters position.
    :type label: str
    :return: A new parameter
    :rtype: dict
    """

    if type not in ["int", "keyword", "float", "string", "date", None]:
        raise Exception("Invalid type {0}".format(type))

    return {
        K_PARAMETER_POSITION: position,
        K_PARAMETER_TYPE: type,
        K_PARAMETER_PROMPT: prompt,
        K_PARAMETER_REQUIRED: required,
        K_PARAMETER_DEFAULT: default,
        K_PARAMETER_KEYWORDS: keywords,
        K_PARAMETER_LABEL: label,
    }

def qualifier(name, type=None, default_value=None, value_required=True,
              keywords=None, default=False, syntax=None):
    """
    Defines a new qualifier
    :param name: The name of the qualifier
    :type name: str
    :param type: The qualifiers value type if it has one. If it does not take
    a value then None should be used (the default).
    :type type: str or None
    :param default_value: The qualifiers default value. Used if the user
    supplies the qualifier without a value or if the user does not supply the
    qualifier at all. Ignored if the value is required.
    :type default_value: str or int or float or datetime.datetime
    :param value_required: If the value must be specified. This prevents the
    default value from being used
    :type value_required: bool
    :param keywords: The name of the qualifier set to use. Only used by the
    keyword type.
    :type keywords: str
    :param default: If the qualifier is present by default
    :type default: True
    :param syntax: The name of the syntax to switch to when the qualifier is
    supplied
    :type syntax: str
    :return: The new qualifier
    :rtype: dict
    """

    if type not in ["int","keyword","float","string","date", None]:
        raise Exception("Invalid type {0}".format(type))

    # Ignored features: label, negatable, placement, value!list (CDU-26)

    return {
        K_QUALIFIER_NAME: name.upper(),
        K_QUALIFIER_TYPE: type,
        K_QUALIFIER_DEFAULT_VALUE: default_value,
        K_QUALIFIER_VALUE_REQUIRED: value_required,
        K_QUALIFIER_DEFAULT: default,
        K_QUALIFIER_SYNTAX: syntax,
        K_QUALIFIER_KEYWORDS: keywords
    }

def keyword(value, syntax=None):
    """
    Defines a new keyword
    :param value: The keyword name
    :type value: str
    :param syntax: Name of the syntax to switch to
    :type syntax: str
    :return: A new keyword
    :rtype: tuple
    """
    return value.upper(),syntax


def syntax_table(syntaxes):
    """
    Builds a syntax table from the supplied syntaxes
    :param syntaxes: Command syntaxes to build the syntax table from
    :type syntaxes: list
    :return: A table of syntaxes
    :rtype: dict
    """

    stx_table = {}

    for syntax in syntaxes:
        stx_table[syntax[K_SYNTAX_NAME]] = syntax

    return stx_table

def keyword_set(name, keywords):
    """
    Defines a keyword set
    :param name: Name of the keyword set
    :type name: str
    :param keywords: The list of keywords in the set
    :type keywords: list
    :return: A new keyword set
    :rtype: dict
    """
    return {
        K_KEYWORD_SET_NAME: name,
        K_KEYWORD_SET_KEYWORDS: keywords
    }

def keyword_table(keyword_sets):
    """
    Defines a new keyword table
    :param keyword_sets: The list of keyword sets to build the table from
    :type keyword_sets: list
    :return: A new keyword table
    :rtype: dict
    """

    table = {}

    for keyword_set in keyword_sets:
        table[keyword_set[K_KEYWORD_SET_NAME]] = \
            keyword_set[K_KEYWORD_SET_KEYWORDS]

    return table

def verb_table(verbs):
    """
    Builds a new verb table
    :param verbs: List of verbs to build the table from
    :type verbs: list
    :return: A new verb table
    :rtype: dict
    """

    table = {}

    for verb in verbs:
        table[verb[K_VERB_NAME]] = verb

    return table