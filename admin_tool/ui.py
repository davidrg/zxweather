# coding=utf-8
"""
User interface routines
"""

__author__ = 'David Goodwin'

from typing import List, Dict, TypedDict, Callable, Optional

MenuOption = TypedDict('MenuOption', {'key': str, 'name': str, 'type': str, 'func': Callable})

def menu(choices: List[MenuOption], prompt: str="Select option") -> str:
    """
    Runs a menu getting choices from the user.
    :param choices: A dict describing the menu options.
    :param prompt: Prompt to display to the user
    """

    while True:
        print("{0}:".format(prompt))

        options_by_key = {}

        for choice in choices:
            print("\t{0}. {1}".format(choice['key'], choice['name']))
            options_by_key[choice['key']] = choice

        result = input("Choice> ")

        if result in options_by_key.keys():
            if options_by_key[result]['type'] == 'return':
                return result
            elif options_by_key[result]['type'] == 'func' and \
                    'func' in options_by_key[result].keys():
                func = options_by_key[result]['func']
                func()
        else:
            print("Invalid option")


def sub_menu(choices: List[MenuOption], prompt: str="Select option:") -> str:
    """
    Runs a sub-menu which only performs a single action before returning.
    :param choices: A dict describing the menu options.
    :param prompt: The user prompt
    """

    while True:
        print(prompt)

        options_by_key = {}

        for choice in choices:
            print("\t{0}. {1}".format(choice['key'], choice['name']))
            options_by_key[choice['key']] = choice

        result = input("Choice> ")

        if result in options_by_key.keys():
            if options_by_key[result]['type'] == 'return':
                return result
            elif options_by_key[result]['type'] == 'func' and\
                    'func' in options_by_key[result].keys():
                func = options_by_key[result]['func']
                func()

                return result
        else:
            print("Invalid option")


def __get_string_required(prompt: str) -> str:
    """
    Gets a string from the user
    """

    while True:
        value = input("{0}: ".format(prompt))
        if value != "":
            return value
        else:
            print("You must enter a value.")


def __get_string_optional(prompt: str, default: Optional[str]) -> Optional[str]:
    value = input("{0} [{1}]: ".format(prompt, default))
    if value == '':
        value = default
    return value


def get_string(prompt: str, default: Optional[str]='', required: bool=False,
               max_len: Optional[int]=None, min_len: Optional[int]=None) -> Optional[str]:
    """
    Gets a string from the user. If a default value is supplied the user may
    just hit return to accept it. If required is true then the default value
    is ignored.
    :param prompt: User prompt
    :param default: Default value (if required is False)
    :param required: If the user is required to enter a value. Disables default value.
    :param max_len: Maximum string length
    :param min_len: Minimum string length
    :returns: Value entered by user
    """

    while True:
        if required:
            val = __get_string_required(prompt)
        else:
            val = __get_string_optional(prompt, default)

        if max_len is not None and len(val) > max_len:
            print("Response can not be longer than {0} characters".format(max_len))
        elif min_len is not None and len(val) < min_len:
            print("Response must be at least {0} characters".format(min_len))
        else:
            return val


def _get_boolean_required(prompt: str) -> bool:
    """
    Forces the user to choose yes or no. No defaults.
    """

    while True:
        value = input("{0}: ".format(prompt)).lower()
        if value in ['y', 'yes']:
            return True
        elif value in ['n', 'no']:
            return False
        else:
            print("Enter yes or no.")


def _get_boolean_optional(default: bool, prompt: str) -> bool:
    """
    Gets an optional boolean value from the user. A default value can
    be supplied.
    """
    if type(default) is bool:
        if default:
            disp_default = 'yes'
        else:
            disp_default = 'no'
    else:
        disp_default = ''
    value = input("{0} (yes/no) [{1}]: ".format(prompt, disp_default))
    if value == '':
        return default
    elif value in ['y', 'yes']:
        return True
    elif value in ['n', 'no']:
        return False


def get_boolean(prompt: str, default: Optional[bool]=None, required: bool=False) -> Optional[bool]:
    """
    Gets a yes/no response from the user. If required is true then the default
    value is ignored and the user is forced to enter a value.
    :param prompt: User prompt
    :param default: Default value (only used if required = False)
    :param required: If a response is required or not. Overrides the default value
    :returns: value chosen by the user
    """

    if required:
        return _get_boolean_required(prompt)
    else:
        return _get_boolean_optional(default, prompt)


def __get_number_required(prompt: str) -> int:
    """
    Gets a required number
    :param prompt: The prompt for the user
    :return: An integer.
    """

    response = None

    while response is None:
        response = get_string(prompt, None, True)

        if response == '':
            print("Please enter a number.")
        else:
            try:
                return int(response)
            except ValueError:
                print("Please enter a number.")
                response = None


def __get_number_optional(prompt: str, default: Optional[int]) -> Optional[int]:
    """
    Gets an optional number
    :param prompt: The prompt for the user
    :param default: The default value for if the user hits enter without
    typing anything.
    :return: An integer.
    """

    response = None

    if default is None:
        disp_default = ''
    else:
        disp_default = str(default)

    while response is None:
        response = get_string(prompt, disp_default)

        if response == '':
            return default

        try:
            return int(response)
        except ValueError:
            print("Please enter a number.")
            response = None


def get_number(prompt: str, default: Optional[int]=None, required:bool=False) -> Optional[int]:
    """
    Gets a number from the user. If there is no default and the user does
    not enter anything None is returned.
    :param prompt: User prompt
    :param default: Default value if not required
    :param required: If it is required or not
    """

    if required:
        return __get_number_required(prompt)
    else:
        return __get_number_optional(prompt, default)


def __get_float_required(prompt: str) -> float:
    """
    Gets a required float
    :param prompt: The prompt for the user
    :return: An float.
    """

    response = None

    while response is None:
        response = get_string(prompt, None, True)

        if response == '':
            print("Please enter a number.")
        else:
            try:
                return float(response)
            except ValueError:
                print("Please enter a number.")
                response = None


def __get_float_optional(prompt: str, default: Optional[float]) -> Optional[float]:
    """
    Gets an optional float
    :param prompt: The prompt for the user
    :param default: The default value for if the user hits enter without
    typing anything.
    :return: An float.
    """

    response = None

    if default is None:
        disp_default = ''
    else:
        disp_default = str(default)

    while response is None:
        response = get_string(prompt, disp_default)

        if response == '':
            return default

        try:
            return float(response)
        except ValueError:
            print("Please enter a number.")
            response = None


def get_float(prompt: str, default: Optional[float]=None, required: bool=False) -> Optional[float]:
    """
    Gets a float from the user. If there is no default and the user does
    not enter anything None is returned.
    :param prompt: User prompt
    :param default: Default value if not required
    :param required: If it is required or not
    """

    if required:
        return __get_float_required(prompt)
    else:
        return __get_float_optional(prompt, default)


def __get_code_required(prompt: str, valid_codes: List[str]) -> str:

    valid_codes = [x.upper() for x in valid_codes]

    while True:
        val = get_string(prompt, required=True)

        val = val.upper()

        if val in valid_codes:
            return val
        else:
            print("Please enter a valid code.")


def __get_code_optional(prompt: str, valid_codes: List[str], default: Optional[str]) -> Optional[str]:
    valid_codes = [x.upper() for x in valid_codes]

    while True:
        val = get_string(prompt, default)

        if val is None:
            return None
        elif val.upper() in valid_codes:
            return val.upper()
        else:
            print("Please enter a valid code.")


def get_code(prompt: str, valid_codes: List[str], default: Optional[str]=None, required: bool=False) -> Optional[str]:
    """
    Asks the user to enter a valid code.
    :param prompt: User prompt
    :param valid_codes: A list of all valid codes the user is allowed to enter.
                        Used only for validation (these are not displayed).
                        Values must be in uppercase.
    :param default: Default code
    :param required: If a response is required from the user.
    :return: The code chosen by the user
    """

    if required:
        return __get_code_required(prompt, valid_codes)
    else:
        return __get_code_optional(prompt, valid_codes, default)


def pause():
    """
    Waits for the user to hit return.
    """

    input("Hit return to continue")


def get_string_with_length_indicator(prompt: str, default: Optional[str],
                                     max_len: int, required: bool=False) -> Optional[str]:
    """
    Gets a string showing an indicator of how long it is allowed to be
    :param prompt: User prompt
    :param default: Default value
    :param max_len: Maximum string length
    :param required: If the value is required or not
    :return: String entered by the user
    """

    if not required:
        padding = len(prompt) + 5
        # +5 for " []: "

        if default is not None:
            padding += len(default)
        else:
            padding += 4  # "None"

    else:
        padding = len(prompt) + 2
        # +2 for ": "

    dash_length = max_len - 2
    print((' ' * padding) + '|' + ('-' * dash_length) + '|')
    return get_string(prompt, default, max_len=max_len, required=required)
