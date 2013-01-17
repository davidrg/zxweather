# coding=utf-8
"""
Functions for building command tables
"""
__author__ = 'david'

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
        "name": name.upper(),
        "syntax": syntax
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
        "name": name.upper(),
        "verb": verb.upper()
    }

def syntax(name, handler = "", deny_parameters=False, deny_qualifiers=False,
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

    if qualifiers is not None:
        for qualifier in qualifiers:
            qual[qualifier["name"]] = qualifier

    if parameters is not None:
        for parameter in parameters:
            param[parameter["position"]] = parameter

    return {
        "name": name,
        "handler": handler,
        "no_parameters": deny_parameters,
        "no_qualifiers": deny_qualifiers,
        "parameters": param,
        "qualifiers": qual
    }

def parameter(position, type, required=False, prompt=None, default=None,
              keywords=None):
    """
    Defines a new parameter
    :param position: The parameter number
    :type position: int
    :param type: The data type. One of int, keyword, float, string or date.
    :type type: str
    :param required: If the parameter is required or not
    :type required: bool
    :param prompt: The text to use when prompting the user for the parameter.
    This is only used if the parameter is required and the user does not supply
    it.
    :type prompt: str
    :param default: The parameters default value. Only used for optional
    parameters when the user does not supply a value. Use of the default value
    will not cause syntaxes to be switched.
    :type default: str
    :param keywords: The name of the keywords set. Only used when the type is
    keywords.
    :type keywords: str
    :return: A new parameter
    :rtype: dict
    """

    if type not in ["int","keyword","float","string","date"]:
        raise Exception("Invalid type {0}".format(type))

    return {
        "position": position,
        "type": type,
        "prompt": prompt,
        "required": required,
        "default": default,
        "keywords": keywords
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
    :type default_value: str
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
        "name": name.upper(),
        "type": type,
        "default_value": default_value,
        "value_required": value_required,
        "default": default,
        "syntax": syntax,
        "keywords": keywords
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
        stx_table[syntax["name"]] = syntax

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
        "name": name,
        "keywords": keywords
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
        table[keyword_set["name"]] = keyword_set["keywords"]

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
        table[verb["name"]] = verb

    return table