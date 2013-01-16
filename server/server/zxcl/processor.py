# coding=utf-8
"""
The ZXCL Command Processor and basic shell
"""
from parser import Parser

# TODO: convert all keys to uppercase

__author__ = 'david'

class Syntax(object):
    """
    Handles command syntaxey stuff
    """

    def __init__(self, syntax_table):
        """
        Initialises the syntax thing.
        :param syntax_table: The table of command syntaxes
        :type syntax_table: dict
        """
        self._syntax_table = syntax_table
        self._syntax_name = None
        self._qualifiers = []
        self._parameters = []

    def switch_syntax(self, syntax_name):
        """
        Switches to the specified syntax
        :param syntax_name: The name of the syntax to switch to
        :type syntax_name: str
        """

        if syntax_name not in self._syntax_table:
            raise Exception("Invalid syntax '{name}'".format(name=syntax_name))

        self._syntax_name = syntax_name

        self._syntax = self._syntax_table[self._syntax_name]

        if "parameters" in self._syntax:
            self._parameters = self._syntax["parameters"]

        # Parameters are disallowed by this syntax. Wipe out existing ones.
        elif "no_parameters" in self._syntax and self._syntax["no_parameters"]:
            self._parameters = {}

        if "qualifiers" in self._syntax:
            self._qualifiers = self._syntax["qualifiers"]

        # Parameters are disallowed by this syntax. Wipe out existing ones.
        elif "no_qualifiers" in self._syntax and self._syntax["no_qualifiers"]:
            self._qualifiers = {}


    def parameters_allowed(self):
        """
        :returns: True if parameters are allowed by the syntax, false otherwise.
        """

        # No parameters defined
        if len(self._parameters) == 0:
            return False

        # Parameters are allowed
        return True

    def get_parameter(self, parameter_number, position):
        """
        Returns the specified parameter number
        :param position: Where in the command line the parameter appeared (for
        error reporting)
        :type position: int
        :param parameter_number:  The parameter to fetch
        :returns: The parameter dict
        :rtype: dict
        """

        if parameter_number not in self._parameters:
            raise Exception("Unexpected parameter,"
                            " position {pos}".format(pos=position))

        return self._parameters[parameter_number]

    def qualifiers_allowed(self):
        """
        :returns: True if qualifiers are allowed by the syntax, false otherwise.
        """

        # No qualifiers defined (or a syntax switch wiped them out because
        # they're disabled)
        if len(self._qualifiers) == 0:
            return False

        return True

    def get_qualifier(self, name, position):
        """
        Gets the specified qualifier
        :param name: Name of the qualifier
        :param position: Where in the command line it was (used for error reporting)
        :return: Qualifier
        :rtype: dict
        """

        name = name.upper()

        if name not in self._qualifiers:
            raise Exception("Unexpected qualifier '{name}',"
                            " position {pos}".format(name=name,pos=position))

        return self._qualifiers[name]

    def get_handler(self):
        """
        Gets the name of the handler for this syntax.
        :return: Handler name
        :rtype: str
        """
        if "handler" in self._syntax:
            return self._syntax["handler"]
        else:
            return None

class CommandProcessor(object):
    """
    Processes ZXCL commands. If this command language resembles a certain
    three letter command language then...yeah. Its not 100% compatible but it
    mostly follows the same rules.
    """

    def __init__(self, verb_table, syntax_table, keyword_table):
        """
        Initialises the command processor with the supplied command table.
        :param verb_table: A table of verbs
        :type verb_table: dict
        :param syntax_table: A table of command syntaxes
        :type syntax_table: dict
        :param keyword_table: A table of keyword types
        :type keyword_table: dict
        """

        self._verb_table = verb_table
        self._syntax = Syntax(syntax_table)
        self._keyword_table = keyword_table

        self.parser = Parser()

    def get_verb_syntax(self, verb_name):
        """
        Gets the name of the verbs default syntax. It will handle resolving
        verb synonyms until the real verb is found.
        :param verb_name: The name of the verb
        :type verb_name: str
        :return: The verbs default syntax name
        :type: str
        """

        # TODO: Handle unique prefixes

        verb_name = verb_name.upper()

        try:
            verb = self._verb_table[verb_name]
        except Exception:
            # It will have been a Key Error because the verb name is wrong.
            # Rethrow it with something more user friendly. The verb is always
            # at position 0.
            raise Exception("Unrecognised verb '{0}', position 0".format(verb_name))


        if "syntax" in verb and verb["syntax"] is not None:
            return verb["syntax"]

        # Handle synonyms
        elif "verb" in verb and verb["verb"] is not None:
            return self.get_verb_syntax(verb["verb"])
        else:
            raise Exception("Invalid verb {name}".format(name=verb_name))

    def _get_keyword(self, keyword_set, keyword_name):
        """
        Gets the specified keyword from the specified keyword set.
        :param keyword_set: The keyword set to look for the keyword in
        :type keyword_set: str
        :param keyword_name: Name of the keyword to lookup
        :type keyword_name: str
        :return:
        """

        # TODO: Handle unique prefixes

        keyword_name = keyword_name.upper()

        keywords = self._keyword_table[keyword_set]
        keyword = None
        for kw in keywords:
            if keyword_name == kw[0]:
                keyword = kw

        if keyword is None:
            raise Exception("Invalid keyword {0}".format(keyword_name))

        return keyword

    def process_command(self, command_string, prompt_callback, warning_callback=None):
        """
        Processes a command.
        :param command_string: The command to process.
        :param prompt_callback: Function to call if required parameters or
        qualifiers need to be prompted for.
        :type prompt_callback: callable or None
        :return: processed command
        :rtype: dict
        """

        # This may throw an exception. It is the callers job to handle it.
        command_bits = self.parser.parse(command_string)

        # Nothing to do if there is nothing to do.
        if len(command_bits) == 0:
            return

        # It is a list of 4-item tuples.
        # Item 0: type - either v - verb
        #                    or p - parameter
        #                    or q - qualifier
        # Item 1: position in the command
        # Item 2: if [0]=="v": verb name
        #         if [0]=="p": parameter number
        #         if [0]=="q": qualifier name
        # Item 2: if [0]=="p": parameter value
        #         if [0]=="q": qualifier value. None = no value (bare qualifier)
        #         if [0]=="v": None

        # Item 0 should always be a verb.
        verb = command_bits.pop(0)[2]
        self._syntax.switch_syntax(self.get_verb_syntax(verb))

        qualifiers = {}
        parameters = {}

        while len(command_bits) > 0:
            # Loop over all the command bits.

            component = command_bits.pop(0)
            type = component[0]
            position = component[1]
            name = component[2]
            value = component[3]

            if type == Parser.COMP_TYPE_PARAMETER:
                # Remove the value type
                value_type = value[1]
                value = value[0]


                # Ensure the syntax has and allows parameters
                # TODO: Move this check into get_parameter() ?
                if not self._syntax.parameters_allowed():
                    raise Exception("Parameter '{val}' not allowed here, "
                                    "position {pos}".format(
                        val=value,
                        pos=position
                    ))

                # Ensure the parameter actually exists in the syntax
                parameter = self._syntax.get_parameter(name, position)

                # Check the parameters type
                if value_type != parameter["type"]:
                    raise Exception("Parameter type is {type} but got a "
                                    "{got}".format(
                        type=parameter["type"],
                        got=value_type))

                if value_type == "keyword":
                    # This will throw an exception if its an invalid keyword
                    # which is our validation.
                    keyword = self._get_keyword(parameter["keywords"], value)

                    # Handle syntax switching
                    if keyword[1] is not None:
                        # An alternate syntax!
                        self._syntax.switch_syntax(keyword[1])

                        # Unlike qualifiers, parameters are not wiped out.
                        # Any parameters that overlap between syntaxes should
                        # be identical
                        # TODO: Enforce the above somewhere.

                # Store the parameter
                parameters[name] = value

            elif type == Parser.COMP_TYPE_QUALIFIER:
                # Ensure the syntax has and allows qualifiers
                # TODO: Move this check into get_qualifier() ?
                if not self._syntax.qualifiers_allowed():
                    raise Exception("Qualifier '{name}' not allowed, "
                                    "position {pos}".format(
                        name=name,
                        pos=position))

                qualifier = self._syntax.get_qualifier(name, position)

                # A value of some type was supplied. Is it the right type?
                # lets see...
                if value is not None:
                    # Remove the value type
                    value_type = value[1]
                    value = value[0]


                    qual_type = qualifier["type"]

                    # Check to see if the qualifier actually has a value
                    if qual_type is None:
                        raise Exception("Value not allowed - remove value "
                                        "specification from qualifier {name}, "
                                        "possition {pos}".format(
                            name=name,
                            pos=position))

                    # Check that the supplied value matches the qualifiers.
                    if qual_type != value_type:
                        raise Exception("Qualifier {name} value type is {type} "
                                        "but got a {got}".format(
                            name=name,
                            type=qual_type,
                            got=value_type))

                    if value_type == "keyword":
                        # This will throw an exception if its an invalid keyword
                        # which is our validation. We don't care what it
                        # actually is.
                        self._get_keyword(qualifier["keywords"], value)

                    # Handle syntax switching
                    if "syntax" in qualifier and qualifier["syntax"] is not None:
                        # The qualifier wants us to switch syntaxes.

                        self._syntax.switch_syntax(qualifier["syntax"])

                        if len(qualifiers) > 0 and warning_callback is not None:
                            ignored = ""
                            for key in qualifiers:
                                ignored += ", " + key

                            warning_callback("The following qualifiers will "
                                             "be ignored: {0}".format(
                                ignored[2:]))

                        # Wipe out any previously entered qualifiers. They're
                        # not relevant anymore as we're on a different syntax.
                        qualifiers = {}

                # No value was supplied. Should there have been?
                elif "value_required" in qualifier and qualifier["value_required"]:
                    raise Exception("Value required for qualifier "
                                    "{name}".format(name=name))

                # No qualifier was supplied and its not required. Perhaps there
                # is a default?
                elif "default_value" in qualifier:
                    value = qualifier["default_value"]

                # Store the qualifier
                qualifiers[name] = value
            else:
                raise Exception("Unrecognised command component "
                                "type {0}".format(type))


        # TODO: Process optional parameters that have defaults
        # Something like:
        # parameters += syntax.get_parameter_defaults(from=last_param+1)

        # TODO: Add on all default qualifiers

        # TODO: Process disallows here. This is done with the final syntax in effect.

        handler = self._syntax.get_handler()

        return handler, parameters, qualifiers