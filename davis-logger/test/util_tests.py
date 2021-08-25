import unittest
from davis_logger.util import Event
from test.util import CallTracker

__author__ = 'david'



class EventTests(unittest.TestCase):
    """
    Tests the basic event system used by the Davis module.
    """

    def setUp(self):
        self.event = Event()


    def test_has_no_handlers_on_init(self):
        self.assertListEqual(self.event._handlers, [])

    def test_add_operator_adds_a_handler(self):
        def _foo_handler():
            pass

        event = Event()
        event += _foo_handler

        self.assertListEqual(event._handlers, [_foo_handler])

    def test_subtract_operator_removes_a_handler(self):
        def _foo_handler():
            pass

        self.event += _foo_handler
        self.assertListEqual(self.event._handlers, [_foo_handler])

        self.event -= _foo_handler
        self.assertListEqual(self.event._handlers, [])

    def test_fire_calls_event_handler(self):
        """
        Checks that the event does actually call a handler when fired.
        """
        def _foo_handler():
            pass

        inspector = CallTracker(_foo_handler)

        self.event += inspector
        self.event.fire()

        self.assertTrue(inspector.called)

    def test_fire_calls_multiple_event_handlers(self):
        """
        Checks that the event does actually call more than one event handler
        when fired.
        """
        def _foo_handler():
            pass

        inspector1 = CallTracker(_foo_handler)
        inspector2 = CallTracker(_foo_handler)


        self.event += inspector1
        self.event += inspector2
        self.event.fire()

        self.assertTrue(inspector1.called)
        self.assertTrue(inspector2.called)

    def test_clear_handlers_clears_objects_handlers(self):
        class handler_owner(object):
            def handler_1(self):
                pass
            def handler_2(self):
                pass

            def add_handlers(self, event):
                event += self.handler_1
                event += self.handler_2
                return event, [self.handler_1, self.handler_2]

        instance = handler_owner()
        event, result_list = instance.add_handlers(self.event)
        self.assertListEqual(event._handlers, result_list)

        event.remove_handlers(instance)
        self.assertListEqual(event._handlers, [])

    def test_clear_handlers_only_clears_objects_handlers(self):
        class handler_owner(object):
            def handler_1(self):
                pass
            def handler_2(self):
                pass

            def add_handlers(self, event):
                event += self.handler_1
                event += self.handler_2
                return event, [self.handler_1, self.handler_2]

        def _foo_handler():
            pass

        instance = handler_owner()
        event,result = instance.add_handlers(self.event)
        event += _foo_handler
        result.append(_foo_handler)

        self.assertListEqual(event._handlers, result)

        event.remove_handlers(instance)
        self.assertListEqual(event._handlers, [_foo_handler])
