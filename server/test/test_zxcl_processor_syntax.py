# coding=utf-8
"""
Unit tests for the ZXCL Command Processors Syntax class
"""
from server.zxcl.command_table import syntax_table, syntax as CreateSyntax, qualifier, parameter, K_SYNTAX_PARAMETERS, K_SYNTAX_QUALIFIERS
from server.zxcl.processor import Syntax
import unittest

__author__ = 'david'

class SecondSyntaxSwitchTests(unittest.TestCase):
    """
    Tests that the syntax switching behaviour works correctly by performing two
    syntax switches and making sure the second syntax switch has correctly
    overriden the initial details put in place by the initial syntax switch.
    """

    def test_replaces_syntax_name(self):
        """
        Checks that the syntax switch replaces the previous syntaxes name
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")],
                qualifiers=[qualifier(name="qual2")]
            )
        ]))
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertEqual(stx._syntax_name, "bar")

    def test_replaces_handler(self):
        """
        The new syntax should override the previous syntaxes handler
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")],
                qualifiers=[qualifier(name="qual2")]
            )
        ]))
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertEqual(stx.get_handler(), "bar_handler")

    def test_replaces_qualifiers_if_second_has_qualifiers(self):
        """
        If the new syntax has qualifiers its qualifier list should completely
        replace that of the previous syntax
        """

        stx_table = syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")],
                qualifiers=[qualifier(name="qual2")]
            )
        ])
        second_qual_list = stx_table["bar"][K_SYNTAX_QUALIFIERS]

        stx = Syntax(stx_table)
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._qualifiers, second_qual_list)

    def test_replaces_parameters_if_second_has_parameters(self):
        """
        If the new syntax has parameters its parameter list should completely
        replace that of the previous syntax.
        """
        stx_table = syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")],
                qualifiers=[qualifier(name="qual1")]
            )
        ])
        stx = Syntax(stx_table)

        second_param_list = stx_table["bar"][K_SYNTAX_PARAMETERS]

        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._parameters, second_param_list)

    def test_retains_qualifiers_if_second_has_no_qualifiers(self):
        """
        If the second syntax does not define any qualifiers it should leave
        the previous syntaxes qualifiers in effect
        """

        stx_table = syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")]
            )
        ])
        first_qual_list = stx_table["foo"][K_SYNTAX_QUALIFIERS]

        stx = Syntax(stx_table)
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._qualifiers, first_qual_list)

    def test_retains_parameter_list_if_second_has_no_parameters(self):
        """
        If the second syntax does not define any parameters then it should
        leave the previous syntaxes parameters in effect.
        """

        stx_table = syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                qualifiers=[qualifier(name="qual2")]
            )
        ])
        first_param_list = stx_table["foo"][K_SYNTAX_PARAMETERS]

        stx = Syntax(stx_table)
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._parameters, first_param_list)

    def test_clears_qualifiers_if_second_disallows_qualifiers(self):
        """
        If the second syntax disallows qualifiers than the first syntaxes
        qualifiers should be wiped out (leaving no qualifiers in effect)
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                parameters=[parameter(1,type="int"), parameter(2,"string")],
                deny_qualifiers=True
            )
        ]))
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._qualifiers, {})

    def test_clears_parameter_list_if_second_disallows_parameters(self):
        """
        If the second syntax disallows parameters then the first syntaxes
        parameters should be wiped out (leaving no parameters in effect)
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="foo",
                handler="foo_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            ),
            CreateSyntax(
                name="bar",
                handler="bar_handler",
                deny_parameters=True,
                qualifiers=[qualifier(name="qual2")]
            )
        ]))
        stx.switch_syntax("foo")
        stx.switch_syntax("bar")

        self.assertDictEqual(stx._parameters, {})


class BaseSyntaxTests(unittest.TestCase):
    """
    Checks the most basic syntax functionality - mostly the various get methods
    when only one initial syntax switch has been performed.
    """

    def test_throws_an_exception_for_bad_syntax_name(self):
        """
        Tests switch_syntax throws an exception if a bad syntax name is
        supplied.
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            )
        ]))

        with self.assertRaises(Exception):
            stx.switch_syntax("foo")

    def test_new_syntax_object_has_no_name_until_syntax_switch(self):
        """
        A freshly created Syntax object must be syntax-switched before it has
        a syntax name
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            )
        ]))

        self.assertIsNone(stx._syntax_name)

    def test_new_syntax_object_has_no_parameters_until_syntax_switch(self):
        """
        A freshly created Syntax object must be syntax-switched before it has
        any parameters
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            )
        ]))

        self.assertDictEqual(stx._parameters, {})

    def test_new_syntax_object_has_no_qualifiers_until_syntax_switch(self):
        """
        A freshly created Syntax object must be syntax-switched before it has
        any qualifiers
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            )
        ]))

        self.assertDictEqual(stx._qualifiers, {})

    def test_no_parameters_allowed_without_parameters(self):
        """
        If there are no parameters defined by the syntax then it should report
        that no parameters are allowed
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertFalse(stx.parameters_allowed())

    def test_parameters_allowed_with_parameters(self):
        """
        If there are parameters defined by the syntax then it should report
        that parameters are allowed
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(1,type="int")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertTrue(stx.parameters_allowed())

    def test_no_qualifiers_allowed_without_qualifiers(self):
        """
        If there are no qualifiers defined by the syntax then it should report
        that no qualifiers are allowed
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertFalse(stx.qualifiers_allowed())

    def test_qualifiers_allowed_with_qualifiers(self):
        """
        If there are qualifiers defined by the syntax then it should report
        that qualifiers are allowed
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                qualifiers=[qualifier(name="qual1")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertTrue(stx.qualifiers_allowed())

    def test_get_handler_returns_none_for_no_handler(self):
        """
        If no handler is specified then get_handler() should return None
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertIsNone(stx.get_handler())

    def test_get_handler_returns_handler(self):
        """
        If there are qualifiers defined by the syntax then it should report
        that qualifiers are allowed
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertEqual(stx.get_handler(), "syntax_1_handler")

    def test_reset_resets_syntax(self):
        """
        Tests that the reset method clears the syntax name, parameters, etc.
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                handler="syntax_1_handler",
                parameters=[parameter(1,type="int")],
                qualifiers=[qualifier(name="qual1")]
            )
        ]))
        stx.switch_syntax("syntax_1")
        stx.reset()

        self.assertDictEqual(stx._qualifiers, {})
        self.assertDictEqual(stx._parameters, {})
        self.assertIsNone(stx._syntax_name)

class GetRequiredParameterCount(unittest.TestCase):
    """
    Tests Syntax.get_required_parameter_count()
    """
    def test_required_parameter_count_is_zero_for_no_parameters(self):
        """
        Tests that get_required_parameter_count() functions correctly when
        no parameters have been defined.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertEqual(stx.get_required_parameter_count(), 0)

    def test_required_parameter_count_is_zero_for_no_required_parameters(self):
        """
        Tests that get_required_parameter_count() functions correctly when
        no required parameters have been defined.
        :return:
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(1,type="int")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertEqual(stx.get_required_parameter_count(), 0)

    def test_required_parameter_count_is_one_for_one_required_parameters(self):
        """
        Tests that get_required_parameter_count() functions correctly when
        one required parameters has been defined.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(1,type="int",required=True)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertEqual(stx.get_required_parameter_count(), 1)

    def test_required_parameter_count_is_two_for_two_required_parameters(self):
        """
        Tests that get_required_parameter_count() functions correctly when
        two required parameters have been defined.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(1,type="int",required=True),
                            parameter(2,type="int",required=True)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertEqual(stx.get_required_parameter_count(), 2)

class GetDefaultQualifiers(unittest.TestCase):
    """
    Tests Syntax.get_default_qualifiers()
    """
    def test_no_default_qualifiers_when_no_qualifiers_defined(self):
        """
        Checks that there are no default qualifiers when there are no
        qualifiers defined.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertListEqual(stx.get_default_qualifiers(), [])

    def test_no_default_qualifiers_when_no_default_qualifiers_defined(self):
        """
        Checks that there are no default qualifiers when there are no
        qualifiers defined.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                qualifiers=[qualifier(name="foo")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertListEqual(stx.get_default_qualifiers(), [])

    def test_one_default_qualifier_when_one_default_qualifier_defined(self):
        """
        Checks that there are only one qualifier is returned if only one
        default has been defined
        """
        default_qual_1 = qualifier(name="foo",default=True)
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                qualifiers=[default_qual_1,]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertListEqual(stx.get_default_qualifiers(), [default_qual_1,])

    def test_two_default_qualifiers_when_two_default_qualifiers_defined(self):
        """
        Checks that there are only one qualifier is returned if only one
        default has been defined
        """
        default_qual_1 = qualifier(name="foo",default=True)
        default_qual_2 = qualifier(name="bar",default=True)
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                qualifiers=[default_qual_1, default_qual_2]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertListEqual(stx.get_default_qualifiers(), [default_qual_1,
                                                            default_qual_2])

class GetQualifier(unittest.TestCase):
    """
    Tests Syntax.get_qualifier()
    """
    def test_throws_exception_when_qualifiers_not_allowed(self):
        """
        If we request a qualifier when they're not allowed we should get an
        exception.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
            )
        ]))
        stx.switch_syntax("syntax_1")

        with self.assertRaises(Exception):
            stx.get_qualifier("abacus", 0)

    def test_throws_exception_for_nonexistant_qualifier(self):
        """
        If we request a qualifier that doesn't exist we should get an
        exception.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                qualifiers=[qualifier(name="foo",default=True), qualifier(name="bar",default=True)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        with self.assertRaises(Exception):
            stx.get_qualifier("abacus", 0)

    def test_returns_requested_qualifier(self):
        """
        If we request a qualifier that does exist then it should be returned
        """
        qual = qualifier(name="foo",default=True)

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                qualifiers=[qual, qualifier(name="bar",default=True)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_qualifier("foo",0), qual)

class SyntaxGetParameterTests(unittest.TestCase):
    """
    Handles testing Syntax.get_parameter()
    """
    def test_throws_exception_when_parameters_not_allowed(self):
        """
        If we request a parameter when they're not allowed we should get an
        exception.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
            )
        ]))
        stx.switch_syntax("syntax_1")

        with self.assertRaises(Exception):
            stx.get_parameter(0, 0,"")

    def test_returns_None_when_parameters_not_allowed_if_position_is_None(self):
        """
        If we request a parameter without supplying a position then None should
        be returned instead of having an exception thrown.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertIsNone(stx.get_parameter(0, None,""))

    def test_throws_exception_for_nonexistant_parameter(self):
        """
        If we request a parameter that doesn't exist we should get an
        exception.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,"int"),parameter(1,"string"),
                            parameter(2,"float")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        with self.assertRaises(Exception):
            stx.get_parameter(3, 0, "")

    def test_returns_None_for_nonexistant_parameter_if_position_is_None(self):
        """
        If we request a parameter that doesn't exist and we supply None for
        the command-line position we should get None returned instead of an
        exception.
        """

        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,"int"),parameter(1,"string"),
                            parameter(2,"float")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertIsNone(stx.get_parameter(3, None, ""))

    def test_returns_requested_qualifier(self):
        """
        If we request a parameter we should get it.
        """
        param_0 = parameter(position=0,type="int")
        param_1 = parameter(position=1,type="string")
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[param_0, param_1]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter(0,0,""), param_0)

class SyntaxGetParameterDefaults(unittest.TestCase):
    """
    Tests for Syntax.get_parameter_defaults(first_parameter)
    """
    def test_get_parameter_defaults_returns_empty_dict_for_no_parameters(self):
        """
        If no parameters are defined then get_parameter_defaults() should just
        return an empty dict.
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1"
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(0),{})

    def test_get_parameter_defaults_returns_all_parameters_if_first_is_negative(self):
        """
        If the first parameter number is negative then get_parameter_defaults()
        should return all parameters with defaults
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int",default=42),
                            parameter(1,type="string",default="42"),
                            parameter(2,type="float",default=42.0)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(-1),{0:42,1:"42",2:42.0})

    def test_get_parameter_defaults_returns_from_first_param_for_first_parameter_0(self):
        """
        If the first parameter number is zero then all parameters with defaults
        should be returned.
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int",default=42),
                            parameter(1,type="string",default="42"),
                            parameter(2,type="float",default=42.0)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(0),{0:42,1:"42",2:42.0})

    def test_get_parameter_defaults_returns_from_second_param_for_first_parameter_1(self):
        """
        If the first parameter number is 1 only parameters from #1 onwards with
        defaults should be returned
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int",default=42),
                            parameter(1,type="string",default="42"),
                            parameter(2,type="float",default=42.0)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(1),{1:"42",2:42.0})

    def test_get_parameter_defaults_returns_from_third_param_for_first_parameter_2(self):
        """
        If the first parameter number is 1 only parameters from #1 onwards with
        defaults should be returned
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int",default=42),
                            parameter(1,type="string",default="42"),
                            parameter(2,type="float",default=42.0)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(2),{2:42.0})

    def test_get_parameter_defaults_returns_nothing_when_first_is_beyond_last_parameter(self):
        """
        If the first paramter to return for is beyond the last parameter in
        the syntax then the empty dict should be returned
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int",default=42),
                            parameter(1,type="string",default="42"),
                            parameter(2,type="float",default=42.0)]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(300),{})

    def test_get_parameter_defaults_sets_params_to_None_when_no_parameters_have_defaults(self):
        """
        If there are no parameters with defaults then get_parameter_defaults()
        should return a dict where all parameters are set to None
        """
        stx = Syntax(syntax_table([
            CreateSyntax(
                name="syntax_1",
                parameters=[parameter(0,type="int"),
                            parameter(1,type="string"),
                            parameter(2,type="float")]
            )
        ]))
        stx.switch_syntax("syntax_1")

        self.assertDictEqual(stx.get_parameter_defaults(0),{0:None,1:None,2:None})
