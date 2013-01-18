# coding=utf-8
"""
Tests the ZXCL Command Processor
"""
import unittest
from server.zxcl.command_table import syntax_table, qualifier, parameter, \
    verb_table, verb, synonym, syntax, keyword_table, keyword, keyword_set, \
    K_VERB_NAME, K_VERB_SYNTAX
from server.zxcl.processor import CommandProcessor

__author__ = 'david'

class FunctionCallLogger(object):
    """
    Utility class for logging if functions are called.
    """
    def __init__(self, function):
        self.function = function
        self.was_called = False

    def __call__(self, value=None):
        self.was_called = True
        self.function_parameter = value
        return self.function(value)

class CommandProcessorTests(unittest.TestCase):
    """
    Tests for the ZXCL Command Processor
    """
    def setUp(self):
        """
        Sets up command tables and a command processor to be used by the
        tests.
        """
        self.verb_tbl = verb_table([
            verb(name="set", syntax="set_syntax"),
            verb(name="show", syntax="show_syntax"),
            synonym(name="show_synonym", verb="show"),
            verb(name="starttls", syntax="starttls_syntax"),
            verb(name="login", syntax="login_syntax"),
#            verb(name="stream", syntax="stream_syntax"),
#            synonym(name="subscribe", verb="stream"),
            verb(name="logout", syntax="logout_syntax"),
            verb(name="quit", syntax="quit_syntax"),
            {K_VERB_NAME:"INVALID_VERB", K_VERB_SYNTAX:None},
        ])

        self.show_param_0 = parameter(
            position=0,
            type="keyword",
            prompt="What?",
            required=True,
            keywords="show_keywords"
        )

        self.set_param_0 = parameter(
            position=0,
            type="keyword",
            prompt="What?",
            required=True,
            keywords="set_keywords"
        )

        self.syntax_tbl = syntax_table([
            syntax(name="show_syntax", parameters=[self.show_param_0]),
            syntax(name="set_syntax", parameters=[self.set_param_0]),
            syntax(
                name="starttls_syntax",
                parameters=[
                    parameter(
                        position=0,
                        type="int",
                        default=42
                    ),
                    parameter(
                        position=1,
                        type="float",
                        default=12.0
                    ),
                    parameter(
                        position=2,
                        type="int"
                    )
                ],
                qualifiers=[qualifier(
                    name="enable",
                    default_value="true",
                    default=True
                )],
                handler="starttls_handler"
            ),
            syntax(
                name="login_syntax",
                parameters=[parameter(position=0, type="string", required=True,
                    label="Username"),
                            parameter(position=1, type="int", default=12345)],
                handler="login_handler",
                qualifiers=[
                    qualifier(
                        name="password",
                        type="string",
                        value_required=True
                    ),
                    qualifier(
                        name="default_value",
                        type="int",
                        default_value=42,
                        value_required=False
                    ),
                    qualifier(
                        name="keyword_qual",
                        type="keyword",
                        keywords="qualifier_keywords"
                    ),
                    qualifier(
                        name="no_value"
                    )
                ]
            ),
#            syntax(
#                name="stream_syntax",
#                deny_parameters=True,
#                handler="subscribe_handler",
#                qualifiers=[
#                    qualifier(name="up", syntax="stream_up_syntax"),
#                    qualifier(name="station", type="string", value_required=True),
#                    qualifier(name="samples"),
#                    qualifier(name="live"),
#                    qualifier(name="catchup"),
#                    qualifier(name="date", type="date"),
#                    qualifier(name="time", type="string")
#                ]
#            ),
#            syntax(
#                name="stream_up_syntax",
#                deny_parameters=True,
#                handler="stream_up_handler"
#            ),
            syntax(
                name="logout_syntax",
                deny_parameters=True,
                qualifiers=[qualifier(name="message",syntax="logout_message_syntax"),
                            qualifier(name="orig_syntax_qual")],
#                handler="logout_handler"
            ),
            syntax(
                name="logout_message_syntax",
                parameters=[parameter(position=0,type="string",required=True, prompt="Foo")],
                qualifiers=[qualifier(name="message",syntax="logout_message_syntax")],
                #                handler="logout_handler"
            ),
            syntax(
                name="quit_syntax",
                deny_parameters=True,
                deny_qualifiers=True,
                handler="quit_handler"
            ),
            syntax(
                name="show_compatibility",
                parameters=[self.show_param_0],
                # so that the invalid qualifier doesn't die due to qualifiers
                # being disallowed:
                qualifiers=[qualifier(name="qual1")],
                handler="show_compatibility_handler"
            ),
            syntax(
                name="set_client",
#                handler="set_client_handler",
                parameters=[
                    self.set_param_0,
                    parameter(
                        position=1,
                        type="string",
                        required=True,
                        prompt="Name:",
                        label="Client Name"
                    )
                ],
#                qualifiers=[
#                    qualifier(
#                        name="version",
#                        type="string",
#                        value_required=True
#                    )
#                ]
            ),
#            syntax(
#                name="set_interface",
#                handler="set_interface_handler",
#                parameters=[self.set_param_0],
#                qualifiers=[qualifier(name="coded")]
#            )
        ])

        self.keyword_tbl = keyword_table([
            keyword_set("show_keywords", [
                keyword(value="compatibility", syntax="show_compatibility"),
            ]),
            keyword_set("set_keywords", [
                keyword(value="client", syntax="set_client"),
#                keyword(value="interface", syntax="set_interface"),
            ]),
            keyword_set("qualifier_keywords", [
                keyword(value="keyword_1"),
                keyword(value="keyword_2")
            ]),
        ])

        self.processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, None, None)

    def test_empty_command_does_nothing(self):
        handler, parameters, qualifiers = self.processor.process_command("!foo")

        self.assertIsNone(handler)
        self.assertDictEqual(parameters, {})
        self.assertDictEqual(qualifiers, {})

    def test_missing_required_parameter_causes_prompt_callback(self):
        """
        Tests that the prompt callback is called when a required parameter is
        missing and that the callbacks parameter is the parameters prompt
        string.
        """
        def prompt_callback(prompt):
            """Called when the required parameter is missing
            :param prompt: the prompt text
            """
            return "compatibility"

        callback = FunctionCallLogger(prompt_callback)

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, callback, None)

        processor.process_command("show")

        self.assertTrue(callback.was_called)
        self.assertEqual(callback.function_parameter, "What?")

    def test_missing_required_parameter_causes_exception_when_no_callback(self):
        """
        If no prompt callback is present then an exception should be thrown
        when required parameters are missing.
        """
        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, None, None)

        with self.assertRaises(Exception):
            processor.process_command("show")

    def test_missing_required_parameter_with_no_prompt_causes_exception(self):
        """
        If a required parameter is missing and it doesn't define any prompt
        string then an exception should be thrown.
        """

        def prompt_callback(prompt):
            """Called when the required parameter is missing
            :param prompt: the prompt text
            """
            return "\"username\""

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, prompt_callback, None)

        with self.assertRaises(Exception):
            processor.process_command("login")

    def test_missing_required_parameter_throws_exception_if_callback_returns_None(self):
        def prompt_callback(prompt):
            """Called when the required parameter is missing
            :param prompt: the prompt text
            """
            return None

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, prompt_callback, None)

        with self.assertRaises(Exception):
            processor.process_command("show")

    def test_missing_required_parameters_from_syntax_switch_are_prompted(self):
        """
        Required parameters that appear after a syntax switch should be treated
        just the same as the original parameters if they're missing.
        """
        def prompt_callback(prompt):
            """Called when the required parameter is missing
            :param prompt: the prompt text
            """
            return "\"some client name\""

        callback = FunctionCallLogger(prompt_callback)

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, callback, None)

        processor.process_command("set client")

        self.assertTrue(callback.was_called)
        self.assertEqual(callback.function_parameter, "Name:")

    def test_parameter_of_wrong_type_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("show 42")

    def test_parameter_with_incorrect_keyword_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("show a_keyword_that_doesnt_exist")

    def test_unrecognised_verb_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("verb_that_doesnt_exist foo")

    def test_synonym_works_like_real_verb(self):
        handler,parameters,qualifiers = self.processor.process_command(
            "show_synonym compatibility")

        self.assertEqual(handler, "show_compatibility_handler")

    def test_invalid_verb_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("invalid_verb foo")

    def test_invalid_parameter_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("login \"username\" invalid_parameter")

    def test_invalid_qualifier_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("show compatibility/bad_qualifier")

    def test_qualifier_missing_required_value_value_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("login \"username\"/password")

    def test_qualifier_default_value_gets_used_when_no_value_is_given(self):
        handler, parameters, qualifiers = self.processor.process_command(
            "login \"username\"/default_value")

        self.assertEqual(qualifiers["default_value"], 42)

    def test_qualifier_with_bad_keyword_value_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("login \"p0\"/keyword_qual=abacus")

    def test_qualifier_value_of_wrong_type_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("login \"p0\"/keyword_qual=12.3")

    def test_qualifier_without_value_throws_exception_if_value_given(self):
        with self.assertRaises(Exception):
            self.processor.process_command("login \"p0\"/no_value=12.3")

    def test_qualifier_initiated_syntax_switch(self):
        """
        This tests that a qualifier can perform a syntax switch. It is tested
        by checking that a required parameter in the new syntax causes the
        prompt callback to be called because it was not originally specified.
        :return:
        """
        def prompt_callback(prompt):
            """Called when the required parameter is missing
            :param prompt: the prompt text
            """
            return "\"some client name\""

        callback = FunctionCallLogger(prompt_callback)

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, callback, None)

        processor.process_command("logout/message")

        self.assertTrue(callback.was_called)
        self.assertEqual(callback.function_parameter, "Foo")

    def test_qualifiers_before_syntax_switch_trigger_warning_callback(self):
        def test_callback(message):
            """Called when a warning is issued """
            return None

        callback = FunctionCallLogger(test_callback)

        processor = CommandProcessor(
            self.verb_tbl, self.syntax_tbl, self.keyword_tbl, None, callback)

        processor.process_command("logout/orig_syntax_qual/message \"message\"")

        self.assertTrue(callback.was_called)
        self.assertTrue("orig_syntax_qual".upper() in
                        callback.function_parameter)

    def test_default_qualifiers_appear_without_being_specified(self):
        handler, parameters, qualifiers = self.processor.process_command(
            "starttls")

        self.assertEqual(qualifiers["enable"], "true")

    def test_default_parameters_appear_without_being_specified(self):
        handler, parameters, qualifiers = self.processor.process_command(
            "starttls")

        self.assertEqual(parameters[0], 42)
        self.assertEqual(parameters[1], 12.0)

    def test_unspecified_optional_non_default_parameters_appear_as_none(self):
        handler, parameters, qualifiers = self.processor.process_command(
            "starttls")

        self.assertIsNone(parameters[2])

    def test_qualifier_when_qualifiers_disallowed_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("quit/qualifier_12")

    def test_parameter_when_parameters_disallowed_throws_exception(self):
        with self.assertRaises(Exception):
            self.processor.process_command("quit abacus")

    def test_results(self):
        handler, parameters, qualifiers = self.processor.process_command(
            "login /password=\"pass\" /keyword_qual=keyword_1 \"user\"/no_value/default_value")

        self.assertEqual(handler, "login_handler")

        self.assertEqual(len(parameters), 2)
        self.assertEqual(parameters[0], "user")
        self.assertEqual(parameters[1], 12345)

        self.assertEqual(len(qualifiers), 4)
        self.assertEqual(qualifiers["password"], "pass")
        self.assertEqual(qualifiers["keyword_qual"], "keyword_1")
        self.assertEqual(qualifiers["default_value"], 42)
        self.assertIsNone(qualifiers["no_value"])
