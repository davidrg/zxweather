# coding=utf-8
"""
The ZXCL Command Processor and basic shell
"""
from parser import Parser

# These are the command table key constants.
from server.zxcl.command_table import K_PARAMETER_PROMPT, \
    K_QUALIFIER_DEFAULT_VALUE, K_QUALIFIER_TYPE, K_QUALIFIER_VALUE_REQUIRED, \
    K_QUALIFIER_KEYWORDS, K_QUALIFIER_SYNTAX, K_PARAMETER_KEYWORDS, \
    K_PARAMETER_TYPE, K_VERB_VERB, K_VERB_SYNTAX, K_QUALIFIER_DEFAULT, \
    K_PARAMETER_DEFAULT, K_PARAMETER_REQUIRED, K_SYNTAX_HANDLER, \
    K_SYNTAX_PARAMETERS, K_SYNTAX_NO_PARAMETERS, K_SYNTAX_QUALIFIERS, \
    K_SYNTAX_NO_QUALIFIERS, K_PARAMETER_LABEL, K_QUALIFIER_NAME

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
        self.reset()

    def reset(self):
        """
        Resets the syntax class
        """
        self._syntax_name = None
        self._qualifiers = {}
        self._parameters = {}

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

        if K_SYNTAX_PARAMETERS in self._syntax:
            self._parameters = self._syntax[K_SYNTAX_PARAMETERS]

        # Parameters are disallowed by this syntax. Wipe out existing ones.
        elif K_SYNTAX_NO_PARAMETERS in self._syntax and \
             self._syntax[K_SYNTAX_NO_PARAMETERS]:
            self._parameters = {}

        if K_SYNTAX_QUALIFIERS in self._syntax:
            self._qualifiers = self._syntax[K_SYNTAX_QUALIFIERS]


        # Parameters are disallowed by this syntax. Wipe out existing ones.
        elif K_SYNTAX_NO_QUALIFIERS in self._syntax and \
             self._syntax[K_SYNTAX_NO_QUALIFIERS]:
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

    def get_parameter(self, parameter_number, position, value):
        """
        Returns the specified parameter number
        :param parameter_number:  The parameter to fetch
        :param position: Where in the command line the parameter appeared (for
        error reporting). If None then no exceptions will be thrown and None
        will be returned in the case of errors instead. Use this only when
        calling to deal with defaulted parameters, etc.
        :type position: int or None
        :param value: The value encountered in the command string (if any) for
        the parameter
        :returns: The parameter dict
        :rtype: dict
        """ 

        if not self.parameters_allowed():
            if position is None:
                return None
            raise Exception("Parameter '{val}' not allowed here, "
                            "position {pos}".format(
                val=value,
                pos=position
            ))

        if parameter_number not in self._parameters:
            if position is None:
                return None
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

        if not self.qualifiers_allowed():
            raise Exception("Qualifier '{name}' not allowed, "
                            "position {pos}".format(
                name=name,
                pos=position))

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
        if K_SYNTAX_HANDLER in self._syntax:
            return self._syntax[K_SYNTAX_HANDLER]
        else:
            return None

    def get_required_parameter_count(self):
        """
        Returns the number of required parameters in the syntax
        """
        required = 0
        for param in self._parameters:
            if self._parameters[param][K_PARAMETER_REQUIRED]:
                required += 1
        return required

    def get_parameter_defaults(self, first_parameter):
        """
        Gets The defaults for all parameters starting at the specified
        parameter number.
        :param first_parameter: The first parameter to get defaults for
        :type first_parameter: int
        :returns: A dict containing all optional parameters after the first
        parameter and their values.
        :rtype: dict
        """
        param = first_parameter

        if param < 0: param = 0

        param_count = len(self._parameters)

        parameters = {}

        while param < param_count:
            p = self.get_parameter(param, None, None)

            if p is not None and K_PARAMETER_DEFAULT in p and \
               p[K_PARAMETER_DEFAULT] is not None:
                # We have a parameter and it has a default
                parameters[param] = p[K_PARAMETER_DEFAULT]
            else:
                # All parameters will always be present. If the user doesn't
                # supply it and it doesn't have a default it will just be set
                # to None.
                parameters[param] = None

            param += 1

        return parameters

    def get_default_qualifiers(self):
        """
        Gets a list of all qualifiers that should be on by default if the user
        doesn't specify them
        """

        defaults = []

        for qualifier_name in self._qualifiers:
            qualifier = self._qualifiers[qualifier_name]
            if K_QUALIFIER_DEFAULT in qualifier and \
               qualifier[K_QUALIFIER_DEFAULT] is True:
                defaults.append(qualifier)

        return defaults

class CommandProcessor(object):
    """
    Processes ZXCL commands. If this command language resembles a certain
    three letter command language then...yeah. Its not 100% compatible but it
    mostly follows the same rules.
    """

    def __init__(self, verb_table, syntax_table, keyword_table, prompt_callback, warning_callback):
        """
        Initialises the command processor with the supplied command table.
        :param verb_table: A table of verbs
        :type verb_table: dict
        :param syntax_table: A table of command syntaxes
        :type syntax_table: dict
        :param keyword_table: A table of keyword types
        :type keyword_table: dict
        :param prompt_callback: A function to call when required parameters are needed
        :type prompt_callback: callable
        :param warning_callback: A function to handle warning messages
        :type warning_callback: callable
        """

        self._verb_table = verb_table
        self._syntax = Syntax(syntax_table)
        self._keyword_table = keyword_table

        self.parser = Parser()

        self._prompt_callback = prompt_callback
        self._warning_callback = warning_callback

    def _get_verb_syntax(self, verb_name):
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


        if K_VERB_SYNTAX in verb and verb[K_VERB_SYNTAX] is not None:
            return verb[K_VERB_SYNTAX]

        # Handle synonyms
        elif K_VERB_VERB in verb and verb[K_VERB_VERB] is not None:
            return self._get_verb_syntax(verb[K_VERB_VERB])
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

    def _process_parameter(self, name, position, value, value_type):
        # Ensure the parameter actually exists in the syntax. This will
        # also check that the parameter is actually allowed, etc.
        parameter = self._syntax.get_parameter(name, position, value)

        syntax_switched = False

        # Check the parameters type
        if value_type != parameter[K_PARAMETER_TYPE]:
            raise Exception("Parameter type is {type} but got a "
                            "{got}".format(
                type=parameter[K_PARAMETER_TYPE],
                got=value_type))
        if value_type == "keyword":
            # This will throw an exception if its an invalid keyword
            # which is our validation.
            keyword = self._get_keyword(parameter[K_PARAMETER_KEYWORDS], value)

            # Handle syntax switching
            if keyword[1] is not None:
                # An alternate syntax!
                self._syntax.switch_syntax(keyword[1])
                syntax_switched = True

                # Unlike qualifiers, parameters are not wiped out.
                # Any parameters that overlap between syntaxes should
                # be identical
                # TODO: Enforce the above somewhere.

        return syntax_switched

    def _process_qualifier(self, name, position, qualifiers, value):
        # This will also ensure the syntax has and allows qualifiers
        qualifier = self._syntax.get_qualifier(name, position)

        syntax_switched = False

        # Handle syntax switching
        if K_QUALIFIER_SYNTAX in qualifier and \
           qualifier[K_QUALIFIER_SYNTAX] is not None:
            # The qualifier wants us to switch syntaxes.

            self._syntax.switch_syntax(qualifier[K_QUALIFIER_SYNTAX])

            if len(qualifiers) > 0 and self._warning_callback is not None:
                ignored = ""
                for key in qualifiers:
                    # Qualifier names stored for output from the command
                    # processor are stored in lowercase. Convert to uppercase
                    # for display.
                    ignored += ", " + key.upper()

                self._warning_callback("The following qualifiers will "
                                       "be ignored: {0}".format(
                    ignored[2:]))

            # Wipe out any previously entered qualifiers. They're
            # not relevant anymore as we're on a different syntax.
            syntax_switched = True

        # A value of some type was supplied. Is it the right type?
        # lets see...
        if value is not None:
            # Remove the value type
            value_type = value[1]
            value = value[0]

            qual_type = qualifier[K_QUALIFIER_TYPE]

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
                self._get_keyword(qualifier[K_QUALIFIER_KEYWORDS], value)

        # No value was supplied. Should there have been?
        elif qualifier[K_QUALIFIER_TYPE] is not None and \
             K_QUALIFIER_VALUE_REQUIRED in qualifier and \
             qualifier[K_QUALIFIER_VALUE_REQUIRED]:
            raise Exception("Value required for qualifier "
                            "{name}".format(name=name))

        # No qualifier was supplied and its not required. Perhaps there
        # is a default?
        elif K_QUALIFIER_DEFAULT_VALUE in qualifier:
            value = qualifier[K_QUALIFIER_DEFAULT_VALUE]

        return value, syntax_switched

    def process_command(self, command_string):
        """
        Processes a command.
        :param command_string: The command to process.
        :type command_string: str or unicode
        :return: processed command
        :rtype: dict
        """

        self._syntax.reset()

        # This may throw an exception. It is the callers job to handle it.
        command_bits = self.parser.parse(command_string)

        # Nothing to do if there is nothing to do.
        if command_bits is None:
            return None,{},{}

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
        self._syntax.switch_syntax(self._get_verb_syntax(verb))

        qualifiers = {}
        parameters = {}

        # Process any missing required parameters
        parameters.update(self._process_required_parameters(
            CommandProcessor._parameters_in_command(command_bits)))

        # TODO: handle negatable qualifiers & keywords somehow.
        # This is only really useful for:
        #  - Switching off qualifiers that are on by default
        #  - Switching off keyword-type optional parameters with a default value

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

                # Check that its valid, handle syntax switches, etc.
                syntax_switched = self._process_parameter(
                    name, position, value, value_type)

                # Store the parameter
                parameters[name] = value

            else:
                # It must be a qualifier
                value, syntax_switched = self._process_qualifier(
                    name, position, qualifiers, value)

                # A syntax switch may mean qualifiers need to be wiped out
                if syntax_switched:
                    qualifiers = {}

                # Store the qualifier. Qualifier names for output are stored
                # in lowercase.
                qualifiers[name.lower()] = value


            if syntax_switched:
                # Check for any missing required parameters
                # We move parameters from command_bits to parameters as we've
                # processed them. So we need to add together both to get the
                # full parameter count.
                parameters.update(self._process_required_parameters(
                    self._parameters_in_command(command_bits) +
                    len(parameters)))

        # TODO: Grab a list of all explicitly defined parameters and qualifiers
        # This will be used later for processing disallows


        # Fill in any missing optional parameters (with their default values if
        # they have one, None otherwise).

        # We use len(parameters) as the next parameter number as they're
        # numbered from 0 so len(parameters)-1 is the final parameter we
        # processed.
        parameters.update(self._syntax.get_parameter_defaults(len(parameters)))
        # Parameters defaulted like this aren't validated.

        # Turn on default qualifiers that the user hasn't turned on already
        qualifiers.update(self._process_default_qualifiers(qualifiers))

        # TODO: Process disallows here. This is done with the final syntax in effect.

        handler = self._syntax.get_handler()

        return handler, parameters, qualifiers

    @staticmethod
    def _parameters_in_command(command_bits):
        """
        Counts how many parameters are in the supplied command string
        :param command_bits: The parsed command string
        :type command_bits: list
        :return: The number of parameters it contains
        :rtype: int
        """
        parameters = 0
        for bit in command_bits:
            type = bit[0]
            if type == Parser.COMP_TYPE_PARAMETER:
                parameters += 1
        return parameters

    def _process_required_parameters(self, parameter_count):
        """
        Prompts the user (or whoever) for any required parameters that haven't
        been specified.
        :param parameter_count: Number of parameters already entered
        :return: A dict containing parameter values
        """

        parameters = parameter_count

        # Figure out how many are required
        required = self._syntax.get_required_parameter_count()

        entered_parameters = {}

        while parameters < required:
            param_number = parameters
            param = self._syntax.get_parameter(param_number,0,None)

            param_label = 'P' + str(param_number)
            if param[K_PARAMETER_LABEL] is not None:
                param_label = param[K_PARAMETER_LABEL]

            if K_PARAMETER_PROMPT not in param or \
               param[K_PARAMETER_PROMPT] is None:
                raise Exception("Required parameter {0} missing".format(
                    param_label))

            result = self._prompt_callback(param[K_PARAMETER_PROMPT])

            if result is None:
                raise Exception("Parameter '{param}' is required".format(
                    param=param_label))
            else:
                # Parse up the value to get its type.
                value_type = Parser.get_value_type(result, True)

                # And validate it
                syntax_switched = self._process_parameter(
                    param_number, 0, result, value_type)

                # All OK
                entered_parameters[param_number] = result

                if syntax_switched:
                    # We might have new required parameters
                    required = self._syntax.get_required_parameter_count()

            parameters += 1

        return entered_parameters

    def _process_default_qualifiers(self, qualifiers):
        """
        Turns on any default qualifiers that the user hasn't explicitly turned
        on.
        :param qualifiers: The list of qualifiers that are already turned on
        :type qualifiers: dict
        :returns: A dictionary returning new qualifiers that need to be turned
        on by default
        :rtype: dict
        """

        defaults = self._syntax.get_default_qualifiers()

        new_qualifiers = {}

        for qualifier in defaults:
            # Qualifier names for output are stored in lowercase.
            qualifier_name = qualifier[K_QUALIFIER_NAME].lower()
            if qualifier_name not in qualifiers:
                # Qualifier should be on by default. The user hasn't turned it
                # on so we will.
                value, syntax_switched = self._process_qualifier(
                    qualifier_name, None, qualifiers, None)

                # Syntax switching is ignored for defaulted parameters.

                new_qualifiers[qualifier_name] = value


        return new_qualifiers